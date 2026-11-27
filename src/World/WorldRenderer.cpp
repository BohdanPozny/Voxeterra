#include "World/WorldRenderer.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Renderer.hpp"
#include "Camera.hpp"
#include "utils/Profiler.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <thread>

namespace {
constexpr float kLodFactor = 4.0f;  // a node splits while the camera is closer than this many node sizes
constexpr int kMaxLevel = 6;
constexpr uint32_t kQuadsPerDraw = 1u << 18;
constexpr uint32_t kArenaBlockVertices = 16u << 20;  // 64 MB per block
constexpr size_t kMaxUploadsPerFrame = 64;

struct PushConstants {
    glm::vec4 originScale;  // column origin in voxels, cell size
};

inline int floorDiv(int a, int b) {
    return a >= 0 ? a / b : (a - b + 1) / b;
}

// Distance in XZ from a point to the column's footprint (0 inside).
float rectDistance(const ColumnKey& key, glm::vec2 p) {
    const float size = static_cast<float>(key.sizeVoxels());
    const float x0 = static_cast<float>(key.x) * size;
    const float z0 = static_cast<float>(key.z) * size;
    const float dx = std::max({x0 - p.x, 0.0f, p.x - (x0 + size)});
    const float dz = std::max({z0 - p.y, 0.0f, p.y - (z0 + size)});
    return std::sqrt(dx * dx + dz * dz);
}
}

WorldRenderer::~WorldRenderer() {
    cleanup();
}

bool WorldRenderer::init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t frameCount) {
    m_device = &device;
    m_logicalDevice = device.getLogicalDevice();
    m_frameCount = frameCount;

    if (!createDescriptorSetLayout()) {
        std::cerr << "[WorldRenderer] Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // Voxel pipeline (solid): depth-write on, no blending
    if (!m_pipeline.initWithVertexInput(m_logicalDevice, renderPass, extent,
                                        "shaders/voxel.vert.spv",
                                        "shaders/voxel.frag.spv",
                                        m_descriptorSetLayout,
                                        /*enableAlphaBlending=*/false,
                                        /*enableDepthWrite=*/true,
                                        device.getPipelineCache())) {
        std::cerr << "[WorldRenderer] Failed to create voxel pipeline" << std::endl;
        return false;
    }

    // Water pipeline: depth-test on / depth-write off + alpha blending
    if (!m_waterPipeline.initWithVertexInput(m_logicalDevice, renderPass, extent,
                                             "shaders/voxel.vert.spv",
                                             "shaders/water.frag.spv",
                                             m_descriptorSetLayout,
                                             /*enableAlphaBlending=*/true,
                                             /*enableDepthWrite=*/false,
                                             device.getPipelineCache())) {
        std::cerr << "[WorldRenderer] Failed to create water pipeline" << std::endl;
        return false;
    }

    if (!createUniformBuffers()) return false;
    if (!createDescriptorPool()) return false;
    if (!createDescriptorSets()) return false;
    if (!createQuadIndices()) return false;
    if (!m_arena.init(device, kArenaBlockVertices)) return false;

    const unsigned cores = std::thread::hardware_concurrency();
    m_workers.start(cores > 1 ? cores - 1 : 1);

    std::cout << "[WorldRenderer] Initialized successfully" << std::endl;
    return true;
}

void WorldRenderer::cleanup() {
    if (m_logicalDevice == VK_NULL_HANDLE) return;

    m_workers.stop();
    m_results.clear();
    m_columns.clear();
    m_desired.clear();
    m_scheduled.clear();
    m_retired.clear();
    m_arena.cleanup();
    m_quadIndices.reset();
    m_uniformBuffers.clear();

    m_descriptorSets.clear();
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_logicalDevice, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_logicalDevice, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    // Pipeline resources tear down via RAII.
    m_logicalDevice = VK_NULL_HANDLE;
    m_device = nullptr;
}

bool WorldRenderer::createQuadIndices() {
    std::vector<uint32_t> indices(static_cast<size_t>(kQuadsPerDraw) * 6);
    for (uint32_t q = 0; q < kQuadsPerDraw; ++q) {
        const uint32_t b = q * 4;
        uint32_t* i = &indices[static_cast<size_t>(q) * 6];
        i[0] = b; i[1] = b + 1; i[2] = b + 2;
        i[3] = b; i[4] = b + 2; i[5] = b + 3;
    }
    m_quadIndices = std::make_unique<Buffer>();
    if (!m_quadIndices->init(*m_device, indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "[WorldRenderer] Failed to create quad index buffer" << std::endl;
        return false;
    }
    m_quadIndices->copyData(indices.data(), indices.size() * sizeof(uint32_t));
    return true;
}

float WorldRenderer::distanceTo(const ColumnKey& key) const {
    return rectDistance(key, m_cameraXZ);
}

void WorldRenderer::selectColumns(int renderDistance) {
    ZoneScoped;
    m_desired.clear();
    const float range = static_cast<float>(renderDistance * CHUNK_SIZE);
    const int rootSize = CHUNK_SIZE << kMaxLevel;

    // Quadtree descent: leaves are the columns to show, so levels never overlap.
    auto visit = [&](auto& self, const ColumnKey& key) -> void {
        const float d = distanceTo(key);
        if (d > range) return;
        if (key.level > 0 && d < kLodFactor * static_cast<float>(key.sizeVoxels())) {
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i)
                    self(self, ColumnKey{key.level - 1, key.x * 2 + i, key.z * 2 + j});
        } else {
            m_desired.emplace(key, 0u);
        }
    };

    const int x0 = floorDiv(static_cast<int>(std::floor(m_cameraXZ.x - range)), rootSize);
    const int x1 = floorDiv(static_cast<int>(std::floor(m_cameraXZ.x + range)), rootSize);
    const int z0 = floorDiv(static_cast<int>(std::floor(m_cameraXZ.y - range)), rootSize);
    const int z1 = floorDiv(static_cast<int>(std::floor(m_cameraXZ.y + range)), rootSize);
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x)
            visit(visit, ColumnKey{kMaxLevel, x, z});

    // A side is closed only when the neighbour is drawn at the same level.
    for (auto& [key, sides] : m_desired) {
        auto missing = [&](int dx, int dz) { return !m_desired.count(ColumnKey{key.level, key.x + dx, key.z + dz}); };
        sides = (missing(-1, 0) ? SIDE_NEG_X : 0u) | (missing(1, 0) ? SIDE_POS_X : 0u) |
                (missing(0, -1) ? SIDE_NEG_Z : 0u) | (missing(0, 1) ? SIDE_POS_Z : 0u);
    }
}

void WorldRenderer::scheduleBuilds() {
    ZoneScoped;
    // Jobs that never started are re-queued below with fresh priorities.
    for (const auto& job : m_workers.clearQueue()) {
        auto it = m_scheduled.find(job.key);
        if (it != m_scheduled.end() && it->second == job.openSides) m_scheduled.erase(it);
    }

    std::vector<ColumnWorkers::Job> jobs;
    for (const auto& [key, sides] : m_desired) {
        auto col = m_columns.find(key);
        if (col != m_columns.end() && col->second.openSides == sides) continue;
        auto running = m_scheduled.find(key);
        if (running != m_scheduled.end() && running->second == sides) continue;
        m_scheduled[key] = sides;
        jobs.push_back({key, sides, distanceTo(key)});
    }
    m_workers.enqueue(jobs);
}

void WorldRenderer::retire(const MeshArena::Allocation& alloc) {
    if (alloc.valid()) m_retired.emplace_back(m_frameNumber, alloc);
}

void WorldRenderer::upload(ColumnWorkers::Result& result) {
    ZoneScoped;
    const auto& mesh = result.mesh;
    const int cell = result.key.cellSize();

    Column col;
    col.openSides = result.openSides;
    col.solidQuads = static_cast<uint32_t>(mesh.solid.size() / 4);
    col.waterQuads = static_cast<uint32_t>(mesh.water.size() / 4);
    col.minY = mesh.minY * cell;
    col.maxY = mesh.maxY * cell;

    const auto total = static_cast<uint32_t>(mesh.solid.size() + mesh.water.size());
    if (total > 0) {
        col.alloc = m_arena.allocate(total);
        if (col.alloc.valid()) {
            uint32_t* dst = m_arena.data(col.alloc);
            std::copy(mesh.solid.begin(), mesh.solid.end(), dst);
            std::copy(mesh.water.begin(), mesh.water.end(), dst + mesh.solid.size());
        } else {
            std::cerr << "[WorldRenderer] Out of mesh memory" << std::endl;
            col.solidQuads = col.waterQuads = 0;
        }
    }

    auto [it, inserted] = m_columns.try_emplace(result.key, col);
    if (!inserted) {
        retire(it->second.alloc);
        it->second = col;
    }
}

bool WorldRenderer::uploadResults() {
    ZoneScoped;
    m_workers.collect(m_results);

    size_t uploads = 0;
    auto it = m_results.begin();
    for (; it != m_results.end() && uploads < kMaxUploadsPerFrame; ++it) {
        auto scheduled = m_scheduled.find(it->key);
        if (scheduled != m_scheduled.end() && scheduled->second == it->openSides) m_scheduled.erase(scheduled);

        // Superseded while it was being built.
        auto wanted = m_desired.find(it->key);
        if (wanted == m_desired.end() || wanted->second != it->openSides) continue;

        upload(*it);
        ++uploads;
    }
    m_results.erase(m_results.begin(), it);
    return uploads > 0;
}

bool WorldRenderer::isCovered(const ColumnKey& stale) const {
    // Coarser replacement: the desired ancestor must be loaded.
    for (ColumnKey k = stale; k.level < kMaxLevel; ) {
        k = ColumnKey{k.level + 1, floorDiv(k.x, 2), floorDiv(k.z, 2)};
        if (m_desired.count(k)) return m_columns.count(k) > 0;
    }

    // Finer replacements: every desired column inside the footprint must be loaded.
    const int size = stale.sizeVoxels();
    const int sx = stale.x * size, sz = stale.z * size;
    for (const auto& [key, sides] : m_desired) {
        if (key.level >= stale.level) continue;
        const int ks = key.sizeVoxels();
        const int kx = key.x * ks, kz = key.z * ks;
        if (kx < sx || kx >= sx + size || kz < sz || kz >= sz + size) continue;
        if (!m_columns.count(key)) return false;
    }
    return true;
}

void WorldRenderer::dropCoveredColumns() {
    ZoneScoped;
    for (auto it = m_columns.begin(); it != m_columns.end(); ) {
        if (!m_desired.count(it->first) && isCovered(it->first)) {
            retire(it->second.alloc);
            it = m_columns.erase(it);
        } else {
            ++it;
        }
    }
}

void WorldRenderer::updateStreaming(const glm::vec3& cameraPos, int renderDistance) {
    ZoneScoped;
    const glm::vec3 voxelPos = cameraPos / VOXEL_SIZE;
    m_cameraXZ = glm::vec2(voxelPos.x, voxelPos.z);

    bool changed = false;
    const glm::ivec3 chunk = worldToChunkCoord(voxelPos);
    if (glm::ivec2(chunk.x, chunk.z) != m_lastCenter || renderDistance != m_lastDistance) {
        m_lastCenter = glm::ivec2(chunk.x, chunk.z);
        m_lastDistance = renderDistance;
        selectColumns(renderDistance);
        scheduleBuilds();
        changed = true;
    }

    if (uploadResults() || changed) {
        dropCoveredColumns();
        recomputeStats();
    }

    TracyPlot("Loaded columns", static_cast<int64_t>(m_columns.size()));
    TracyPlot("Pending columns", static_cast<int64_t>(m_scheduled.size()));
    TracyPlot("Mesh memory (MB)", static_cast<double>(m_arena.usedBytes()) / (1024.0 * 1024.0));
}

void WorldRenderer::recomputeStats() {
    m_totalQuads = 0;
    for (const auto& kv : m_columns) m_totalQuads += kv.second.solidQuads + kv.second.waterQuads;
}

void WorldRenderer::extractFrustumPlanes(const glm::mat4& vp) {
    // glm is column-major: row i = (vp[0][i], vp[1][i], vp[2][i], vp[3][i]).
    auto setPlane = [&](int idx, float nx, float ny, float nz, float d) {
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        m_frustumPlanes[idx].normal = glm::vec3(nx / len, ny / len, nz / len);
        m_frustumPlanes[idx].d = d / len;
    };
    // Left:   row3 + row0
    setPlane(0, vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right:  row3 - row0
    setPlane(1, vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom: row3 + row1
    setPlane(2, vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top:    row3 - row1
    setPlane(3, vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near (Vulkan z 0..1): row2
    setPlane(4, vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    // Far:    row3 - row2
    setPlane(5, vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
}

void WorldRenderer::updateUniforms(uint32_t frameIndex, const Camera& camera) {
    ZoneScoped;
    UniformBufferObject ubo{};
    ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(VOXEL_SIZE));
    ubo.view  = camera.getViewMatrix();
    ubo.proj  = camera.getProjectionMatrix();
    ubo.proj[1][1] *= -1;  // Vulkan Y-flip

    // Un-flipped MVP puts the planes in voxel space, matching the column AABBs.
    extractFrustumPlanes(camera.getProjectionMatrix() * ubo.view * ubo.model);

    m_uniformBuffers[frameIndex]->copyData(&ubo, sizeof(ubo));
}

// AABB vs. frustum (p-vertex) test.
static bool aabbInFrustum(const WorldRenderer::Plane planes[6],
                          const glm::vec3& mn, const glm::vec3& mx) {
    for (int i = 0; i < 6; ++i) {
        const auto& p = planes[i];
        glm::vec3 pv(
            p.normal.x >= 0.0f ? mx.x : mn.x,
            p.normal.y >= 0.0f ? mx.y : mn.y,
            p.normal.z >= 0.0f ? mx.z : mn.z);
        if (glm::dot(p.normal, pv) + p.d < 0.0f) return false;
    }
    return true;
}

void WorldRenderer::drawPass(VkCommandBuffer cmd, const Pipeline& pipeline, uint32_t frameIndex, bool water) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1,
                            &m_descriptorSets[frameIndex], 0, nullptr);
    vkCmdBindIndexBuffer(cmd, m_quadIndices->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    uint32_t boundBlock = UINT32_MAX;
    for (const auto& [key, col] : m_columns) {
        uint32_t quads = water ? col.waterQuads : col.solidQuads;
        if (quads == 0) continue;

        const float size = static_cast<float>(key.sizeVoxels());
        const glm::vec3 mn(static_cast<float>(key.x) * size, static_cast<float>(col.minY), static_cast<float>(key.z) * size);
        const glm::vec3 mx(mn.x + size, static_cast<float>(col.maxY), mn.z + size);
        if (!aabbInFrustum(m_frustumPlanes, mn, mx)) continue;
        if (!water) ++m_visibleColumns;

        if (col.alloc.block != boundBlock) {
            boundBlock = col.alloc.block;
            const VkBuffer vb = m_arena.buffer(boundBlock);
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        }

        const PushConstants pc{glm::vec4(mn.x, 0.0f, mn.z, static_cast<float>(key.cellSize()))};
        vkCmdPushConstants(cmd, pipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        // The shared index pattern covers kQuadsPerDraw quads; larger meshes take several draws.
        uint32_t firstVertex = col.alloc.offset + (water ? col.solidQuads * 4 : 0);
        while (quads > 0) {
            const uint32_t n = std::min(quads, kQuadsPerDraw);
            vkCmdDrawIndexed(cmd, n * 6, 1, 0, static_cast<int32_t>(firstVertex), 0);
            firstVertex += n * 4;
            quads -= n;
        }
    }
}

void WorldRenderer::render(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    ZoneScoped;

    // Called after the frame fence wait, so frames older than MAX_FRAMES_IN_FLIGHT are done.
    std::erase_if(m_retired, [&](const auto& entry) {
        if (m_frameNumber < entry.first + Renderer::MAX_FRAMES_IN_FLIGHT) return false;
        m_arena.free(entry.second);
        return true;
    });
    ++m_frameNumber;

    m_visibleColumns = 0;
    drawPass(commandBuffer, m_pipeline, frameIndex, false);
    drawPass(commandBuffer, m_waterPipeline, frameIndex, true);

    TracyPlot("Visible columns", static_cast<int64_t>(m_visibleColumns));
}

bool WorldRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    return vkCreateDescriptorSetLayout(m_logicalDevice, &layoutInfo, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

bool WorldRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    m_uniformBuffers.resize(m_frameCount);

    for (uint32_t i = 0; i < m_frameCount; ++i) {
        auto ub = std::make_unique<Buffer>();
        if (!ub->init(*m_device, bufferSize,
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            std::cerr << "[WorldRenderer] Failed to create uniform buffer " << i << std::endl;
            return false;
        }
        m_uniformBuffers[i] = std::move(ub);
    }
    return true;
}

bool WorldRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = m_frameCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = m_frameCount;

    return vkCreateDescriptorPool(m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

bool WorldRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(m_frameCount, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = m_frameCount;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(m_frameCount);
    if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[WorldRenderer] Failed to allocate descriptor sets" << std::endl;
        return false;
    }

    for (uint32_t i = 0; i < m_frameCount; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_logicalDevice, 1, &write, 0, nullptr);
    }
    return true;
}

#include "World/WorldRenderer.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Renderer.hpp"
#include "Camera.hpp"
#include "World/Chunk.hpp"
#include "utils/Profiler.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
constexpr int kWorldYChunks = 4;  // world Y pinned to [0, kWorldYChunks)

const glm::ivec3 kNeighborOffsets[6] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1},
};
}

WorldRenderer::~WorldRenderer() {
    cleanup();
}

bool WorldRenderer::init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t frameCount) {
    m_device = &device;
    m_logicalDevice = device.getLogicalDevice();
    m_frameCount = frameCount;

    // 1. Descriptor set layout (UBO binding)
    if (!createDescriptorSetLayout()) {
        std::cerr << "[WorldRenderer] Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // 2. Voxel pipeline (solid): depth-write on, no blending
    if (!m_pipeline.initWithVertexInput(m_logicalDevice, renderPass, extent,
                                        "shaders/voxel.vert.spv",
                                        "shaders/voxel.frag.spv",
                                        m_descriptorSetLayout,
                                        /*enableAlphaBlending=*/false,
                                        /*enableDepthWrite=*/true)) {
        std::cerr << "[WorldRenderer] Failed to create voxel pipeline" << std::endl;
        return false;
    }

    // 2b. Water pipeline: depth-test on / depth-write off + alpha blending
    if (!m_waterPipeline.initWithVertexInput(m_logicalDevice, renderPass, extent,
                                             "shaders/voxel.vert.spv",
                                             "shaders/water.frag.spv",
                                             m_descriptorSetLayout,
                                             /*enableAlphaBlending=*/true,
                                             /*enableDepthWrite=*/false)) {
        std::cerr << "[WorldRenderer] Failed to create water pipeline" << std::endl;
        return false;
    }

    // 3. Uniform buffers
    if (!createUniformBuffers()) return false;

    // 4. Descriptor pool + sets
    if (!createDescriptorPool()) return false;
    if (!createDescriptorSets()) return false;

    std::cout << "[WorldRenderer] Initialized successfully" << std::endl;
    return true;
}

void WorldRenderer::cleanup() {
    if (m_logicalDevice == VK_NULL_HANDLE) return;

    // Voxel buffers
    m_chunkBuffers.clear();
    m_pendingMesh.clear();
    m_retiredBuffers.clear();
    m_world.reset();

    // Uniform buffers
    m_uniformBuffers.clear();

    // Descriptors
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

void WorldRenderer::generateWorld() {
    m_world = std::make_unique<World>();
    // Chunks are generated lazily by updateStreaming() as the camera moves.
}

void WorldRenderer::retireBuffer(std::unique_ptr<Buffer> buffer) {
    if (buffer) m_retiredBuffers.emplace_back(m_frameNumber, std::move(buffer));
}

void WorldRenderer::uploadChunk(const glm::ivec3& cpos) {
    ZoneScoped;
    Chunk* chunk = m_world->getChunk(cpos);
    if (!chunk) return;
    chunk->setDirty(true);
    chunk->generateMesh(m_world.get());

    const auto& vertices  = chunk->getVertices();
    const auto& indices   = chunk->getIndices();
    const auto& wVertices = chunk->getWaterVertices();
    const auto& wIndices  = chunk->getWaterIndices();

    const VkDeviceSize vSize  = vertices.size()  * sizeof(VoxelVertex);
    const VkDeviceSize iSize  = indices.size()   * sizeof(uint32_t);
    const VkDeviceSize wvSize = wVertices.size() * sizeof(VoxelVertex);
    const VkDeviceSize wiSize = wIndices.size()  * sizeof(uint32_t);
    const VkDeviceSize total  = vSize + iSize + wvSize + wiSize;

    ChunkBuffers cb;
    if (total > 0) {
        ZoneScopedN("Upload chunk buffer");
        auto buffer = std::make_unique<Buffer>();
        if (buffer->init(*m_device, total,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            cb.indexOffset       = vSize;
            cb.waterVertexOffset = vSize + iSize;
            cb.waterIndexOffset  = vSize + iSize + wvSize;
            if (vSize)  buffer->copyData(vertices.data(),  vSize);
            if (iSize)  buffer->copyData(indices.data(),   iSize,  cb.indexOffset);
            if (wvSize) buffer->copyData(wVertices.data(), wvSize, cb.waterVertexOffset);
            if (wiSize) buffer->copyData(wIndices.data(),  wiSize, cb.waterIndexOffset);
            cb.indexCount      = static_cast<uint32_t>(indices.size());
            cb.waterIndexCount = static_cast<uint32_t>(wIndices.size());
            cb.vertexCount     = vertices.size();
            cb.buffer = std::move(buffer);
        }
    }
    chunk->clearMesh();

    auto it = m_chunkBuffers.find(cpos);
    if (it != m_chunkBuffers.end()) {
        retireBuffer(std::move(it->second.buffer));
        it->second = std::move(cb);
    } else {
        m_chunkBuffers.emplace(cpos, std::move(cb));
    }
}


void WorldRenderer::rebuildLoadQueue(const glm::ivec3& center, int loadRadius) {
    ZoneScoped;
    const int loadSq = loadRadius * loadRadius;

    m_lastStreamCenter = center;
    m_loadRadius = loadRadius;
    m_streamingInProgress = true;
    m_loadQueue.clear();
    m_loadQueueCursor = 0;
    m_loadQueue.reserve(static_cast<size_t>(loadSq) * 4 * kWorldYChunks);

    // Disc of (dx, dz) offsets, all Y levels per column.
    for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
        for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
            if (dx * dx + dz * dz > loadSq) continue;       // circular disc
            for (int cy = 0; cy < kWorldYChunks; ++cy) {
                m_loadQueue.emplace_back(center.x + dx, cy, center.z + dz);
            }
        }
    }

    // Nearest-first so the player sees the closest chunks materialise first.
    std::sort(m_loadQueue.begin(), m_loadQueue.end(),
              [&](const glm::ivec3& a, const glm::ivec3& b) {
                  int da = (a.x - center.x) * (a.x - center.x)
                         + (a.z - center.z) * (a.z - center.z);
                  int db = (b.x - center.x) * (b.x - center.x)
                         + (b.z - center.z) * (b.z - center.z);
                  if (da != db) return da < db;
                  return a.y < b.y;                          // lower chunks first
              });
}

void WorldRenderer::unloadFarChunks(const glm::ivec3& center, int unloadSq) {
    ZoneScoped;
    for (auto it = m_chunkBuffers.begin(); it != m_chunkBuffers.end(); ) {
        const glm::ivec3& p = it->first;
        int dx = p.x - center.x;
        int dz = p.z - center.z;
        if (dx * dx + dz * dz > unloadSq) {
            m_world->removeChunk(p);
            m_pendingMesh.erase(p);
            retireBuffer(std::move(it->second.buffer));
            it = m_chunkBuffers.erase(it);
        } else {
            ++it;
        }
    }
}

bool WorldRenderer::isExpectedChunk(const glm::ivec3& cpos) const {
    if (cpos.y < 0 || cpos.y >= kWorldYChunks) return false;
    const int dx = cpos.x - m_lastStreamCenter.x;
    const int dz = cpos.z - m_lastStreamCenter.z;
    return dx * dx + dz * dz <= m_loadRadius * m_loadRadius;
}

bool WorldRenderer::isReadyToMesh(const glm::ivec3& cpos) const {
    for (const auto& off : kNeighborOffsets) {
        const glm::ivec3 np = cpos + off;
        if (isExpectedChunk(np) && !m_chunkBuffers.count(np)) return false;
    }
    return true;
}

void WorldRenderer::streamNextBatch() {
    ZoneScoped;
    // Cap per-frame work so the main thread stays responsive.
    constexpr int kMaxNewPerFrame = 8;
    constexpr size_t kMaxMeshesPerFrame = 16;
    int newlyCreated = 0;

    while (m_loadQueueCursor < m_loadQueue.size() && newlyCreated < kMaxNewPerFrame) {
        const glm::ivec3 cpos = m_loadQueue[m_loadQueueCursor++];
        if (m_chunkBuffers.count(cpos)) continue;            // already loaded

        if (!m_world->getChunk(cpos)) {
            ZoneScopedN("Generate chunk");
            m_world->generateChunkData(m_world->createChunk(cpos));
        }
        m_chunkBuffers.emplace(cpos, ChunkBuffers{});
        ++newlyCreated;

        // Loaded neighbours treated this chunk as air on their shared border.
        m_pendingMesh.insert(cpos);
        for (const auto& off : kNeighborOffsets) {
            const glm::ivec3 np = cpos + off;
            if (m_chunkBuffers.count(np)) m_pendingMesh.insert(np);
        }
    }

    // Mesh each chunk once all of its in-range neighbours exist, nearest first.
    std::vector<glm::ivec3> ready;
    for (const auto& cpos : m_pendingMesh) {
        if (isReadyToMesh(cpos)) ready.push_back(cpos);
    }
    const size_t meshCount = std::min(ready.size(), kMaxMeshesPerFrame);
    auto distSq = [&](const glm::ivec3& p) {
        const int dx = p.x - m_lastStreamCenter.x;
        const int dz = p.z - m_lastStreamCenter.z;
        return dx * dx + dz * dz;
    };
    std::partial_sort(ready.begin(), ready.begin() + meshCount, ready.end(),
                      [&](const glm::ivec3& a, const glm::ivec3& b) { return distSq(a) < distSq(b); });
    for (size_t i = 0; i < meshCount; ++i) {
        uploadChunk(ready[i]);
        m_pendingMesh.erase(ready[i]);
    }

    // Mark the streaming pass complete once the queue is drained and every chunk is meshed.
    if (m_loadQueueCursor >= m_loadQueue.size() && m_pendingMesh.empty()) {
        m_streamingInProgress = false;
        m_loadQueue.clear();
        m_loadQueue.shrink_to_fit();
        m_loadQueueCursor = 0;
    }
}

void WorldRenderer::updateStreaming(const glm::vec3& cameraPos, int renderDistance) {
    ZoneScoped;
    if (!m_world) return;

    // Streaming is XZ-only; drop the Y component from the camera's chunk coord.
    glm::ivec3 center = Chunk::worldToChunkCoord(cameraPos);
    center.y = 0;
    const int loadRadius   = renderDistance;
    const int unloadRadius = renderDistance + 1;

    // Rebuild the nearest-first load queue whenever the centre moves.
    if (center != m_lastStreamCenter) {
        rebuildLoadQueue(center, loadRadius);
    } else if (!m_streamingInProgress) {
        // Fully streamed around this centre; nothing to do.
        return;
    }

    unloadFarChunks(center, unloadRadius * unloadRadius);
    streamNextBatch();
    recomputeStats();

    TracyPlot("Loaded chunks", static_cast<int64_t>(m_chunkBuffers.size()));
    TracyPlot("Load queue remaining", static_cast<int64_t>(m_loadQueue.size() - m_loadQueueCursor));
    TracyPlot("Pending meshes", static_cast<int64_t>(m_pendingMesh.size()));
}

void WorldRenderer::recomputeStats() {
    m_totalVertices = 0;
    m_totalTriangles = 0;
    for (const auto& kv : m_chunkBuffers) {
        m_totalTriangles += kv.second.indexCount / 3;
        m_totalVertices  += kv.second.vertexCount;
    }
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
    ubo.model = glm::mat4(1.0f);
    ubo.view  = camera.getViewMatrix();
    ubo.proj  = camera.getProjectionMatrix();
    ubo.proj[1][1] *= -1;  // Vulkan Y-flip

    // Frustum is extracted from the un-flipped VP so planes live in world space
    // (the Y-flip only matters in clip space).
    extractFrustumPlanes(camera.getProjectionMatrix() * camera.getViewMatrix());

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

void WorldRenderer::render(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    ZoneScoped;
    if (!m_world) return;

    // Called after the frame fence wait, so frames older than MAX_FRAMES_IN_FLIGHT are done.
    std::erase_if(m_retiredBuffers, [&](const auto& entry) {
        return m_frameNumber >= entry.first + Renderer::MAX_FRAMES_IN_FLIGHT;
    });
    ++m_frameNumber;

    m_visibleChunks = 0;
    const float cs = static_cast<float>(CHUNK_SIZE);

    // Pass 1: opaque chunks, depth-write enabled.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline.getLayout(), 0, 1,
                            &m_descriptorSets[frameIndex], 0, nullptr);

    for (const auto& kv : m_chunkBuffers) {
        const auto& cb = kv.second;
        if (!cb.buffer || cb.indexCount == 0) continue;

        const glm::ivec3& cp = kv.first;
        glm::vec3 mn(cp.x * cs, cp.y * cs, cp.z * cs);
        glm::vec3 mx = mn + glm::vec3(cs);
        if (!aabbInFrustum(m_frustumPlanes, mn, mx)) continue;

        VkBuffer vertexBuffers[] = {cb.buffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, cb.buffer->getBuffer(), cb.indexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, cb.indexCount, 1, 0, 0, 0);
        ++m_visibleChunks;
    }

    // Pass 2: water mesh, alpha blending, depth-write disabled.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_waterPipeline.getPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_waterPipeline.getLayout(), 0, 1,
                            &m_descriptorSets[frameIndex], 0, nullptr);

    for (const auto& kv : m_chunkBuffers) {
        const auto& cb = kv.second;
        if (!cb.buffer || cb.waterIndexCount == 0) continue;

        const glm::ivec3& cp = kv.first;
        glm::vec3 mn(cp.x * cs, cp.y * cs, cp.z * cs);
        glm::vec3 mx = mn + glm::vec3(cs);
        if (!aabbInFrustum(m_frustumPlanes, mn, mx)) continue;

        VkBuffer vertexBuffers[] = {cb.buffer->getBuffer()};
        VkDeviceSize offsets[] = {cb.waterVertexOffset};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, cb.buffer->getBuffer(), cb.waterIndexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, cb.waterIndexCount, 1, 0, 0, 0);
    }

    TracyPlot("Visible chunks", static_cast<int64_t>(m_visibleChunks));
    TracyPlot("Retired buffers", static_cast<int64_t>(m_retiredBuffers.size()));
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

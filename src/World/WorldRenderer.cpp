#include "World/WorldRenderer.hpp"
#include "Vulkan/Device.hpp"
#include "Camera.hpp"
#include "World/Chunk.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>

WorldRenderer::~WorldRenderer() {
    cleanup();
}

bool WorldRenderer::init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t imageCount) {
    m_device = &device;
    m_logicalDevice = device.getLogicalDevice();
    m_imageCount = imageCount;

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

void WorldRenderer::updateStreaming(const glm::vec3& cameraPos, int renderDistance) {
    if (!m_world) return;

    // Streaming is XZ-only; world Y is pinned to 0 for this build.
    auto floorDiv = [](int a, int b) -> int {
        return a >= 0 ? a / b : (a - b + 1) / b;
    };
    int ccx = floorDiv(static_cast<int>(std::floor(cameraPos.x)), CHUNK_SIZE);
    int ccz = floorDiv(static_cast<int>(std::floor(cameraPos.z)), CHUNK_SIZE);
    glm::ivec3 center(ccx, 0, ccz);

    const int loadRadius   = renderDistance;
    const int unloadRadius = renderDistance + 1;
    const int loadSq       = loadRadius * loadRadius;
    const int unloadSq     = unloadRadius * unloadRadius;

    // Vertical range is fixed (world Y pinned to [0, WORLD_Y_CHUNKS)).
    constexpr int WORLD_Y_CHUNKS = 4;

    // Rebuild the nearest-first load queue whenever the centre moves.
    if (center != m_lastStreamCenter) {
        m_lastStreamCenter = center;
        m_streamingInProgress = true;
        m_loadQueue.clear();
        m_loadQueueCursor = 0;
        m_loadQueue.reserve(static_cast<size_t>(loadSq) * 4 * WORLD_Y_CHUNKS);

        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
                int dsq = dx * dx + dz * dz;
                if (dsq > loadSq) continue;                    // circular disc
                for (int cy = 0; cy < WORLD_Y_CHUNKS; ++cy) {
                    m_loadQueue.emplace_back(center.x + dx, cy, center.z + dz);
                }
            }
        }
        std::sort(m_loadQueue.begin(), m_loadQueue.end(),
                  [&](const glm::ivec3& a, const glm::ivec3& b) {
                      int da = (a.x - center.x) * (a.x - center.x)
                             + (a.z - center.z) * (a.z - center.z);
                      int db = (b.x - center.x) * (b.x - center.x)
                             + (b.z - center.z) * (b.z - center.z);
                      if (da != db) return da < db;
                      return a.y < b.y;                        // lower chunks first
                  });
    } else if (!m_streamingInProgress) {
        // Fully streamed around this centre; nothing to do.
        return;
    }

    // 1) Unload chunks outside the circular hysteresis radius.
    for (auto it = m_chunkBuffers.begin(); it != m_chunkBuffers.end(); ) {
        const glm::ivec3& p = it->first;
        int dx = p.x - center.x;
        int dz = p.z - center.z;
        if (dx * dx + dz * dz > unloadSq) {
            m_world->removeChunk(p);
            it = m_chunkBuffers.erase(it);
        } else {
            ++it;
        }
    }

    // Helper: rebuild the mesh and upload GPU buffers for a chunk.
    auto uploadChunk = [&](const glm::ivec3& cpos) {
        Chunk* chunk = m_world->getChunk(cpos);
        if (!chunk) return;
        chunk->setDirty(true);
        chunk->generateMesh(m_world.get());

        const auto& vertices  = chunk->getVertices();
        const auto& indices   = chunk->getIndices();
        const auto& wVertices = chunk->getWaterVertices();
        const auto& wIndices  = chunk->getWaterIndices();

        ChunkBuffers cb;
        if (!vertices.empty() && !indices.empty()) {
            cb.indexCount = static_cast<uint32_t>(indices.size());
            cb.vertex = std::make_unique<Buffer>();
            if (cb.vertex->init(*m_device,
                                vertices.size() * sizeof(VoxelVertex),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                cb.vertex->copyData(vertices.data(), vertices.size() * sizeof(VoxelVertex));
            }
            cb.index = std::make_unique<Buffer>();
            if (cb.index->init(*m_device,
                               indices.size() * sizeof(uint32_t),
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                cb.index->copyData(indices.data(), indices.size() * sizeof(uint32_t));
            }
        }
        if (!wVertices.empty() && !wIndices.empty()) {
            cb.waterIndexCount = static_cast<uint32_t>(wIndices.size());
            cb.waterVertex = std::make_unique<Buffer>();
            if (cb.waterVertex->init(*m_device,
                                     wVertices.size() * sizeof(VoxelVertex),
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                cb.waterVertex->copyData(wVertices.data(), wVertices.size() * sizeof(VoxelVertex));
            }
            cb.waterIndex = std::make_unique<Buffer>();
            if (cb.waterIndex->init(*m_device,
                                    wIndices.size() * sizeof(uint32_t),
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                cb.waterIndex->copyData(wIndices.data(), wIndices.size() * sizeof(uint32_t));
            }
        }
        m_chunkBuffers[cpos] = std::move(cb);
    };

    // 2) Pop a small batch of the nearest un-loaded chunks from the queue.
    // The queue is pre-sorted (nearest first) so the player sees the closest
    // chunks materialise first without blocking the main thread.
    constexpr int kMaxNewPerFrame = 8;
    int newlyCreated = 0;

    static const glm::ivec3 neighborOffsets[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1},
    };

    while (m_loadQueueCursor < m_loadQueue.size() && newlyCreated < kMaxNewPerFrame) {
        const glm::ivec3 cpos = m_loadQueue[m_loadQueueCursor++];
        if (m_chunkBuffers.count(cpos)) continue;  // already loaded (e.g. neighbour upload)

        Chunk* chunk = m_world->getChunk(cpos);
        if (!chunk) {
            chunk = m_world->createChunk(cpos);
            m_world->generateChunkData(chunk);
        }

        uploadChunk(cpos);
        ++newlyCreated;

        // Re-mesh already-loaded neighbours so shared borders stop treating
        // this chunk as empty air (prevents water/stone seams).
        for (const auto& off : neighborOffsets) {
            const glm::ivec3 np = cpos + off;
            if (m_chunkBuffers.count(np)) uploadChunk(np);
        }
    }

    if (m_loadQueueCursor >= m_loadQueue.size()) {
        m_streamingInProgress = false;
        m_loadQueue.clear();
        m_loadQueue.shrink_to_fit();
        m_loadQueueCursor = 0;
    }

    // 3) Recompute statistics.
    m_totalVertices = 0;
    m_totalTriangles = 0;
    for (const auto& kv : m_chunkBuffers) {
        const auto& cb = kv.second;
        if (cb.indexCount == 0) continue;
        m_totalTriangles += cb.indexCount / 3;
        // Vertex count is tracked on the Chunk object, not on the GPU buffers.
        const glm::ivec3& p = kv.first;
        if (Chunk* c = m_world->getChunk(p)) {
            m_totalVertices += c->getVertices().size();
        }
    }
}

void WorldRenderer::updateUniforms(uint32_t imageIndex, const Camera& camera) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.proj[1][1] *= -1;  // Vulkan Y-flip

    // Gribb-Hartmann frustum extraction from the un-flipped projection so the
    // planes live in world space (the Y-flip only matters in clip space).
    glm::mat4 vp = camera.getProjectionMatrix() * camera.getViewMatrix();
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
    // Far:   row3 - row2
    setPlane(5, vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    void* data;
    vkMapMemory(m_logicalDevice, m_uniformBuffers[imageIndex]->getMemory(), 0, sizeof(ubo), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(m_logicalDevice, m_uniformBuffers[imageIndex]->getMemory());
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

void WorldRenderer::render(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    if (!m_world) return;

    m_visibleChunks = 0;
    const float cs = static_cast<float>(CHUNK_SIZE);

    // Pass 1: opaque chunks, depth-write enabled.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline.getLayout(), 0, 1,
                            &m_descriptorSets[imageIndex], 0, nullptr);

    for (const auto& kv : m_chunkBuffers) {
        const auto& cb = kv.second;
        if (!cb.vertex || !cb.index || cb.indexCount == 0) continue;

        const glm::ivec3& cp = kv.first;
        glm::vec3 mn(cp.x * cs, cp.y * cs, cp.z * cs);
        glm::vec3 mx = mn + glm::vec3(cs);
        if (!aabbInFrustum(m_frustumPlanes, mn, mx)) continue;

        VkBuffer vertexBuffers[] = {cb.vertex->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, cb.index->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, cb.indexCount, 1, 0, 0, 0);
        ++m_visibleChunks;
    }

    // Pass 2: water mesh, alpha blending, depth-write disabled.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_waterPipeline.getPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_waterPipeline.getLayout(), 0, 1,
                            &m_descriptorSets[imageIndex], 0, nullptr);

    for (const auto& kv : m_chunkBuffers) {
        const auto& cb = kv.second;
        if (!cb.waterVertex || !cb.waterIndex || cb.waterIndexCount == 0) continue;

        const glm::ivec3& cp = kv.first;
        glm::vec3 mn(cp.x * cs, cp.y * cs, cp.z * cs);
        glm::vec3 mx = mn + glm::vec3(cs);
        if (!aabbInFrustum(m_frustumPlanes, mn, mx)) continue;

        VkBuffer vertexBuffers[] = {cb.waterVertex->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, cb.waterIndex->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, cb.waterIndexCount, 1, 0, 0, 0);
    }
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
    m_uniformBuffers.resize(m_imageCount);

    for (uint32_t i = 0; i < m_imageCount; ++i) {
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
    poolSize.descriptorCount = m_imageCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = m_imageCount;

    return vkCreateDescriptorPool(m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

bool WorldRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(m_imageCount, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = m_imageCount;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(m_imageCount);
    if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "[WorldRenderer] Failed to allocate descriptor sets" << std::endl;
        return false;
    }

    for (uint32_t i = 0; i < m_imageCount; ++i) {
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

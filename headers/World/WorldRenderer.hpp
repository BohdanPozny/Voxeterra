#pragma once

#include "Vulkan/Pipeline.hpp"
#include "Vulkan/Buffer.hpp"
#include "World/World.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <unordered_map>
#include <unordered_set>

class Device;
class Camera;

// Encapsulates everything required to render the voxel world:
//   - solid + water graphics pipelines
//   - world + streaming chunk buffers
//   - uniform buffers and descriptor sets per frame-in-flight
class WorldRenderer {
public:
    // MVP uniform block uploaded per frame.
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    // Plane in world space: n.p + d >= 0 means "inside".
    struct Plane { glm::vec3 normal; float d; };

    WorldRenderer() = default;
    ~WorldRenderer();

    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    // Initialise pipelines, uniform buffers and descriptor resources.
    bool init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t frameCount);
    void cleanup();

    // Prepare an empty world; streaming will populate chunks on demand.
    void generateWorld();

    // Load chunks within renderDistance, unload chunks beyond (renderDistance + 1).
    // Expected to be called every frame from PlayingState.
    void updateStreaming(const glm::vec3& cameraPos, int renderDistance);

    // Upload MVP matrices + rebuild frustum planes for the given frame-in-flight.
    void updateUniforms(uint32_t frameIndex, const Camera& camera);

    // Record draw calls for the visible chunks.
    void render(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    World* getWorld() const { return m_world.get(); }

    // Statistics sampled from the most recent render pass.
    size_t getTotalVertices() const { return m_totalVertices; }
    size_t getTotalTriangles() const { return m_totalTriangles; }
    size_t getChunkCount() const { return m_chunkBuffers.size(); }
    size_t getVisibleChunks() const { return m_visibleChunks; }

private:
    Device* m_device = nullptr;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;

    Pipeline m_pipeline;        // opaque pass: depth-write on
    Pipeline m_waterPipeline;   // transparent pass: depth-test only + alpha blending

    std::unique_ptr<World> m_world;

    // One allocation per chunk: [solid verts | solid indices | water verts | water indices].
    struct ChunkBuffers {
        std::unique_ptr<Buffer> buffer;
        VkDeviceSize indexOffset = 0;
        VkDeviceSize waterVertexOffset = 0;
        VkDeviceSize waterIndexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t waterIndexCount = 0;
        size_t vertexCount = 0;
    };
    std::unordered_map<glm::ivec3, ChunkBuffers, IVec3Hash> m_chunkBuffers;

    // Buffers replaced or unloaded while earlier frames may still read them.
    std::vector<std::pair<uint64_t, std::unique_ptr<Buffer>>> m_retiredBuffers;
    uint64_t m_frameNumber = 0;
    void retireBuffer(std::unique_ptr<Buffer> buffer);

    // Tracks the last streaming center so we do not rebuild every frame.
    glm::ivec3 m_lastStreamCenter{INT32_MAX};

    // True while chunks around m_lastStreamCenter are still being populated.
    // Forces updateStreaming() to keep running until the ring is fully loaded.
    bool m_streamingInProgress = true;

    // Pre-sorted (nearest-first) queue of chunks to stream around the current
    // centre. Consumed a few entries per frame so the main thread stays responsive.
    std::vector<glm::ivec3> m_loadQueue;
    size_t m_loadQueueCursor = 0;
    int m_loadRadius = 0;

    // Loaded chunks whose mesh is missing or stale (a neighbour arrived since).
    std::unordered_set<glm::ivec3, IVec3Hash> m_pendingMesh;

    // Uniform buffers per frame-in-flight
    std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;

    // Descriptor objects (one set per frame-in-flight).
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    uint32_t m_frameCount = 0;

    // Statistics.
    size_t m_totalVertices = 0;
    size_t m_totalTriangles = 0;
    size_t m_visibleChunks  = 0;

    // Frustum planes rebuilt each frame in updateUniforms().
    Plane m_frustumPlanes[6];

    bool createDescriptorSetLayout();
    bool createUniformBuffers();
    bool createDescriptorPool();
    bool createDescriptorSets();

    // Re-mesh the chunk at cpos and (re)upload its vertex/index buffers.
    void uploadChunk(const glm::ivec3& cpos);

    // Walk loaded chunks and refresh m_totalVertices / m_totalTriangles.
    void recomputeStats();

    // Rebuild m_loadQueue (circular disc, nearest-first) around the new centre.
    void rebuildLoadQueue(const glm::ivec3& center, int loadRadius);

    // Drop chunks whose XZ distance exceeds the hysteresis radius.
    void unloadFarChunks(const glm::ivec3& center, int unloadSq);

    // Generate up to kMaxNewPerFrame chunks from m_loadQueue, then mesh up to
    // kMaxMeshesPerFrame pending chunks whose in-range neighbours are all loaded.
    void streamNextBatch();
    bool isExpectedChunk(const glm::ivec3& cpos) const;
    bool isReadyToMesh(const glm::ivec3& cpos) const;

    // Gribb-Hartmann frustum-plane extraction from the un-flipped VP matrix.
    void extractFrustumPlanes(const glm::mat4& vp);
};

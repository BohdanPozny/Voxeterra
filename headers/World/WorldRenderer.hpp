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
    bool init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t imageCount);
    void cleanup();

    // Prepare an empty world; streaming will populate chunks on demand.
    void generateWorld();

    // Load chunks within renderDistance, unload chunks beyond (renderDistance + 1).
    // Expected to be called every frame from PlayingState.
    void updateStreaming(const glm::vec3& cameraPos, int renderDistance);

    // Upload MVP matrices + rebuild frustum planes for the given frame.
    void updateUniforms(uint32_t imageIndex, const Camera& camera);

    // Record draw calls for the visible chunks.
    void render(VkCommandBuffer commandBuffer, uint32_t imageIndex);

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

    // Per-chunk GPU buffers keyed by chunk coordinate.
    struct ChunkBuffers {
        std::unique_ptr<Buffer> vertex;
        std::unique_ptr<Buffer> index;
        uint32_t indexCount = 0;

        std::unique_ptr<Buffer> waterVertex;
        std::unique_ptr<Buffer> waterIndex;
        uint32_t waterIndexCount = 0;
    };
    std::unordered_map<glm::ivec3, ChunkBuffers, IVec3Hash> m_chunkBuffers;

    // Tracks the last streaming center so we do not rebuild every frame.
    glm::ivec3 m_lastStreamCenter{INT32_MAX};

    // True while chunks around m_lastStreamCenter are still being populated.
    // Forces updateStreaming() to keep running until the ring is fully loaded.
    bool m_streamingInProgress = true;

    // Pre-sorted (nearest-first) queue of chunks to stream around the current
    // centre. Consumed a few entries per frame so the main thread stays responsive.
    std::vector<glm::ivec3> m_loadQueue;
    size_t m_loadQueueCursor = 0;

    // Uniform buffers per frame-in-flight
    std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;

    // Descriptor objects (one set per frame-in-flight).
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    uint32_t m_imageCount = 0;

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
};

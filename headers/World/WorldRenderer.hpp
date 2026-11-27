#pragma once

#include "Vulkan/Pipeline.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/MeshArena.hpp"
#include "World/ColumnWorkers.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

class Device;
class Camera;

// Renders the voxel world as a quadtree of LOD columns: full detail near the camera,
// coarser cells further out. Columns are built on worker threads and sub-allocated
// from a shared vertex arena; quads use one shared index buffer.
class WorldRenderer {
public:
    // MVP uniform block uploaded per frame.
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    // Plane in voxel space: n.p + d >= 0 means "inside".
    struct Plane { glm::vec3 normal; float d; };

    WorldRenderer() = default;
    ~WorldRenderer();

    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    bool init(Device& device, VkRenderPass renderPass, VkExtent2D extent, uint32_t frameCount);
    void cleanup();

    // Select the LOD columns for this camera, schedule missing ones and upload finished
    // builds. renderDistance is in full-detail chunks. Called every frame from PlayingState.
    void updateStreaming(const glm::vec3& cameraPos, int renderDistance);

    // Upload MVP matrices + rebuild frustum planes for the given frame-in-flight.
    void updateUniforms(uint32_t frameIndex, const Camera& camera);

    // Record draw calls for the visible columns.
    void render(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    // Statistics.
    size_t getTotalVertices() const { return m_totalQuads * 4; }
    size_t getTotalTriangles() const { return m_totalQuads * 2; }
    size_t getChunkCount() const { return m_columns.size(); }
    size_t getVisibleChunks() const { return m_visibleColumns; }
    size_t getPendingColumns() const { return m_scheduled.size(); }
    uint64_t getMeshBytes() const { return m_arena.usedBytes(); }

private:
    struct Column {
        MeshArena::Allocation alloc;  // solid quads, then water quads
        uint32_t solidQuads = 0;
        uint32_t waterQuads = 0;
        unsigned openSides = 0;
        int minY = 0, maxY = 0;       // voxel range for culling
    };

    using ColumnMap = std::unordered_map<ColumnKey, Column, ColumnKeyHash>;
    using SideMap = std::unordered_map<ColumnKey, unsigned, ColumnKeyHash>;

    Device* m_device = nullptr;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;

    Pipeline m_pipeline;        // opaque pass: depth-write on
    Pipeline m_waterPipeline;   // transparent pass: depth-test only + alpha blending

    MeshArena m_arena;
    std::unique_ptr<Buffer> m_quadIndices;  // 0,1,2,0,2,3 + 4k pattern
    ColumnWorkers m_workers;

    ColumnMap m_columns;    // uploaded, possibly stale until their area is covered
    SideMap m_desired;      // current LOD selection -> open sides
    SideMap m_scheduled;    // queued or running builds -> open sides
    std::vector<ColumnWorkers::Result> m_results;

    // Arena ranges freed while earlier frames may still read them.
    std::vector<std::pair<uint64_t, MeshArena::Allocation>> m_retired;
    uint64_t m_frameNumber = 0;

    glm::vec2 m_cameraXZ{0.0f};  // voxels
    glm::ivec2 m_lastCenter{INT32_MAX};
    int m_lastDistance = -1;

    // Uniform buffers and descriptor sets per frame-in-flight.
    std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    uint32_t m_frameCount = 0;

    size_t m_totalQuads = 0;
    size_t m_visibleColumns = 0;

    Plane m_frustumPlanes[6];

    bool createDescriptorSetLayout();
    bool createUniformBuffers();
    bool createDescriptorPool();
    bool createDescriptorSets();
    bool createQuadIndices();

    void selectColumns(int renderDistance);
    void scheduleBuilds();
    bool uploadResults();
    void dropCoveredColumns();
    bool isCovered(const ColumnKey& stale) const;
    float distanceTo(const ColumnKey& key) const;
    void upload(ColumnWorkers::Result& result);
    void retire(const MeshArena::Allocation& alloc);
    void recomputeStats();
    void drawPass(VkCommandBuffer cmd, const Pipeline& pipeline, uint32_t frameIndex, bool water);

    // Gribb-Hartmann frustum-plane extraction from the un-flipped MVP matrix.
    void extractFrustumPlanes(const glm::mat4& vp);
};

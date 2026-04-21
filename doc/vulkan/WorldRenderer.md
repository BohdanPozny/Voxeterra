# WorldRenderer

## Огляд

`WorldRenderer` — високорівневий клас для рендерингу воксельного світу. Управляє:
- Voxel pipeline та descriptors
- Vertex/Index buffers для чанків
- Uniform buffers (MVP матриці)
- Меш генерацією для чанків

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class Device;
class Pipeline;
class RenderPass;
class Window;
class World;
class Camera;
class Buffer;

class WorldRenderer {
public:
    WorldRenderer() = default;
    ~WorldRenderer();

    bool init(Device& device, RenderPass& renderPass, Window& window);
    void cleanup();

    void update(float deltaTime);
    void recordDrawCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    World& getWorld() { return *m_world; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    // Pipeline
    std::unique_ptr<Pipeline> m_pipeline;

    // World data
    std::unique_ptr<World> m_world;

    // Vertex/Index buffers per chunk
    struct ChunkBuffers {
        std::unique_ptr<Buffer> vertexBuffer;
        std::unique_ptr<Buffer> indexBuffer;
        uint32_t indexCount = 0;
    };
    std::unordered_map<ChunkPos, ChunkBuffers, ChunkPosHash> m_chunkBuffers;

    // Uniform buffers (one per swapchain image)
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };
    std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;

    // Descriptor set layout & pool
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    void createUniformBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage, const Camera& camera);
    void createChunkBuffers();
    void recordChunkDraw(VkCommandBuffer cmd, const ChunkBuffers& buffers);
};
```

## Ініціалізація

```cpp
bool WorldRenderer::init(Device& device, RenderPass& renderPass, Window& window) {
    m_device = device.getLogicalDevice();
    m_physicalDevice = device.getPhysicalDevice();

    // Створюємо 3D pipeline
    m_pipeline = std::make_unique<Pipeline>();
    if (!m_pipeline->init(device, renderPass, window.getExtent(),
                          "shaders/voxel.vert.spv",
                          "shaders/voxel.frag.spv")) {
        return false;
    }

    // Створюємо світ та генеруємо тестові дані
    m_world = std::make_unique<World>();
    m_world->generateTestWorld();

    // Створюємо uniform buffers та descriptors
    createUniformBuffers();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSets();

    // Створюємо GPU buffers для чанків
    createChunkBuffers();

    return true;
}
```

## Uniform Buffers (MVP)

```cpp
void WorldRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    size_t swapchainImageCount = m_swapchain->getImageCount();
    m_uniformBuffers.resize(swapchainImageCount);

    for (size_t i = 0; i < swapchainImageCount; i++) {
        m_uniformBuffers[i] = std::make_unique<Buffer>();
        m_uniformBuffers[i]->init(*m_devicePtr, bufferSize,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void WorldRenderer::updateUniformBuffer(uint32_t currentImage, const Camera& camera) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);  // Модельна матриця (identity)
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.proj[1][1] *= -1;  // Flip Y для Vulkan (LHS vs RHS)

    // Копіюємо в GPU
    m_uniformBuffers[currentImage]->copyData(&ubo, sizeof(ubo));
}
```

## Descriptor Set Layout

```cpp
void WorldRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout);
}
```

## Chunk Buffer Generation

```cpp
void WorldRenderer::createChunkBuffers() {
    for (auto& [pos, chunk] : m_world->getChunks()) {
        // Генеруємо меш для чанку
        auto [vertices, indices] = chunk.generateMesh();

        if (vertices.empty() || indices.empty()) continue;

        ChunkBuffers buffers;

        // Vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        buffers.vertexBuffer = std::make_unique<Buffer>();
        buffers.vertexBuffer->init(*m_devicePtr, vertexBufferSize,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        buffers.vertexBuffer->copyData(vertices.data(), vertexBufferSize);

        // Index buffer
        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        buffers.indexBuffer = std::make_unique<Buffer>();
        buffers.indexBuffer->init(*m_devicePtr, indexBufferSize,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        buffers.indexBuffer->copyData(indices.data(), indexBufferSize);

        buffers.indexCount = static_cast<uint32_t>(indices.size());
        m_chunkBuffers[pos] = std::move(buffers);
    }
}
```

## Recording Draw Commands

```cpp
void WorldRenderer::recordDrawCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                      m_pipeline->getPipeline());

    // Bind descriptor sets (MVP matrices)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           m_pipeline->getLayout(), 0, 1, 
                           &m_descriptorSets[imageIndex], 0, nullptr);

    // Draw each chunk
    for (const auto& [pos, buffers] : m_chunkBuffers) {
        if (buffers.indexCount == 0) continue;

        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {buffers.vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // Bind index buffer
        vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer->getBuffer(), 
                            0, VK_INDEX_TYPE_UINT32);

        // Draw indexed
        vkCmdDrawIndexed(commandBuffer, buffers.indexCount, 1, 0, 0, 0);
    }
}
```

## Cleanup

```cpp
WorldRenderer::~WorldRenderer() {
    cleanup();
}

void WorldRenderer::cleanup() {
    // Cleanup buffers
    m_chunkBuffers.clear();
    m_uniformBuffers.clear();

    // Cleanup descriptors
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    }

    // Cleanup pipeline
    if (m_pipeline) {
        m_pipeline->cleanup();
    }
}
```

## Vertex Shader (voxel.vert)

```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

## Fragment Shader (voxel.frag)

```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

## Дивіться також

- [[World\|World]] — управління чанками
- [[Chunk\|Chunk]] — генерація мешу
- [[Buffer\|Buffer]] — Vulkan buffer wrapper
- [[Pipeline\|Pipeline]] — 3D graphics pipeline

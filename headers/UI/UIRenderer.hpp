#pragma once

#include "UI/UIElement.hpp"
#include "Vulkan/Pipeline2D.hpp"
#include "Vulkan/TextRenderer.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Device;
class RenderPass;

// Batches the UI widget tree into a single Vulkan draw call.
class UIRenderer {
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    Device* m_devicePtr = nullptr;  // used when creating the growing vertex buffer.

    std::unique_ptr<Pipeline2D>  m_pipeline2D;
    std::unique_ptr<TextRenderer> m_textRenderer;
    bool m_useTextRenderer = false;

    struct UIVertex {
        glm::vec2 position;  // NDC
        glm::vec4 color;
        glm::vec2 texCoord;  // (0,0) means "no texture" for the shader
    };
    
    std::vector<UIVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    
    // Persistent, growable vertex buffer.
    std::unique_ptr<class Buffer> m_vertexBuffer;
    size_t m_vertexBufferSize = 0;
    
    VkExtent2D m_screenExtent;

    // Descriptor set holding the font atlas; required by the 2D pipeline layout.
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_fontDescriptorSet = VK_NULL_HANDLE;

    bool createFontDescriptorSet();

    void collectUIElements(UIElement* element);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

public:
    UIRenderer() = default;
    ~UIRenderer();
    
    bool init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent);
    void cleanup();
    
    // Draw one element and recurse into its children (legacy helper; prefer renderUI()).
    void renderElement(UIElement* element, VkCommandBuffer commandBuffer);

    // Draw the entire widget tree rooted at rootElement.
    void renderUI(UIElement* rootElement, VkCommandBuffer commandBuffer);

    // Low-level primitive helpers.
    void addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
    void clearBatch();
    void flushBatch(VkCommandBuffer commandBuffer);
    
    // Convert normalised [0,1] coords to NDC [-1,1].
    glm::vec2 normalizedToScreen(const glm::vec2& normalized) const;
};

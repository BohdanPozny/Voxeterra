#pragma once

#include "UI/UIElement.hpp"
#include "Vulkan/Pipeline2D.hpp"
#include "Vulkan/TextRenderer.hpp"
#include "Vulkan/Buffer.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

class Device;

// Batches the UI widget tree into a single Vulkan draw call.
class UIRenderer {
private:
    VkDevice m_device = VK_NULL_HANDLE;
    Device* m_devicePtr = nullptr;  // used when creating the growing vertex buffers.

    std::unique_ptr<Pipeline2D>  m_pipeline2D;
    std::unique_ptr<TextRenderer> m_textRenderer;
    glm::vec2 m_screenSize{1.0f};  // swapchain extent of the frame being built

    // Position in NDC; untextured quads sample the atlas's white texel.
    using UIVertex = TextRenderer::UIVertex;
    std::vector<UIVertex> m_vertices;

    // One growable vertex buffer per frame-in-flight.
    struct FrameBuffer {
        std::unique_ptr<Buffer> buffer;
        size_t size = 0;
    };
    std::vector<FrameBuffer> m_frameBuffers;

    // Descriptor set holding the font atlas; required by the 2D pipeline layout.
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_fontDescriptorSet = VK_NULL_HANDLE;

    bool createFontDescriptorSet();

    void collectUIElements(UIElement* element);
    void addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);

public:
    UIRenderer() = default;
    ~UIRenderer();
    
    bool init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent,
              const std::string& fontPath, int fontSize, uint32_t frameCount);
    void cleanup();

    // Draw the entire widget tree rooted at rootElement.
    void renderUI(UIElement* rootElement, VkCommandBuffer commandBuffer, uint32_t frameIndex,
                  VkExtent2D extent);
};

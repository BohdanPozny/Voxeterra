#include "UI/UIRenderer.hpp"
#include "UI/UIButton.hpp"
#include "UI/UILabel.hpp"
#include "UI/UIPanel.hpp"
#include "Vulkan/Device.hpp"
#include "utils/Profiler.hpp"
#include <algorithm>
#include <iostream>
#include <string>

UIRenderer::~UIRenderer() {
    cleanup();
}

bool UIRenderer::init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent,
                      const std::string& fontPath, int fontSize, uint32_t frameCount) {
    m_device = device.getLogicalDevice();
    m_devicePtr = &device;
    m_frameBuffers.resize(frameCount);

    m_pipeline2D = std::make_unique<Pipeline2D>();
    if (!m_pipeline2D->init(device, renderPass, screenExtent)) {
        std::cerr << "[UIRenderer] Failed to initialize Pipeline2D" << std::endl;
        return false;
    }

    // The atlas also holds the white texel that untextured quads sample.
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(device, fontPath, fontSize)) {
        std::cerr << "[UIRenderer] Failed to load font: " << fontPath << std::endl;
        return false;
    }

    if (!createFontDescriptorSet()) {
        std::cerr << "[UIRenderer] Failed to create font descriptor set" << std::endl;
        return false;
    }

    std::cout << "[UIRenderer] Initialized with Pipeline2D" << std::endl;
    return true;
}

bool UIRenderer::createFontDescriptorSet() {
    if (!m_textRenderer || m_textRenderer->getImageView() == VK_NULL_HANDLE) {
        std::cerr << "[UIRenderer] Cannot create descriptor set: no font texture" << std::endl;
        return false;
    }

    // Single-set descriptor pool for the font sampler.
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "[UIRenderer] Failed to create descriptor pool" << std::endl;
        return false;
    }

    // Allocate using Pipeline2D's descriptor set layout.
    VkDescriptorSetLayout layout = m_pipeline2D->getDescriptorSetLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_fontDescriptorSet) != VK_SUCCESS) {
        std::cerr << "[UIRenderer] Failed to allocate descriptor set" << std::endl;
        return false;
    }

    // Bind the atlas texture into the descriptor.
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_textRenderer->getImageView();
    imageInfo.sampler = m_textRenderer->getSampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_fontDescriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    return true;
}

void UIRenderer::cleanup() {
    // Destroying the pool frees every descriptor set it owns.
    if (m_descriptorPool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_fontDescriptorSet = VK_NULL_HANDLE;
    }

    // Pipeline2D and TextRenderer own their Vulkan objects; tear them down
    // while the logical device is still valid.
    m_textRenderer.reset();
    if (m_pipeline2D) m_pipeline2D->cleanup();
    m_pipeline2D.reset();
    m_frameBuffers.clear();

    m_device = VK_NULL_HANDLE;
}

void UIRenderer::addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
    // Convert normalised coords to NDC (-1..1) and emit a quad.
    const float x1 = pos.x * 2.0f - 1.0f;
    const float y1 = pos.y * 2.0f - 1.0f;
    const float x2 = (pos.x + size.x) * 2.0f - 1.0f;
    const float y2 = (pos.y + size.y) * 2.0f - 1.0f;
    const glm::vec2 uv = m_textRenderer->getSolidUV();

    m_vertices.push_back({glm::vec2(x1, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y2), color, uv});

    m_vertices.push_back({glm::vec2(x1, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y2), color, uv});
    m_vertices.push_back({glm::vec2(x1, y2), color, uv});
}

void UIRenderer::renderUI(UIElement* rootElement, VkCommandBuffer commandBuffer, uint32_t frameIndex,
                          VkExtent2D extent) {
    ZoneScoped;
    if (!rootElement || extent.width == 0 || extent.height == 0) return;

    m_screenSize = glm::vec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
    m_vertices.clear();
    collectUIElements(rootElement);
    if (m_vertices.empty()) return;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline2D->getPipeline());
    if (m_fontDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipeline2D->getLayout(), 0, 1,
                                &m_fontDescriptorSet, 0, nullptr);
    }

    // This frame's fence has been waited on, so its buffer is free to rewrite or replace.
    FrameBuffer& fb = m_frameBuffers[frameIndex];
    const size_t bufferSize = sizeof(UIVertex) * m_vertices.size();
    if (!fb.buffer || bufferSize > fb.size) {
        auto buffer = std::make_unique<Buffer>();
        if (!buffer->init(*m_devicePtr, bufferSize * 2,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            std::cerr << "[UIRenderer] Failed to create vertex buffer" << std::endl;
            return;
        }
        fb.buffer = std::move(buffer);
        fb.size = bufferSize * 2;
    }
    fb.buffer->copyData(m_vertices.data(), bufferSize);

    VkBuffer vertexBuffers[] = {fb.buffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(m_vertices.size()), 1, 0, 0);
}

void UIRenderer::collectUIElements(UIElement* element) {
    if (!element || !element->isVisible()) return;
    
    // Draw the element's background unless it is effectively transparent.
    const glm::vec4 color = element->getColor();
    if (color.a > 0.01f) {
        addQuad(element->getPosition(), element->getSize(), color);
    }

    // Labels and buttons render a text overlay on top of the quad.
    UILabel*  label  = dynamic_cast<UILabel*>(element);
    UIButton* button = dynamic_cast<UIButton*>(element);
    std::string text;
    glm::vec4 textColor(1.0f);
    bool hasText = false;

    if (label) {
        text = label->getText();
        textColor = label->getTextColor();
        hasText = !text.empty();
    } else if (button) {
        text = button->getText();
        hasText = !text.empty();
    }

    if (hasText) {
        const glm::vec2 boxPos  = element->getPosition() * m_screenSize;
        const glm::vec2 boxSize = element->getSize() * m_screenSize;

        // Line height fills ~60% of the box, shrunk if the string would overflow it.
        float scale = boxSize.y * 0.6f / m_textRenderer->getLineHeight();
        const float padding = std::min(boxSize.y * 0.25f, boxSize.x * 0.08f);
        float width = m_textRenderer->measureTextWidth(text, scale);
        const float maxWidth = boxSize.x - 2.0f * padding;
        if (width > maxWidth && width > 0.0f) {
            scale *= maxWidth / width;
            width = maxWidth;
        }

        const bool centered = button || (label && label->isCentered());
        const float textX = centered ? boxPos.x + (boxSize.x - width) * 0.5f
                                     : boxPos.x + padding;
        // Centre capital letters vertically; descenders hang below as usual.
        const float baselineY = boxPos.y + (boxSize.y + m_textRenderer->getCapHeight() * scale) * 0.5f;

        m_textRenderer->renderText(text, glm::vec2(textX, baselineY), scale, textColor,
                                   2.0f / m_screenSize, m_vertices);
    }

    for (const auto& child : element->getChildren()) {
        collectUIElements(child.get());
    }
}

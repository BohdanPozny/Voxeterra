#include "UI/UIRenderer.hpp"
#include "UI/UIButton.hpp"
#include "UI/UILabel.hpp"
#include "UI/UIPanel.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Pipeline2D.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/TextRenderer.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include <fstream>

UIRenderer::~UIRenderer() {
    cleanup();
}

bool UIRenderer::init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent) {
    m_device = device.getLogicalDevice();
    m_physicalDevice = device.getPhysicalDevice();
    m_devicePtr = &device;
    m_screenExtent = screenExtent;

    m_pipeline2D = std::make_unique<Pipeline2D>();
    if (!m_pipeline2D->init(device, renderPass, screenExtent)) {
        std::cerr << "[UIRenderer] Failed to initialize Pipeline2D" << std::endl;
        return false;
    }

    // Larger atlas font size improves clarity when the text is scaled down.
    m_textRenderer = std::make_unique<TextRenderer>();
    m_useTextRenderer = m_textRenderer->init(device, "fonts/default.ttf", 48);
    if (!m_useTextRenderer) {
        std::cerr << "[UIRenderer] TTF font failed, falling back to bitmap font" << std::endl;
    }

    // Descriptor set is required by the Pipeline2D layout even when the font is absent.
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
    m_vertexBuffer.reset();

    m_device = VK_NULL_HANDLE;
}

void UIRenderer::renderElement(UIElement* element, VkCommandBuffer commandBuffer) {
    if (!element || !element->isVisible()) return;

    // Prefer renderUI(): it batches the entire tree in one draw call.
    clearBatch();
    addQuad(element->getPosition(), element->getSize(), element->getColor());
    for (const auto& child : element->getChildren()) {
        renderElement(child.get(), commandBuffer);
    }
}

void UIRenderer::addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
    // Convert normalised coords to NDC (-1..1) and emit a quad.
    // UV = (0,0) tells the shader to skip the font texture lookup.
    const float x1 = pos.x * 2.0f - 1.0f;
    const float y1 = pos.y * 2.0f - 1.0f;
    const float x2 = (pos.x + size.x) * 2.0f - 1.0f;
    const float y2 = (pos.y + size.y) * 2.0f - 1.0f;
    const glm::vec2 uv(0.0f, 0.0f);

    m_vertices.push_back({glm::vec2(x1, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y2), color, uv});

    m_vertices.push_back({glm::vec2(x1, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y2), color, uv});
    m_vertices.push_back({glm::vec2(x1, y2), color, uv});
}

void UIRenderer::clearBatch() {
    m_vertices.clear();
    m_indices.clear();
}

void UIRenderer::renderUI(UIElement* rootElement, VkCommandBuffer commandBuffer) {
    if (!rootElement) return;
    
    clearBatch();
    collectUIElements(rootElement);
    if (m_vertices.empty()) return;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline2D->getPipeline());
    if (m_fontDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipeline2D->getLayout(), 0, 1,
                                &m_fontDescriptorSet, 0, nullptr);
    }

    // Grow the vertex buffer geometrically to amortise reallocations.
    const size_t bufferSize = sizeof(UIVertex) * m_vertices.size();
    if (!m_vertexBuffer || bufferSize > m_vertexBufferSize) {
        m_vertexBufferSize = bufferSize * 2;
        m_vertexBuffer = std::make_unique<Buffer>();
        if (!m_vertexBuffer->init(*m_devicePtr, m_vertexBufferSize,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            std::cerr << "[UIRenderer] Failed to create vertex buffer" << std::endl;
            return;
        }
    }
    m_vertexBuffer->copyData(m_vertices.data(), bufferSize);

    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
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
        if (m_useTextRenderer && m_textRenderer) {
            // Both the quad batch and TextRenderer emit vertices in NDC (-1..1).
            const glm::vec2 pos  = element->getPosition();
            const glm::vec2 size = element->getSize();

            // Text height ~ 60% of the element's height expressed in NDC.
            const float fontCellHeight = static_cast<float>(m_textRenderer->getFontSize());
            const float targetHeightNDC = size.y * 2.0f * 0.6f;
            const float scale = targetHeightNDC / fontCellHeight;
            const float textWidthNDC = m_textRenderer->measureTextWidth(text, scale);

            // Buttons centre the text; labels use a small left padding.
            float textX;
            if (button) {
                const float centerX = (pos.x + size.x * 0.5f) * 2.0f - 1.0f;
                textX = centerX - textWidthNDC * 0.5f;
            } else {
                textX = (pos.x + size.x * 0.08f) * 2.0f - 1.0f;
            }

            // TextRenderer draws from the baseline near the top of the glyph box.
            const float centerYNDC = (pos.y + size.y * 0.5f) * 2.0f - 1.0f;
            const float textY = centerYNDC + targetHeightNDC * 0.5f;

            auto& vertices = reinterpret_cast<std::vector<TextRenderer::UIVertex>&>(m_vertices);
            m_textRenderer->renderText(text, glm::vec2(textX, textY), scale, textColor, vertices);
        } else if (label) {
            // Fallback: 5x7 pixel bitmap font built from quads.
        float pixelSize = element->getSize().y / 9.0f;
        float charWidth = pixelSize * 6.0f;

        float startX = element->getPosition().x + pixelSize;
        float startY = element->getPosition().y + pixelSize;
        auto getPattern = [](char c) -> const int* {
            static const int font_F[7] = {0b11110, 0b10000, 0b11100, 0b10000, 0b10000, 0b10000, 0b10000};
            static const int font_P[7] = {0b11110, 0b10010, 0b10010, 0b11110, 0b10000, 0b10000, 0b10000};
            static const int font_S[7] = {0b01110, 0b10000, 0b10000, 0b01110, 0b00010, 0b00010, 0b11100};
            static const int font_0[7] = {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
            static const int font_1[7] = {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
            static const int font_2[7] = {0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111};
            static const int font_3[7] = {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
            static const int font_4[7] = {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
            static const int font_5[7] = {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110};
            static const int font_6[7] = {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
            static const int font_7[7] = {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
            static const int font_8[7] = {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
            static const int font_9[7] = {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};
            static const int font_colon[7] = {0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000};
            static const int font_dot[7] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100};
            static const int font_space[7] = {0, 0, 0, 0, 0, 0, 0};
            
            switch(c) {
                case 'F': return font_F;
                case 'P': return font_P;
                case 'S': return font_S;
                case '0': return font_0;
                case '1': return font_1;
                case '2': return font_2;
                case '3': return font_3;
                case '4': return font_4;
                case '5': return font_5;
                case '6': return font_6;
                case '7': return font_7;
                case '8': return font_8;
                case '9': return font_9;
                case ':': return font_colon;
                case '.': return font_dot;
                case ' ': return font_space;
                default: return font_space;
            }
        };
        
        auto drawChar = [&](char c, float x, float y) {
            const int* pattern = getPattern(c);
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (pattern[row] & (1 << (4 - col))) {
                        addQuad(glm::vec2(x + col * pixelSize, y + row * pixelSize),
                                glm::vec2(pixelSize * 0.9f, pixelSize * 0.9f),
                                textColor);
                    }
                }
            }
        };

            for (size_t i = 0; i < text.length(); ++i) {
                drawChar(text[i], startX + i * charWidth, startY);
            }
        }
    }

    for (const auto& child : element->getChildren()) {
        collectUIElements(child.get());
    }
}

// Deprecated: retained so existing references keep linking; renderUI performs the actual draw.
void UIRenderer::flushBatch(VkCommandBuffer /*commandBuffer*/) {}

glm::vec2 UIRenderer::normalizedToScreen(const glm::vec2& normalized) const {
    return glm::vec2(normalized.x * 2.0f - 1.0f, normalized.y * 2.0f - 1.0f);
}

uint32_t UIRenderer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return 0;
}

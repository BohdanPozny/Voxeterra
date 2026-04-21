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
    m_devicePtr = &device;  // Зберігаємо для створення Buffer
    m_screenExtent = screenExtent;
    
    // Створюємо 2D pipeline
    m_pipeline2D = std::make_unique<Pipeline2D>();
    if (!m_pipeline2D->init(device, renderPass, screenExtent)) {
        std::cerr << "[UIRenderer] Failed to initialize Pipeline2D" << std::endl;
        return false;
    }
    
    // Завантажуємо TTF шрифт
    m_textRenderer = std::make_unique<TextRenderer>();
    if (m_textRenderer->init(device, "fonts/default.ttf", 16)) {
        m_useTextRenderer = true;
        std::cout << "[UIRenderer] TTF font loaded - ready for rendering" << std::endl;
    } else {
        std::cout << "[UIRenderer] TTF font failed, using bitmap font" << std::endl;
        m_useTextRenderer = false;
    }
    
    std::cout << "[UIRenderer] Initialized with Pipeline2D" << std::endl;
    return true;
}

void UIRenderer::cleanup() {
    // TextRenderer cleanup викликається автоматично через destructor
    m_textRenderer.reset();
    
    // Pipeline2D cleanup - викликаємо явно поки device валідний, потім reset
    if (m_pipeline2D) {
        m_pipeline2D->cleanup();
    }
    m_pipeline2D.reset();
    
    m_vertexBuffer.reset();
    m_device = VK_NULL_HANDLE;
}

void UIRenderer::renderElement(UIElement* element, VkCommandBuffer commandBuffer) {
    if (!element || !element->isVisible()) return;
    
    // Рендеримо поточний елемент
    clearBatch();
    
    // Додаємо quad для елемента
    addQuad(element->getPosition(), element->getSize(), element->getColor());
    
    // Flush batch (поки що нічого не робимо, бо немає pipeline)
    // flushBatch(commandBuffer);
    
    // Рекурсивно рендеримо дочірні елементи
    for (const auto& child : element->getChildren()) {
        renderElement(child.get(), commandBuffer);
    }
}

void UIRenderer::addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) {
    // Конвертуємо normalized координати в screen space (-1 to 1)
    float x1 = pos.x * 2.0f - 1.0f;
    float y1 = pos.y * 2.0f - 1.0f;
    float x2 = (pos.x + size.x) * 2.0f - 1.0f;
    float y2 = (pos.y + size.y) * 2.0f - 1.0f;
    
    // Створюємо 6 вершин для 2 трикутники (quad)
    // UV = (0,0) означає "без текстури" (shader перевіряє це)
    glm::vec2 uv(0.0f, 0.0f);
    
    // Трикутник 1
    m_vertices.push_back({glm::vec2(x1, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y1), color, uv});
    m_vertices.push_back({glm::vec2(x2, y2), color, uv});
    
    // Трикутник 2
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
    
    // Збираємо всі UI елементи в batch
    collectUIElements(rootElement);
    
    if (m_vertices.empty()) {
        static int emptyCount = 0;
        if (emptyCount++ < 5) {
            std::cout << "[UIRenderer] ⚠️ No vertices to render!" << std::endl;
        }
        return;
    }
    
    static int renderCount = 0;
    if (renderCount++ < 5) {
        std::cout << "[UIRenderer] ✓ Rendering " << m_vertices.size() << " vertices" << std::endl;
    }
    
    // Bind UI pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline2D->getPipeline());
    
    // Перевіряємо чи треба створити/оновити vertex buffer
    size_t bufferSize = sizeof(UIVertex) * m_vertices.size();
    
    if (!m_vertexBuffer || bufferSize > m_vertexBufferSize) {
        // Створюємо новий buffer (більший)
        m_vertexBufferSize = bufferSize * 2; // З запасом
        m_vertexBuffer = std::make_unique<Buffer>();
        
        if (!m_vertexBuffer->init(*m_devicePtr, m_vertexBufferSize,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            std::cerr << "[UIRenderer] Failed to create vertex buffer!" << std::endl;
            return;
        }
    }
    
    // Копіюємо vertices в buffer
    m_vertexBuffer->copyData(m_vertices.data(), bufferSize);
    
    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    // Draw!
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(m_vertices.size()), 1, 0, 0);
}

void UIRenderer::collectUIElements(UIElement* element) {
    if (!element || !element->isVisible()) return;
    
    // Малюємо тільки якщо це не прозора панель (alpha > 0)
    glm::vec4 color = element->getColor();
    
    if (color.a > 0.01f) {
        addQuad(element->getPosition(), element->getSize(), color);
    }
    
    // Якщо це UILabel - малюємо текст
    UILabel* label = dynamic_cast<UILabel*>(element);
    if (label) {
        std::string text = label->getText();
        glm::vec4 textColor = label->getTextColor();
        
        // Якщо TTF шрифт завантажено - використовуємо його
        if (m_useTextRenderer && m_textRenderer) {
            float scale = element->getSize().y / 20.0f;  // Масштаб відносно висоти
            // Конвертуємо вектор (структури ідентичні, тільки namespace різний)
            auto& vertices = reinterpret_cast<std::vector<TextRenderer::UIVertex>&>(m_vertices);
            m_textRenderer->renderText(text, element->getPosition(), scale, textColor, vertices);
        } else {
            // Fallback: bitmap font (5x7 пікселів на символ)
        float pixelSize = element->getSize().y / 9.0f;  // 7 пікселів + відступи
        float charWidth = pixelSize * 6.0f;  // 5 пікселів + 1 відступ
        
        float startX = element->getPosition().x + pixelSize;
        float startY = element->getPosition().y + pixelSize;
        
        // Bitmap font patterns (5x7 pixels) - кожен символ як масив рядків
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
            
            // Малюємо кожен піксель літери
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (pattern[row] & (1 << (4 - col))) {  // Перевіряємо біт
                        glm::vec2 pixelPos(x + col * pixelSize, y + row * pixelSize);
                        glm::vec2 pixelSz(pixelSize * 0.9f, pixelSize * 0.9f);
                        addQuad(pixelPos, pixelSz, textColor);
                    }
                }
            }
        };
        
            // Малюємо кожен символ тексту
            for (size_t i = 0; i < text.length(); ++i) {
                drawChar(text[i], startX + i * charWidth, startY);
            }
        }  // Кінець else (bitmap font)
    }  // Кінець if (label)
    
    // Рекурсивно додаємо дочірні елементи
    for (const auto& child : element->getChildren()) {
        collectUIElements(child.get());
    }
}

void UIRenderer::flushBatch(VkCommandBuffer commandBuffer) {
    if (m_vertices.empty()) return;
    
    // Deprecated - використовуємо renderUI замість цього
}

glm::vec2 UIRenderer::normalizedToScreen(const glm::vec2& normalized) const {
    // Конвертуємо 0-1 в -1 to 1
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

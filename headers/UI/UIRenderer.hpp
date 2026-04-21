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

// UIRenderer - рендеринг UI елементів через Vulkan
class UIRenderer {
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    Device* m_devicePtr = nullptr;  // Для створення Buffer
    
    // 2D Pipeline для рендерингу
    std::unique_ptr<Pipeline2D> m_pipeline2D;
    
    // Text renderer для TTF шрифтів
    std::unique_ptr<TextRenderer> m_textRenderer;
    bool m_useTextRenderer = false;  // Чи використовувати TTF (якщо шрифт завантажено)
    
    // Vertex buffer для UI quad
    struct UIVertex {
        glm::vec2 position;  // Screen space (-1 to 1)
        glm::vec4 color;
        glm::vec2 texCoord;  // Текстурні координати (0-1)
    };
    
    std::vector<UIVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    
    // Persistent vertex buffer (використовуємо Buffer клас)
    std::unique_ptr<class Buffer> m_vertexBuffer;
    size_t m_vertexBufferSize = 0;
    
    VkExtent2D m_screenExtent;
    
    void collectUIElements(UIElement* element);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

public:
    UIRenderer() = default;
    ~UIRenderer();
    
    bool init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent);
    void cleanup();
    
    // Рендеринг UI елемента (рекурсивно з дочірніми)
    void renderElement(UIElement* element, VkCommandBuffer commandBuffer);
    
    // Рендеринг UI панелі (для MainMenu)
    void renderUI(UIElement* rootElement, VkCommandBuffer commandBuffer);
    
    // Допоміжні методи для створення примітивів
    void addQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
    void clearBatch();
    void flushBatch(VkCommandBuffer commandBuffer);
    
    // Конвертація normalized координат (0-1) в screen space (-1 to 1)
    glm::vec2 normalizedToScreen(const glm::vec2& normalized) const;
};

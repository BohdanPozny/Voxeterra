#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <unordered_map>

class Device;
class Pipeline2D;

// TextRenderer - рендеринг тексту через bitmap font
class TextRenderer {
public:
    struct Character {
        glm::vec2 size;      // Розмір символу
        glm::vec2 bearing;   // Offset від baseline
        float advance;       // Відстань до наступного символу
        glm::vec2 texCoords[4]; // UV координати в текстурі
    };

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    Device* m_devicePtr = nullptr;
    
    std::unordered_map<char, Character> m_characters;
    
    // Bitmap font texture (простий підхід - всі символи в одній текстурі)
    VkImage m_fontTexture = VK_NULL_HANDLE;
    VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
    VkImageView m_fontImageView = VK_NULL_HANDLE;
    VkSampler m_fontSampler = VK_NULL_HANDLE;
    
    int m_fontSize = 16;

public:
    TextRenderer() = default;
    ~TextRenderer();
    
    bool init(Device& device, const std::string& fontPath, int fontSize = 16);
    void cleanup();
    
    // Vertex структура (має співпадати з UIRenderer::UIVertex)
    struct UIVertex {
        glm::vec2 position;
        glm::vec4 color;
        glm::vec2 texCoord;
    };
    
    // Рендеринг тексту (повертає vertices для малювання)
    void renderText(const std::string& text, glm::vec2 position, float scale, 
                    glm::vec4 color, std::vector<UIVertex>& outVertices);
    
private:
    bool loadFont(const std::string& fontPath);
    void generateBitmapFont();
};

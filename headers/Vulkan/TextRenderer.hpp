#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <unordered_map>

class Device;
class Pipeline2D;

// Glyph atlas built from a TTF font using stb_truetype; consumed by UIRenderer.
class TextRenderer {
public:
    struct Character {
        glm::vec2 size;          // glyph size in pixels
        glm::vec2 bearing;       // offset from the pen position
        float     advance;       // horizontal advance to next glyph
        glm::vec2 texCoords[4];  // atlas UVs (tl, tr, br, bl)
    };

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    Device* m_devicePtr = nullptr;
    
    std::unordered_map<char, Character> m_characters;
    
    // Single-channel atlas containing every glyph.
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

    // Resources consumed by UIRenderer's descriptor set.
    VkImageView getImageView() const { return m_fontImageView; }
    VkSampler getSampler() const { return m_fontSampler; }
    
    // Must stay binary-compatible with UIRenderer::UIVertex.
    struct UIVertex {
        glm::vec2 position;
        glm::vec4 color;
        glm::vec2 texCoord;
    };
    
    // Append text quads to outVertices; returns nothing.
    void renderText(const std::string& text, glm::vec2 position, float scale,
                    glm::vec4 color, std::vector<UIVertex>& outVertices);

    // Text width using renderText units (NDC).
    float measureTextWidth(const std::string& text, float scale) const;

    int getFontSize() const { return m_fontSize; }
    
private:
    bool loadFont(const std::string& fontPath);
    void generateBitmapFont();
};

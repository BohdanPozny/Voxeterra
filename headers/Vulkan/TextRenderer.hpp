#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

class Device;
class Pipeline2D;

// Glyph atlas built from a TTF font using stb_truetype; consumed by UIRenderer.
class TextRenderer {
public:
    struct Character {
        glm::vec2 size;          // glyph size in pixels
        glm::vec2 bearing;       // offset from the pen position
        float     advance;       // horizontal advance to next glyph
        glm::vec2 uvMin;         // atlas UV of the top-left corner
        glm::vec2 uvMax;         // atlas UV of the bottom-right corner
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
    float m_ascent = 0.0f;      // pixels above the baseline at m_fontSize
    float m_descent = 0.0f;     // pixels below the baseline (negative)
    float m_capHeight = 0.0f;   // height of 'H', used for vertical centring
    glm::vec2 m_solidUV{0.0f};  // centre of the white block reserved in the atlas

public:
    TextRenderer() = default;
    ~TextRenderer();
    
    bool init(Device& device, const std::string& fontPath, int fontSize = 16);
    void cleanup();

    // Resources consumed by UIRenderer's descriptor set.
    VkImageView getImageView() const { return m_fontImageView; }
    VkSampler getSampler() const { return m_fontSampler; }
    
    // Layout must match the Pipeline2D vertex input (32 bytes).
    struct UIVertex {
        glm::vec2 position;
        glm::vec4 color;
        glm::vec2 texCoord;
    };
    
    // Append text quads to outVertices. baseline is in pixels; pixelToNdc maps a
    // pixel length to NDC per axis so glyphs keep their aspect ratio.
    void renderText(const std::string& text, glm::vec2 baseline, float scale,
                    glm::vec4 color, glm::vec2 pixelToNdc, std::vector<UIVertex>& outVertices) const;

    // Text advance width in pixels.
    float measureTextWidth(const std::string& text, float scale) const;

    float getLineHeight() const { return m_ascent - m_descent; }
    float getCapHeight() const { return m_capHeight; }
    glm::vec2 getSolidUV() const { return m_solidUV; }
    
private:
    bool uploadAtlas(VkBuffer staging, uint32_t width, uint32_t height);
};

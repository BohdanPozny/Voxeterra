#define STB_TRUETYPE_IMPLEMENTATION
#include "utils/stb_truetype.h"
#include "Vulkan/TextRenderer.hpp"
#include "Vulkan/Device.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

TextRenderer::~TextRenderer() {
    cleanup();
}

bool TextRenderer::init(Device& device, const std::string& fontPath, int fontSize) {
    m_device = device.getLogicalDevice();
    m_physicalDevice = device.getPhysicalDevice();
    m_devicePtr = &device;
    m_fontSize = fontSize;
    
    // Load the TTF file into memory.
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[TextRenderer] Failed to open font: " << fontPath << std::endl;
        return false;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0);
    
    std::vector<unsigned char> fontBuffer(fileSize);
    file.read(reinterpret_cast<char*>(fontBuffer.data()), fileSize);
    file.close();
    
    // Initialise stb_truetype with the font blob.
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontBuffer.data(), 0)) {
        std::cerr << "[TextRenderer] Failed to init font" << std::endl;
        return false;
    }
    
    // Rasterise printable ASCII glyphs (32..126).
    float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(fontSize));
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    
    // Single 512x512 atlas is plenty for the ASCII range.
    const int atlasWidth = 512;
    const int atlasHeight = 512;
    std::vector<unsigned char> atlasData(atlasWidth * atlasHeight, 0);
    
    int penX = 0, penY = 0;
    int rowHeight = 0;
    
    for (int c = 32; c < 127; ++c) {
        int width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &width, &height, &xoff, &yoff);
        
        if (bitmap) {
            // Move to a new row if the glyph overflows the current one.
            if (penX + width >= atlasWidth) {
                penX = 0;
                penY += rowHeight + 1;
                rowHeight = 0;
            }
            
            // Blit glyph bitmap into the atlas.
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int atlasIdx = (penY + y) * atlasWidth + (penX + x);
                    if (atlasIdx < atlasData.size()) {
                        atlasData[atlasIdx] = bitmap[y * width + x];
                    }
                }
            }
            
            Character ch;
            ch.size = glm::vec2(width, height);
            ch.bearing = glm::vec2(xoff, yoff);
            
            int advance, leftBearing;
            stbtt_GetCodepointHMetrics(&font, c, &advance, &leftBearing);
            ch.advance = advance * scale;
            
            // UVs for the four corners.
            ch.texCoords[0] = glm::vec2(float(penX) / atlasWidth, float(penY) / atlasHeight);  // top-left
            ch.texCoords[1] = glm::vec2(float(penX + width) / atlasWidth, float(penY) / atlasHeight);  // top-right
            ch.texCoords[2] = glm::vec2(float(penX + width) / atlasWidth, float(penY + height) / atlasHeight);  // bottom-right
            ch.texCoords[3] = glm::vec2(float(penX) / atlasWidth, float(penY + height) / atlasHeight);  // bottom-left
            
            m_characters[c] = ch;
            
            penX += width + 1;
            rowHeight = std::max(rowHeight, height);
            
            stbtt_FreeBitmap(bitmap, nullptr);
        } else {
            // Glyphs without a bitmap (e.g. space) still need an advance value.
            Character ch{};
            ch.size = glm::vec2(0.0f);
            ch.bearing = glm::vec2(0.0f);
            int advance = 0, leftBearing = 0;
            stbtt_GetCodepointHMetrics(&font, c, &advance, &leftBearing);
            ch.advance = advance * scale;
            ch.texCoords[0] = ch.texCoords[1] = ch.texCoords[2] = ch.texCoords[3] = glm::vec2(0.0f);
            m_characters[c] = ch;
        }
    }
    
    std::cout << "[TextRenderer] Loaded font: " << fontPath << " (" << m_characters.size() << " chars)" << std::endl;
    std::cout << "[TextRenderer] Created font atlas: " << atlasWidth << "x" << atlasHeight << std::endl;
    
    // Create a single-channel Vulkan image initialised with atlasData.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = atlasWidth;
    imageInfo.extent.height = atlasHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;   // single-channel (alpha coverage)
    imageInfo.tiling = VK_IMAGE_TILING_LINEAR;  // linear tiling avoids staging buffer
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_fontTexture) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to create font texture" << std::endl;
        return false;
    }
    
    // Query the driver for the image's memory requirements.
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_fontTexture, &memRequirements);

    // Allocate host-visible memory since we upload directly (no staging).
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    uint32_t memoryTypeIndex = 0;
    bool found = false;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            found = true;
            break;
        }
    }
    
    if (!found) {
        std::cerr << "[TextRenderer] Failed to find suitable memory type" << std::endl;
        return false;
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_fontMemory) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to allocate font texture memory" << std::endl;
        return false;
    }
    
    vkBindImageMemory(m_device, m_fontTexture, m_fontMemory, 0);
    
    // Upload glyph pixels directly. A staging buffer would be preferable for GPU-local memory.
    void* data;
    vkMapMemory(m_device, m_fontMemory, 0, memRequirements.size, 0, &data);
    memcpy(data, atlasData.data(), atlasData.size());
    vkUnmapMemory(m_device, m_fontMemory);
    
    // Image view used when binding the sampler.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_fontTexture;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_fontImageView) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to create font image view" << std::endl;
        return false;
    }
    
    // Linear filtering sampler clamped to atlas edges.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_fontSampler) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to create font sampler" << std::endl;
        return false;
    }
    
    std::cout << "[TextRenderer] Font texture created successfully" << std::endl;
    
    return true;
}

void TextRenderer::cleanup() {
    if (m_fontSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_fontSampler, nullptr);
        m_fontSampler = VK_NULL_HANDLE;
    }
    if (m_fontImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_fontImageView, nullptr);
        m_fontImageView = VK_NULL_HANDLE;
    }
    if (m_fontTexture != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_fontTexture, nullptr);
        m_fontTexture = VK_NULL_HANDLE;
    }
    if (m_fontMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_fontMemory, nullptr);
        m_fontMemory = VK_NULL_HANDLE;
    }
    m_characters.clear();
}

void TextRenderer::renderText(const std::string& text, glm::vec2 position, float scale, 
                               glm::vec4 color, std::vector<UIVertex>& outVertices) {
    float x = position.x;
    float y = position.y;
    
    for (char c : text) {
        if (m_characters.find(c) == m_characters.end()) continue;
        
        Character& ch = m_characters[c];
        
        float xpos = x + ch.bearing.x * scale;
        float ypos = y + ch.bearing.y * scale;
        float w = ch.size.x * scale;
        float h = ch.size.y * scale;
        
        // Emit a quad as two CCW triangles (atlas UVs: tl, tr, br, bl).
        const glm::vec2 uv0 = ch.texCoords[0];
        const glm::vec2 uv1 = ch.texCoords[1];
        const glm::vec2 uv2 = ch.texCoords[2];
        const glm::vec2 uv3 = ch.texCoords[3];

        outVertices.push_back({glm::vec2(xpos,     ypos),     color, uv0});
        outVertices.push_back({glm::vec2(xpos + w, ypos),     color, uv1});
        outVertices.push_back({glm::vec2(xpos + w, ypos + h), color, uv2});

        outVertices.push_back({glm::vec2(xpos,     ypos),     color, uv0});
        outVertices.push_back({glm::vec2(xpos + w, ypos + h), color, uv2});
        outVertices.push_back({glm::vec2(xpos,     ypos + h), color, uv3});
        
        x += ch.advance * scale;
    }
}

float TextRenderer::measureTextWidth(const std::string& text, float scale) const {
    float width = 0.0f;
    for (char c : text) {
        auto it = m_characters.find(c);
        if (it != m_characters.end()) {
            width += it->second.advance * scale;
        }
    }
    return width;
}

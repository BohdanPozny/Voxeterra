#define STB_TRUETYPE_IMPLEMENTATION
#include "utils/stb_truetype.h"
#include "Vulkan/TextRenderer.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Buffer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

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
    const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(fontSize));

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    m_ascent = ascent * scale;
    m_descent = descent * scale;

    struct Glyph {
        int c, width, height, x, y;
        unsigned char* bitmap;
    };
    std::vector<Glyph> glyphs;

    // Shelf packing; 2 px gutters keep linear filtering from bleeding between glyphs.
    // The first slot is a 2x2 white block that untextured quads sample.
    constexpr int kPad = 2;
    constexpr int atlasWidth = 1024;
    int penX = 2 + kPad, penY = 0;
    int rowHeight = 2;

    for (int c = 32; c < 127; ++c) {
        Glyph g{c, 0, 0, 0, 0, nullptr};
        int xoff = 0, yoff = 0;
        g.bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &g.width, &g.height, &xoff, &yoff);

        int advance, leftBearing;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &leftBearing);

        Character ch{};
        ch.advance = advance * scale;
        ch.bearing = glm::vec2(xoff, yoff);
        ch.size = glm::vec2(g.width, g.height);

        if (g.bitmap) {
            if (penX + g.width > atlasWidth) {
                penX = 0;
                penY += rowHeight + kPad;
                rowHeight = 0;
            }
            g.x = penX;
            g.y = penY;
            penX += g.width + kPad;
            rowHeight = std::max(rowHeight, g.height);
            glyphs.push_back(g);
        } else {
            ch.size = glm::vec2(0.0f);  // e.g. space: advance only
        }
        m_characters[static_cast<char>(c)] = ch;
        if (c == 'H') m_capHeight = static_cast<float>(-yoff);
    }

    int atlasHeight = 1;
    while (atlasHeight < penY + rowHeight) atlasHeight *= 2;

    std::vector<unsigned char> atlasData(static_cast<size_t>(atlasWidth) * atlasHeight, 0);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            atlasData[static_cast<size_t>(y) * atlasWidth + x] = 255;
    m_solidUV = glm::vec2(1.0f / atlasWidth, 1.0f / atlasHeight);

    for (const Glyph& g : glyphs) {
        for (int y = 0; y < g.height; ++y) {
            std::memcpy(&atlasData[static_cast<size_t>(g.y + y) * atlasWidth + g.x],
                        &g.bitmap[static_cast<size_t>(y) * g.width], static_cast<size_t>(g.width));
        }
        Character& ch = m_characters[static_cast<char>(g.c)];
        ch.uvMin = glm::vec2(float(g.x) / atlasWidth, float(g.y) / atlasHeight);
        ch.uvMax = glm::vec2(float(g.x + g.width) / atlasWidth, float(g.y + g.height) / atlasHeight);
        stbtt_FreeBitmap(g.bitmap, nullptr);
    }

    std::cout << "[TextRenderer] Loaded font: " << fontPath << " (" << m_characters.size() << " chars)" << std::endl;
    std::cout << "[TextRenderer] Created font atlas: " << atlasWidth << "x" << atlasHeight << std::endl;

    Buffer staging;
    if (!staging.init(device, atlasData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "[TextRenderer] Failed to create staging buffer" << std::endl;
        return false;
    }
    staging.copyData(atlasData.data(), atlasData.size());

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(atlasWidth);
    imageInfo.extent.height = static_cast<uint32_t>(atlasHeight);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_fontTexture) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to create font texture" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_fontTexture, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    uint32_t memoryTypeIndex = 0;
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
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

    if (!uploadAtlas(staging.getBuffer(), static_cast<uint32_t>(atlasWidth), static_cast<uint32_t>(atlasHeight))) {
        std::cerr << "[TextRenderer] Failed to upload font atlas" << std::endl;
        return false;
    }
    
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
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_fontSampler) != VK_SUCCESS) {
        std::cerr << "[TextRenderer] Failed to create font sampler" << std::endl;
        return false;
    }
    
    std::cout << "[TextRenderer] Font texture created successfully" << std::endl;
    
    return true;
}

bool TextRenderer::uploadAtlas(VkBuffer staging, uint32_t width, uint32_t height) {
    const uint32_t family = m_devicePtr->getQueueFamilies().graphicsFamily.value();
    VkQueue queue = m_devicePtr->getGraphicsQueue();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = family;

    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &pool) != VK_SUCCESS) return false;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    bool ok = vkAllocateCommandBuffers(m_device, &allocInfo, &cmd) == VK_SUCCESS;

    if (ok) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_fontTexture;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(cmd, staging, m_fontTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        ok = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS &&
             vkQueueWaitIdle(queue) == VK_SUCCESS;
    }

    vkDestroyCommandPool(m_device, pool, nullptr);
    return ok;
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

void TextRenderer::renderText(const std::string& text, glm::vec2 baseline, float scale,
                              glm::vec4 color, glm::vec2 pixelToNdc,
                              std::vector<UIVertex>& outVertices) const {
    // Snap the pen to whole pixels so every string samples the atlas the same way.
    float x = std::round(baseline.x);
    const float y = std::round(baseline.y);

    for (char c : text) {
        auto it = m_characters.find(c);
        if (it == m_characters.end()) continue;
        const Character& ch = it->second;

        if (ch.size.x > 0.0f) {
            const glm::vec2 p0 = (glm::vec2(x, y) + ch.bearing * scale) * pixelToNdc - 1.0f;
            const glm::vec2 p1 = p0 + ch.size * scale * pixelToNdc;
            const glm::vec2 uv0 = ch.uvMin;
            const glm::vec2 uv1 = ch.uvMax;

            outVertices.push_back({p0,                    color, uv0});
            outVertices.push_back({glm::vec2(p1.x, p0.y), color, glm::vec2(uv1.x, uv0.y)});
            outVertices.push_back({p1,                    color, uv1});

            outVertices.push_back({p0,                    color, uv0});
            outVertices.push_back({p1,                    color, uv1});
            outVertices.push_back({glm::vec2(p0.x, p1.y), color, glm::vec2(uv0.x, uv1.y)});
        }
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

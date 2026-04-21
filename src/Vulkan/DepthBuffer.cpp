#include "Vulkan/DepthBuffer.hpp"
#include "Vulkan/Device.hpp"
#include <iostream>

DepthBuffer::~DepthBuffer() noexcept {
    cleanup();
}

void DepthBuffer::cleanup() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE) {
        if (m_depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_deviceHandle, m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }
        if (m_depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_deviceHandle, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }
        if (m_depthImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_deviceHandle, m_depthImageMemory, nullptr);
            m_depthImageMemory = VK_NULL_HANDLE;
        }
    }
}

uint32_t DepthBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
}

VkFormat DepthBuffer::findDepthFormat() {
    return VK_FORMAT_D32_SFLOAT;
}

bool DepthBuffer::init(Device& device, VkExtent2D extent) noexcept {
    m_deviceHandle = device.getLogicalDevice();
    m_physicalDevice = device.getPhysicalDevice();

    VkFormat depthFormat = findDepthFormat();

    // Depth image (used as depth-stencil attachment).
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_deviceHandle, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        std::cerr << "[DepthBuffer] Failed to create depth image" << std::endl;
        return false;
    }

    // Allocate device-local memory.
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_deviceHandle, m_depthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_deviceHandle, &allocInfo, nullptr, &m_depthImageMemory) != VK_SUCCESS) {
        std::cerr << "[DepthBuffer] Failed to allocate depth image memory" << std::endl;
        return false;
    }

    vkBindImageMemory(m_deviceHandle, m_depthImage, m_depthImageMemory, 0);

    // View used in the render pass attachment.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_deviceHandle, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        std::cerr << "[DepthBuffer] Failed to create depth image view" << std::endl;
        return false;
    }

    std::cout << "[DepthBuffer] Created successfully" << std::endl;
    return true;
}

#pragma once

#include <vulkan/vulkan.h>

class Device;

class DepthBuffer {
private:
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    
    VkDevice m_deviceHandle = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findDepthFormat();

public:
    DepthBuffer() = default;
    ~DepthBuffer() noexcept;

    DepthBuffer(const DepthBuffer&) = delete;
    DepthBuffer& operator=(const DepthBuffer&) = delete;

    bool init(Device& device, VkExtent2D extent) noexcept;
    void cleanup() noexcept;

    VkImageView getImageView() const noexcept { return m_depthImageView; }
    VkFormat getFormat() const noexcept { return VK_FORMAT_D32_SFLOAT; }
};

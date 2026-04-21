#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>

class Device;

class Buffer {
private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    
    size_t m_size = 0;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

public:
    Buffer() = default;
    ~Buffer() noexcept;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool init(Device& device, 
              size_t size,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties) noexcept;

    void copyData(const void* data, size_t size);

    VkBuffer getBuffer() const noexcept { return m_buffer; }
    VkDeviceMemory getMemory() const noexcept { return m_memory; }
    size_t getSize() const noexcept { return m_size; }
};

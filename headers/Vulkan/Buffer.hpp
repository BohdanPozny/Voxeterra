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
    void* m_mapped = nullptr;

    bool findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& outIndex);

public:
    Buffer() = default;
    ~Buffer() noexcept;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool init(Device& device, 
              size_t size,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties) noexcept;

    void copyData(const void* data, size_t size, size_t offset = 0);

    // Maps the whole buffer for its lifetime (host-visible memory only).
    void* map();

    VkBuffer getBuffer() const noexcept { return m_buffer; }
    VkDeviceMemory getMemory() const noexcept { return m_memory; }
    size_t getSize() const noexcept { return m_size; }
};

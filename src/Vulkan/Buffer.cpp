#include "Vulkan/Buffer.hpp"
#include "Vulkan/Device.hpp"
#include <iostream>
#include <cstring>

Buffer::~Buffer() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE) {
        if (m_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_deviceHandle, m_buffer, nullptr);
        }
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_deviceHandle, m_memory, nullptr);
        }
    }
}

uint32_t Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::cerr << "[Buffer] Failed to find suitable memory type" << std::endl;
    return 0;
}

bool Buffer::init(Device& device,
                  size_t size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties) noexcept {
    m_deviceHandle = device.getLogicalDevice();
    m_physicalDevice = device.getPhysicalDevice();
    m_size = size;

    // Create buffer handle.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_deviceHandle, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        std::cerr << "[Buffer] Failed to create buffer" << std::endl;
        return false;
    }

    // Allocate and bind device memory.
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_deviceHandle, m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_deviceHandle, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        std::cerr << "[Buffer] Failed to allocate buffer memory" << std::endl;
        return false;
    }

    vkBindBufferMemory(m_deviceHandle, m_buffer, m_memory, 0);
    return true;
}

void Buffer::copyData(const void* data, size_t size) {
    void* mappedData;
    vkMapMemory(m_deviceHandle, m_memory, 0, size, 0, &mappedData);
    std::memcpy(mappedData, data, size);
    vkUnmapMemory(m_deviceHandle, m_memory);
}

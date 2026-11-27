#include "Vulkan/Buffer.hpp"
#include "Vulkan/Device.hpp"
#include "utils/Profiler.hpp"
#include <iostream>
#include <cstring>

[[maybe_unused]] static const char* kTracyPool = "Vulkan buffers";

Buffer::~Buffer() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE) {
        if (m_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_deviceHandle, m_buffer, nullptr);
        }
        if (m_memory != VK_NULL_HANDLE) {
            TracyFreeN(this, kTracyPool);
            vkFreeMemory(m_deviceHandle, m_memory, nullptr);
        }
    }
}

bool Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& outIndex) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            outIndex = i;
            return true;
        }
    }

    std::cerr << "[Buffer] Failed to find suitable memory type" << std::endl;
    return false;
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
    if (!findMemoryType(memRequirements.memoryTypeBits, properties, allocInfo.memoryTypeIndex)) {
        return false;
    }

    if (vkAllocateMemory(m_deviceHandle, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        std::cerr << "[Buffer] Failed to allocate buffer memory" << std::endl;
        return false;
    }

    TracyAllocN(this, allocInfo.allocationSize, kTracyPool);
    vkBindBufferMemory(m_deviceHandle, m_buffer, m_memory, 0);
    return true;
}

void* Buffer::map() {
    if (!m_mapped) vkMapMemory(m_deviceHandle, m_memory, 0, VK_WHOLE_SIZE, 0, &m_mapped);
    return m_mapped;
}

void Buffer::copyData(const void* data, size_t size, size_t offset) {
    if (m_mapped) {
        std::memcpy(static_cast<char*>(m_mapped) + offset, data, size);
        return;
    }
    void* mappedData;
    vkMapMemory(m_deviceHandle, m_memory, offset, size, 0, &mappedData);
    std::memcpy(mappedData, data, size);
    vkUnmapMemory(m_deviceHandle, m_memory);
}

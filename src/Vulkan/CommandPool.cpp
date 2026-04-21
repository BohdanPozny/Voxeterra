#include "Vulkan/CommandPool.hpp"
#include <iostream>

CommandPool::~CommandPool() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE) {
        // Destroying the pool also frees its command buffers.
        vkDestroyCommandPool(m_deviceHandle, m_commandPool, nullptr);
    }
}

bool CommandPool::init(VkDevice device, uint32_t queueFamilyIndex) noexcept {
    m_deviceHandle = device;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_deviceHandle, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::cerr << "[CommandPool] Failed to create command pool" << std::endl;
        return false;
    }

    std::cout << "[CommandPool] Created successfully" << std::endl;
    return true;
}

bool CommandPool::allocateCommandBuffers(uint32_t count) noexcept {
    m_commandBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(m_deviceHandle, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "[CommandPool] Failed to allocate command buffers" << std::endl;
        return false;
    }

    std::cout << "[CommandPool] Allocated " << count << " command buffers" << std::endl;
    return true;
}

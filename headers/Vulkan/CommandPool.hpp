#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class CommandPool {
private:
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

public:
    CommandPool() = default;
    ~CommandPool() noexcept;

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    bool init(VkDevice device, uint32_t queueFamilyIndex) noexcept;
    bool allocateCommandBuffers(uint32_t count) noexcept;

    VkCommandPool getPool() const noexcept { return m_commandPool; }
    const std::vector<VkCommandBuffer>& getCommandBuffers() const noexcept { return m_commandBuffers; }
    VkCommandBuffer getCommandBuffer(size_t index) const noexcept { return m_commandBuffers[index]; }
};

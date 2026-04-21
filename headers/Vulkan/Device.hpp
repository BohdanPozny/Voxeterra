#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <cstdint>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class Device {
private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueIndices;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    bool pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) noexcept;

public:
    Device() = default;
    ~Device() noexcept;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool init(VkInstance instance, VkSurfaceKHR surface) noexcept;

    VkDevice getLogicalDevice() noexcept { return m_device; }
    VkPhysicalDevice getPhysicalDevice() noexcept { return m_physicalDevice; }

    QueueFamilyIndices getQueueFamilies() const noexcept { return m_queueIndices; }
    
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
};

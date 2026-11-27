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

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    // Persisted between runs: MoltenVK translates SPIR-V to Metal and compiles pipelines
    // on first use, which dominates start-up on macOS.
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    void createPipelineCache();

    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    int rateDevice(VkPhysicalDevice device) const;
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
    VkPipelineCache getPipelineCache() const { return m_pipelineCache; }
    // Writes the cache next to the executable; call once every pipeline exists.
    void savePipelineCache();
};

#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>

class Window;
class Device;

class Swapchain {
private:
    VkSwapchainKHR m_swapchain;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkFormat m_imageFormat;
    VkExtent2D m_extent;

    VkDevice m_deviceHandle;

    VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR>& avaibleFormats);
    VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR>& avaiblePresetModes);
    VkExtent2D chooseExtent(VkSurfaceCapabilitiesKHR& capabilities, Window& window);

    void createImageViews();

public:
    Swapchain() = default;
    ~Swapchain() noexcept;

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    bool init(Device& device, Window& window, VkSurfaceKHR m_surface) noexcept;
    void cleanup() noexcept;

    VkSwapchainKHR getSwapchain() { return m_swapchain; }
    VkFormat getFormat() { return m_imageFormat; }
    VkExtent2D getExtent() { return m_extent; }
    const std::vector<VkImageView>& getImageViews() { return m_imageViews; }
};

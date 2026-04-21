#include "Vulkan/Swapchain.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "Vulkan/Device.hpp"
#include "Window.hpp"

Swapchain::~Swapchain() noexcept {
    cleanup();
}

void Swapchain::cleanup() noexcept {
    if (m_deviceHandle != VK_NULL_HANDLE) {
        for (auto imageView:m_imageViews) {
            vkDestroyImageView(m_deviceHandle, imageView, nullptr);
        }
        m_imageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_deviceHandle, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
        
        m_deviceHandle = VK_NULL_HANDLE;
    }
}

bool Swapchain::init(Device& device, Window& window, VkSurfaceKHR m_surface) noexcept {
    m_deviceHandle = device.getLogicalDevice();

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.getPhysicalDevice(), m_surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.getPhysicalDevice(), m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.getPhysicalDevice(), m_surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.getPhysicalDevice(), m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.getPhysicalDevice(), m_surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = choosePresentMode(presentModes);
    VkExtent2D extent = chooseExtent(capabilities, window);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (imageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_deviceHandle, &createInfo, nullptr, &m_swapchain)) {
        std::cerr << "[Swapchain] Not create VkSwapchainKHR" << std::endl;
        return false;
    }

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    vkGetSwapchainImagesKHR(m_deviceHandle, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_deviceHandle, m_swapchain, &imageCount, m_images.data());

    createImageViews();

    return true;
}

VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR>& avaibleFormats) {
    for (const auto& avaibleFormat:avaibleFormats) {
        if (avaibleFormat.format == VK_FORMAT_B8G8R8A8_SRGB && avaibleFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            return avaibleFormat;
        }
    }

    return avaibleFormats[0];
}

VkPresentModeKHR Swapchain::choosePresentMode(std::vector<VkPresentModeKHR>& avaiblePresentModes) {
    for (const auto& avaiblePresentMode:avaiblePresentModes) {
        if (avaiblePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return avaiblePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseExtent(VkSurfaceCapabilitiesKHR& capabilities, Window& window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        window.getFramebufferSize(&width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_deviceHandle, &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            std::cerr << "[Swapchain] Not create ImageView" << std::endl;
        }
    }
}


# Vulkan Swapchain

## Огляд

`Swapchain` — це ланцюг зображень (images) для відображення на екрані. Відповідає за:
- Створення `VkSwapchainKHR`
- Управління image views
- Вибір формату, режиму відображення та розміру

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Device;

class Swapchain {
public:
    Swapchain() = default;
    ~Swapchain();

    bool init(Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void cleanup();
    void recreate(Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height);

    // Getters
    VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    VkFormat getImageFormat() const { return m_imageFormat; }
    VkExtent2D getExtent() const { return m_extent; }
    size_t getImageCount() const { return m_images.size(); }
    VkImageView getImageView(size_t index) const { return m_imageViews[index]; }
    uint32_t acquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex);

private:
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkDevice m_device = VK_NULL_HANDLE;

    // Helper methods
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t width, uint32_t height);
};
```

## Структури підтримки Swapchain

```cpp
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails Swapchain::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    
    // Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    
    // Formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }
    
    // Present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    
    return details;
}
```

## Вибір Surface Format

```cpp
VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    
    // Шукаємо B8G8R8A8_UNORM + SRGB_NONLINEAR
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    
    // Fallback: перший доступний
    return formats[0];
}
```

## Вибір Present Mode

```cpp
VkPresentModeKHR Swapchain::choosePresentMode(
    const std::vector<VkPresentModeKHR>& modes) {
    
    // Пріоритет: MAILBOX (low latency, no tearing) > FIFO (VSync)
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;  // Трійна буферизація, низька затримка
        }
    }
    
    // FIFO гарантовано підтримується (VSync)
    return VK_PRESENT_MODE_FIFO_KHR;
}
```

| Mode | Опис |
|------|------|
| `VK_PRESENT_MODE_IMMEDIATE_KHR` | Негайно, можливий tearing |
| `VK_PRESENT_MODE_FIFO_KHR` | VSync, черга з 1-3 зображеннями |
| `VK_PRESENT_MODE_FIFO_RELAXED_KHR` | VSync зі зниженням до IMMEDIATE якщо відстаємо |
| `VK_PRESENT_MODE_MAILBOX_KHR` | Трійна буферизація, низька затримка |

## Вибір Extent (розміру)

```cpp
VkExtent2D Swapchain::chooseExtent(
    const VkSurfaceCapabilitiesKHR& caps, uint32_t width, uint32_t height) {
    
    // Якщо currentExtent визначено — використовуємо його
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    
    // Інакше — обмежуємо запитаний розмір
    VkExtent2D actualExtent = {width, height};
    
    actualExtent.width = std::clamp(
        actualExtent.width,
        caps.minImageExtent.width,
        caps.maxImageExtent.width
    );
    actualExtent.height = std::clamp(
        actualExtent.height,
        caps.minImageExtent.height,
        caps.maxImageExtent.height
    );
    
    return actualExtent;
}
```

## Створення Swapchain

```cpp
bool Swapchain::init(Device& device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    m_device = device.getLogicalDevice();
    
    SwapChainSupportDetails details = querySwapChainSupport(
        device.getPhysicalDevice(), surface);
    
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(details.formats);
    VkPresentModeKHR presentMode = choosePresentMode(details.presentModes);
    VkExtent2D extent = chooseExtent(details.capabilities, width, height);
    
    // Кількість зображень (потрійна буферизація якщо можливо)
    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && 
        imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    // Queue families (graphics vs present)
    uint32_t queueFamilyIndices[] = {
        device.getGraphicsQueueFamily(),
        device.getPresentQueueFamily()
    };
    
    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        // Різні черги — concurrent sharing mode
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // Одна черга — exclusive (краща продуктивність)
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    createInfo.preTransform = details.capabilities.currentTransform;  // No rotation
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;   // No blending with window
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;  // Дозволяємо відсікати приховані пікселі
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
    
    // Отримуємо зображення
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_images.data());
    
    // Створюємо image views
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews[i]);
    }
    
    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
    
    return true;
}
```

## Отримання наступного зображення

```cpp
uint32_t Swapchain::acquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex) {
    VkResult result = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,           // timeout (безкінечність)
        signalSemaphore,      // сигналізує коли image готовий
        VK_NULL_HANDLE,       // fence (не використовуємо)
        &imageIndex           // output: індекс зображення
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return VK_ERROR_OUT_OF_DATE_KHR;  // Потрібен recreate
    }
    
    return VK_SUCCESS;
}
```

## Cleanup

```cpp
void Swapchain::cleanup() {
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_imageViews.clear();
    
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}
```

## Використання в Engine

```cpp
// Engine::drawFrame()
uint32_t imageIndex;
VkResult result = m_swapchain.acquireNextImage(
    frameSync.getImageAvailableSemaphore(currentFrame), 
    imageIndex
);

// Рендеринг у framebuffer[imageIndex]
// ...

// Подання результату
presentInfo.pImageIndices = &imageIndex;
vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);
```

## Дивіться також

- [[Device\|Device]] — надає queues для swapchain
- [[Framebuffer\|Framebuffer]] — використовує image views
- [[RenderPass\|RenderPass]] — рендерить у swapchain images

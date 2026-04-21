# Vulkan DepthBuffer

## Огляд

`DepthBuffer` управляє Z-buffer для depth testing. Створює Vulkan image, виділяє пам'ять та створює image view для використання як depth attachment.

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>

class Device;

class DepthBuffer {
public:
    DepthBuffer() = default;
    ~DepthBuffer();

    bool init(Device& device, VkExtent2D extent);
    void cleanup();

    VkImageView getImageView() const { return m_imageView; }
    VkFormat getFormat() const { return m_format; }

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkDevice m_device = VK_NULL_HANDLE;

    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                                  const std::vector<VkFormat>& candidates,
                                  VkImageTiling tiling,
                                  VkFormatFeatureFlags features);
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                            uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);
};
```

## Формати Depth Buffer

| Формат | Опис | Використання |
|--------|------|--------------|
| `VK_FORMAT_D32_SFLOAT` | 32-bit float depth | Повна точність, стандарт для Voxeterra |
| `VK_FORMAT_D24_UNORM_S8_UINT` | 24-bit depth + 8-bit stencil | З stencil, якщо потрібно |
| `VK_FORMAT_D16_UNORM` | 16-bit depth | Менша точність, економія пам'яті |

## Ініціалізація

```cpp
bool DepthBuffer::init(Device& device, VkExtent2D extent) {
    m_device = device.getLogicalDevice();
    
    // Вибираємо оптимальний формат
    m_format = findDepthFormat(device.getPhysicalDevice());
    
    // Створюємо image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    vkCreateImage(m_device, &imageInfo, nullptr, &m_image);
    
    // Виділяємо пам'ять
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        device.getPhysicalDevice(),
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT  // GPU-only memory
    );
    
    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory);
    vkBindImageMemory(m_device, m_image, m_memory, 0);
    
    // Створюємо image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView);
    
    return true;
}
```

## Пошук підтримуваного формату

```cpp
VkFormat DepthBuffer::findSupportedFormat(VkPhysicalDevice physicalDevice,
                                          const std::vector<VkFormat>& candidates,
                                          VkImageTiling tiling,
                                          VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        
        if (tiling == VK_IMAGE_TILING_LINEAR && 
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && 
                   (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    
    throw std::runtime_error("Failed to find supported format!");
}

VkFormat DepthBuffer::findDepthFormat(VkPhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}
```

## Cleanup

```cpp
DepthBuffer::~DepthBuffer() {
    cleanup();
}

void DepthBuffer::cleanup() {
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}
```

## Lifecycle

```
Engine::init()
  └── DepthBuffer::init()
        ├── vkCreateImage()
        ├── vkAllocateMemory()
        ├── vkBindImageMemory()
        └── vkCreateImageView()

Engine::cleanup()
  └── DepthBuffer::cleanup() (зворотний порядок)
        ├── vkDestroyImageView()
        ├── vkDestroyImage()
        └── vkFreeMemory()
```

## Використання в Render Pass

```cpp
// Очистка depth на початку render pass
VkClearValue depthClear;
depthClear.depthStencil = {1.0f, 0};  // far plane, stencil 0

renderPassInfo.pClearValues = clearValues;
```

## Дивіться також

- [[RenderPass\|RenderPass]] — використовує depth attachment
- [[Framebuffer\|Framebuffer]] — прив'язує depth image view

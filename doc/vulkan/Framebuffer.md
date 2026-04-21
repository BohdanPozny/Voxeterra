# Vulkan Framebuffer

## Огляд

`Framebuffer` зв'язує `VkImageView` (з swapchain та depth buffer) з `VkRenderPass`. Для кожного swapchain image створюється окремий framebuffer.

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    bool init(VkDevice device, VkRenderPass renderPass,
              const std::vector<VkImageView>& swapchainImageViews,
              VkImageView depthImageView,
              VkExtent2D extent);
    void cleanup();

    VkFramebuffer getFramebuffer(size_t index) const { return m_framebuffers[index]; }
    size_t getCount() const { return m_framebuffers.size(); }

private:
    std::vector<VkFramebuffer> m_framebuffers;
    VkDevice m_device = VK_NULL_HANDLE;
};
```

## Структура Framebuffer

```
Framebuffer (per swapchain image):
  ├── Attachment 0: Swapchain image view (color)
  └── Attachment 1: Depth image view (depth/stencil)
```

## Ініціалізація

```cpp
bool Framebuffer::init(VkDevice device, VkRenderPass renderPass,
                       const std::vector<VkImageView>& swapchainImageViews,
                       VkImageView depthImageView,
                       VkExtent2D extent) {
    m_device = device;
    m_framebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapchainImageViews[i],  // Color
            depthImageView            // Depth
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;  // Сумісний render pass
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(
            device, &framebufferInfo, nullptr, &m_framebuffers[i]
        );
        
        if (result != VK_SUCCESS) {
            return false;
        }
    }
    
    return true;
}
```

## Параметри створення

| Параметр | Опис |
|----------|------|
| `renderPass` | Сумісний render pass (має ті ж attachments) |
| `attachments` | Масив image views (порядок має відповідати render pass) |
| `width/height` | Розміри фреймбуфера |
| `layers` | Кількість шарів (1 для 2D рендерингу) |

## Cleanup

```cpp
Framebuffer::~Framebuffer() {
    cleanup();
}

void Framebuffer::cleanup() {
    for (auto framebuffer : m_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();
}
```

## Використання в Engine

```cpp
// Engine::drawFrame()
VkRenderPassBeginInfo renderPassInfo{};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
renderPassInfo.renderPass = m_renderPass.getRenderPass();
renderPassInfo.framebuffer = m_framebuffer.getFramebuffer(imageIndex);
renderPassInfo.renderArea.offset = {0, 0};
renderPassInfo.renderArea.extent = m_swapchain.getExtent();

vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

// ... rendering commands ...

vkCmdEndRenderPass(commandBuffer);
```

## Залежності

```
Framebuffer створюється після:
  ├── Swapchain (потрібні image views)
  ├── DepthBuffer (потрібен depth image view)
  └── RenderPass (для сумісності attachments)

Framebuffer використовується при:
  └── Recording command buffer (vkCmdBeginRenderPass)
```

## Дивіться також

- [[RenderPass\|RenderPass]] — render pass для сумісності
- [[Swapchain\|Swapchain]] — source of color attachments
- [[DepthBuffer\|DepthBuffer]] — source of depth attachment

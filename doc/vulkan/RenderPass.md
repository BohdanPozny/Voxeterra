# Vulkan RenderPass

## Огляд

`RenderPass` описує структуру та залежності операцій рендерингу. Визначає:
- Attachments (color buffer, depth buffer)
- Subpasses (етапи рендерингу)
- Dependencies (синхронізація між сабпасами)

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>

class RenderPass {
public:
    RenderPass() = default;
    ~RenderPass();

    bool init(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void cleanup();
    
    VkRenderPass getRenderPass() const { return m_renderPass; }

private:
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
```

## Структура RenderPass

### Attachments

| Attachment | Формат | Використання | Опис |
|------------|--------|--------------|------|
| Color | `VK_FORMAT_B8G8R8A8_UNORM` | `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` | Кінцевий колір для swapchain |
| Depth | `VK_FORMAT_D32_SFLOAT` | `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | Z-buffer для depth test |

### Subpass

```
Subpass 0:
  ├── Color attachment: attachment 0 (color)
  └── Depth attachment: attachment 1 (depth)
```

## Ініціалізація

```cpp
bool RenderPass::init(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    m_device = device;
    
    // Attachment 0: Color
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // No MSAA
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Очистити на початку
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Зберегти для подання
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // Попередній вміст не важливий
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Готове до presentation
    
    // Attachment 1: Depth
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Очистити depth
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // Не зберігати (тільки для тестування)
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Attachment references for subpass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;  // Index в attachments array
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    // Subpass dependencies (синхронізація)
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;  // Попередні команди (зовнішні)
    dependency.dstSubpass = 0;  // Наш сабпас
    
    // Від чого чекаємо
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    
    // Що блокуємо
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    // Create render pass
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass);
    return result == VK_SUCCESS;
}
```

## Load/Store Operations

| Операція | Опис |
|----------|------|
| `LOAD_OP_CLEAR` | Очистити attachment перед використанням |
| `LOAD_OP_LOAD` | Завантажити існуючий вміст |
| `LOAD_OP_DONT_CARE` | Вміст не визначений (швидше) |
| `STORE_OP_STORE` | Зберегти вміст для подальшого використання |
| `STORE_OP_DONT_CARE` | Вміст буде втрачено (швидше) |

## Image Layouts

| Layout | Призначення |
|--------|-------------|
| `UNDEFINED` | Початковий стан, вміст не валідний |
| `COLOR_ATTACHMENT_OPTIMAL` | Оптимально для color attachment |
| `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | Оптимально для depth attachment |
| `PRESENT_SRC_KHR` | Готове для presentation на екрані |
| `TRANSFER_DST_OPTIMAL` | Оптимально для запису (копіювання) |

## Cleanup

```cpp
RenderPass::~RenderPass() {
    cleanup();
}

void RenderPass::cleanup() {
    if (m_renderPass != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}
```

## Використання в Engine

```cpp
// Engine::recordCommandBuffer()
VkRenderPassBeginInfo renderPassInfo{};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
renderPassInfo.renderPass = m_renderPass.getRenderPass();
renderPassInfo.framebuffer = m_framebuffer.getFramebuffer(imageIndex);
renderPassInfo.renderArea.offset = {0, 0};
renderPassInfo.renderArea.extent = m_swapchain.getExtent();

// Clear values
std::array<VkClearValue, 2> clearValues{};
clearValues[0].color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
clearValues[1].depthStencil = {1.0f, 0};  // Clear depth to 1.0 (far plane)

renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
renderPassInfo.pClearValues = clearValues.data();

vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

// ... draw commands ...

vkCmdEndRenderPass(commandBuffer);
```

## Multiple Subpasses (Advanced)

Для складніших ефектів можна мати кілька сабпасів:

```
Subpass 0: Deferred G-buffer generation
  └── Outputs: position, normal, albedo attachments

Subpass 1: Lighting calculation
  └── Reads: G-buffer attachments
  └── Output: final color

Subpass 2: Post-processing
  └── Input: final color
  └── Output: presentable image
```

Voxeterra використовує **single subpass** (простий forward rendering).

## Дивіться також

- [[Framebuffer\|Framebuffer]] — створюється для конкретного render pass
- [[Pipeline\|Pipeline]] — прив'язується до render pass при створенні
- [[DepthBuffer\|DepthBuffer]] — depth attachment для render pass

# Vulkan FrameSync

## Огляд

`FrameSync` управляє синхронізацією між CPU та GPU. Використовує semaphores та fences для координації доступу до ресурсів при подвійній буферизації (max_frames_in_flight = 2).

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class FrameSync {
public:
    FrameSync() = default;
    ~FrameSync();

    bool init(VkDevice device, uint32_t maxFramesInFlight);
    void cleanup();

    // Getters для поточного кадру
    VkSemaphore getImageAvailableSemaphore(uint32_t frame) const { 
        return m_imageAvailableSemaphores[frame]; 
    }
    VkSemaphore getRenderFinishedSemaphore(uint32_t frame) const { 
        return m_renderFinishedSemaphores[frame]; 
    }
    VkFence getInFlightFence(uint32_t frame) const { 
        return m_inFlightFences[frame]; 
    }

    // Синхронізація
    void waitForFence(uint32_t frame) const;
    void resetFence(uint32_t frame) const;

private:
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_maxFramesInFlight = 0;
};
```

## Синхронізаційні примітиви Vulkan

### Semaphores (GPU-GPU)

| Тип | Призначення |
|-----|-------------|
| `Image Available` | GPU сигналізує коли swapchain image готовий для рендерингу |
| `Render Finished` | GPU сигналізує коли рендеринг завершено, можна подавати |

### Fences (GPU-CPU)

| Тип | Призначення |
|-----|-------------|
| `In Flight` | CPU чекає завершення GPU перед початком нового кадру |

## Ініціалізація

```cpp
bool FrameSync::init(VkDevice device, uint32_t maxFramesInFlight) {
    m_device = device;
    m_maxFramesInFlight = maxFramesInFlight;

    m_imageAvailableSemaphores.resize(maxFramesInFlight);
    m_renderFinishedSemaphores.resize(maxFramesInFlight);
    m_inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Початково "відкритий"

    for (uint32_t i = 0; i < maxFramesInFlight; i++) {
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, 
                          &m_imageAvailableSemaphores[i]);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, 
                          &m_renderFinishedSemaphores[i]);
        vkCreateFence(device, &fenceInfo, nullptr, 
                      &m_inFlightFences[i]);
    }

    return true;
}
```

## Frame Flow

```
Кадр 0:                                              Кадр 1:
┌──────────────────┐                                ┌──────────────────┐
│ Wait Fence[0]    │                                │ Wait Fence[1]    │
│ (CPU чекає GPU)  │                                │ (CPU чекає GPU)  │
└────────┬─────────┘                                └────────┬─────────┘
         │                                                │
         ▼                                                ▼
┌──────────────────┐                                ┌──────────────────┐
│ Acquire Image    │                                │ Acquire Image    │
│ signal: Sem[0]   │                                │ signal: Sem[1]   │
└────────┬─────────┘                                └────────┬─────────┘
         │                                                │
         ▼                                                ▼
┌──────────────────┐                                ┌──────────────────┐
│ Record & Submit  │                                │ Record & Submit  │
│ wait: Sem[0]     │                                │ wait: Sem[1]     │
│ signal: Sem[0]  │                                │ signal: Sem[1]  │
│ fence: Fence[0] │                                │ fence: Fence[1] │
└────────┬─────────┘                                └────────┬─────────┘
         │                                                │
         ▼                                                ▼
┌──────────────────┐                                ┌──────────────────┐
│ Present          │                                │ Present          │
│ wait: Sem[0]     │                                │ wait: Sem[1]     │
└──────────────────┘                                └──────────────────┘
```

## Методи синхронізації

```cpp
void FrameSync::waitForFence(uint32_t frame) const {
    // CPU блокується поки GPU не завершить кадр
    vkWaitForFences(m_device, 1, &m_inFlightFences[frame], VK_TRUE, UINT64_MAX);
}

void FrameSync::resetFence(uint32_t frame) const {
    // Скидаємо fence для наступного використання
    vkResetFences(m_device, 1, &m_inFlightFences[frame]);
}
```

## Cleanup

```cpp
FrameSync::~FrameSync() {
    cleanup();
}

void FrameSync::cleanup() {
    for (size_t i = 0; i < m_maxFramesInFlight; i++) {
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        }
        if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
    }
}
```

## Використання в Engine

```cpp
void Engine::drawFrame() {
    uint32_t currentFrame = m_currentFrame;
    
    // 1. Чекаємо завершення попереднього кадру з цим індексом
    m_frameSync.waitForFence(currentFrame);
    m_frameSync.resetFence(currentFrame);
    
    // 2. Отримуємо наступне зображення
    uint32_t imageIndex;
    m_swapchain.acquireNextImage(
        m_frameSync.getImageAvailableSemaphore(currentFrame), 
        imageIndex
    );
    
    // 3. Записуємо команди
    VkCommandBuffer cmdBuffer = recordCommandBuffer(imageIndex);
    
    // 4. Відправляємо на виконання
    VkSubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_frameSync.getImageAvailableSemaphore(currentFrame);
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_frameSync.getRenderFinishedSemaphore(currentFrame);
    
    vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, 
                  m_frameSync.getInFlightFence(currentFrame));
    
    // 5. Подаємо результат
    VkPresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_frameSync.getRenderFinishedSemaphore(currentFrame);
    // ... swapchain, imageIndex ...
    vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);
    
    // 6. Переходимо до наступного кадру
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

## Дивіться також

- [[Swapchain\|Swapchain]] — acquireNextImage використовує semaphore
- [[Device\|Device]] — надає queues для submit

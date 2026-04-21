# Vulkan CommandPool

## Огляд

`CommandPool` управляє виділенням командних буферів (`VkCommandBuffer`). Командні буфери записують команди рендерингу, які потім відправляються на GPU.

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Device;

class CommandPool {
public:
    CommandPool() = default;
    ~CommandPool();

    bool init(Device& device, uint32_t queueFamilyIndex, uint32_t bufferCount);
    void cleanup();

    VkCommandBuffer getCommandBuffer(uint32_t index) const { return m_commandBuffers[index]; }
    VkCommandPool getPool() const { return m_commandPool; }
    uint32_t getBufferCount() const { return static_cast<uint32_t>(m_commandBuffers.size()); }

    void resetPool();

private:
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkDevice m_device = VK_NULL_HANDLE;
};
```

## Ініціалізація

```cpp
bool CommandPool::init(Device& device, uint32_t queueFamilyIndex, uint32_t bufferCount) {
    m_device = device.getLogicalDevice();

    // Створюємо command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;  // Graphics queue family
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // Дозволяємо перезапис

    VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        return false;
    }

    // Виділяємо command buffers
    m_commandBuffers.resize(bufferCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // Primary level (can be submitted)
    allocInfo.commandBufferCount = bufferCount;

    result = vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data());
    return result == VK_SUCCESS;
}
```

## Flags Command Pool

| Flag | Опис |
|------|------|
| `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT` | Командні буфери часто перестворюються |
| `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` | Дозволяє перезапис окремих буферів |
| `VK_COMMAND_POOL_CREATE_PROTECTED_BIT` | Protected memory (для DRM) |

## Рівні Command Buffer

| Рівень | Призначення |
|--------|-------------|
| `PRIMARY` | Можуть бути відправлені на виконання |
| `SECONDARY` | Можуть бути виконані з primary buffer (використовуються для multithreading) |

Voxeterra використовує **PRIMARY** рівень для простоти.

## Використання в Engine

```cpp
// Ініціалізація (MAX_FRAMES_IN_FLIGHT = 2 буфери)
m_commandPool.init(m_device, m_device.getGraphicsQueueFamily(), MAX_FRAMES_IN_FLIGHT);

// Кожен кадр: записуємо команди
VkCommandBuffer cmdBuffer = m_commandPool.getCommandBuffer(currentFrame);

vkResetCommandBuffer(cmdBuffer, 0);

VkCommandBufferBeginInfo beginInfo{};
beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Один раз виконати

vkBeginCommandBuffer(cmdBuffer, &beginInfo);

// ... записуємо команди рендерингу ...

vkEndCommandBuffer(cmdBuffer);
```

## Reset Pool

```cpp
void CommandPool::resetPool() {
    // Скидаємо всі command buffers в пулі одночасно (швидше ніж окремо)
    vkResetCommandPool(m_device, m_commandPool, 0);
}
```

## Cleanup

```cpp
CommandPool::~CommandPool() {
    cleanup();
}

void CommandPool::cleanup() {
    // Command buffers звільняються автоматично з пулом
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}
```

## Потік команд

```
CPU (Recording)
  ├── vkBeginCommandBuffer()
  ├── vkCmdBeginRenderPass()
  ├── vkCmdBindPipeline()
  ├── vkCmdBindVertexBuffer()
  ├── vkCmdDraw() / vkCmdDrawIndexed()
  ├── vkCmdEndRenderPass()
  └── vkEndCommandBuffer()
          │
          ▼
GPU (Submit)
  ├── vkQueueSubmit()
  │       └── Command buffer виконується
  └── vkQueuePresentKHR()
```

## Multithreading

Для багатопоточного запису команд можна використовувати:
- **Secondary command buffers** — кожен потік записує свій secondary buffer
- **Multiple command pools** — один пул на потік
- **vkCmdExecuteCommands()** — primary buffer виконує secondary buffers

Voxeterra використовує **single-threaded** запис для простоти.

## Дивіться також

- [[Device\|Device]] — надає queue family index
- [[FrameSync\|FrameSync]] — синхронізація виконання команд

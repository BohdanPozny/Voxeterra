# Vulkan Підсистема

## Огляд

Vulkan підсистема Voxeterra — це набір C++ обгорток (wrappers) навколо Vulkan API, що реалізують RAII підхід до управління ресурсами GPU.

## Структура модулів

```
vulkan/
├── [[Instance\|Instance]]       # VkInstance (точка входу Vulkan)
├── [[Device\|Device]]          # GPU selection, VkDevice, queues
├── [[Swapchain\|Swapchain]]     # VkSwapchainKHR, image views
├── [[RenderPass\|RenderPass]]   # VkRenderPass (color + depth attachments)
├── [[Framebuffer\|Framebuffer]] # VkFramebuffer per swapchain image
├── [[DepthBuffer\|DepthBuffer]] # Z-buffer для depth testing
├── [[CommandPool\|CommandPool]] # VkCommandPool + VkCommandBuffer[]
├── [[FrameSync\|FrameSync]]     # Semaphores + Fences для синхронізації
├── [[Pipeline\|Pipeline]]      # 3D voxel graphics pipeline
├── [[Pipeline2D\|Pipeline2D]]   # 2D UI graphics pipeline
├── [[Shader\|Shader]]           # SPIR-V shader module loader
├── [[Buffer\|Buffer]]           # VkBuffer + VkDeviceMemory wrapper
├── [[TextRenderer\|TextRenderer]] # Bitmap + TTF font rendering
└── [[WorldRenderer\|WorldRenderer]] # High-level voxel world rendering
```

## Ієрархія ініціалізації

Порядок створення Vulkan об'єктів критичний — кожен наступний залежить від попередніх:

```
1. Instance ──> створює VkInstance
       │
2. Window ──> створює VkSurfaceKHR (потребує Instance)
       │
3. Device ──> вибирає GPU з підтримкою surface (потребує Instance + Surface)
       │
4. Swapchain ──> створює ланцюг зображень (потребує Device + Surface)
       │
5. DepthBuffer ──> Z-buffer (потребує Device + Swapchain extent)
       │
6. RenderPass ──> визначає attachments (потребує Device + знає про depth)
       │
7. Framebuffer ──> прив'язує image views до render pass
       │            (потребує Swapchain + RenderPass + DepthBuffer)
       │
8. CommandPool ──> пул командних буферів (потребує Device)
       │
9. FrameSync ──> semaphores + fences (потребує Device)
       │
10. Pipeline ──> graphics pipeline (потребує Device + RenderPass + shaders)
```

## Cleanup порядок (зворотний)

```
Pipeline ──> Shader modules
FrameSync ──> Semaphores, Fences
CommandPool ──> Command buffers, Pool
Framebuffer ──> Framebuffers
RenderPass ──> Render pass
DepthBuffer ──> Image view, Image, Memory
Swapchain ──> Image views, Swapchain
Device ──> Logical device
Instance ──> Instance + Surface
Window ──> GLFW window
```

## Frame Rendering Loop

```
drawFrame():
    │
    ├── waitForFence(frameSync[currentFrame]) ──> CPU чекає GPU
    │
    ├── acquireNextImageKHR(swapchain, imageAvailableSemaphore)
    │   └── GPU сигналізує коли image готовий
    │
    ├── recordCommandBuffer(cmdBuffer, imageIndex):
    │   ├── vkCmdBeginRenderPass(framebuffer[imageIndex])
    │   │
    │   ├── WorldRenderer::recordDraw():
    │   │   ├── vkCmdBindPipeline(voxelPipeline)
    │   │   ├── vkCmdBindDescriptorSets(MVP UBO)
    │   │   ├── vkCmdBindVertexBuffer(chunkVB)
    │   │   ├── vkCmdBindIndexBuffer(chunkIB)
    │   │   └── vkCmdDrawIndexed()
    │   │
    │   ├── UIRenderer::renderUI():
    │   │   ├── vkCmdBindPipeline(uiPipeline)
    │   │   └── vkCmdDraw() ──> quads для UI
    │   │
    │   └── vkCmdEndRenderPass()
    │
    ├── submitCommandBuffer():
    │   ├── waitSemaphore: imageAvailable
    │   ├── signalSemaphore: renderFinished
    │   └── signalFence: inFlightFence
    │
    └── presentKHR(renderFinishedSemaphore)
```

## Vulkan Extensions

### Обов'язкові (Required)

| Extension | Призначення |
|-----------|-------------|
| `VK_KHR_surface` | Платформо-незалежна surface абстракція |
| `VK_KHR_swapchain` | Ланцюг зображень для відображення |

### GLFW Extensions (Auto)

```cpp
uint32_t glfwExtensionCount = 0;
const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
// Зазвичай: VK_KHR_surface + VK_KHR_xcb_surface (Linux) або VK_KHR_win32_surface (Windows)
```

## Queue Families

```cpp
// Voxeterra використовує single queue family:
struct QueueFamilyIndices {
    uint32_t graphicsAndPresent;  // Graphics + Presentation (combined)
    bool found = false;
};

// Критерії вибору GPU:
// 1. Підтримка graphics + present в одному family
// 2. Підтримка swapchain extension
```

## Swapchain Configuration

| Параметр | Значення | Опис |
|----------|----------|------|
| Surface format | `VK_FORMAT_B8G8R8A8_UNORM` | 8-біт на канал, sRGB |
| Color space | `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` | Стандартний sRGB |
| Present mode | `VK_PRESENT_MODE_FIFO_KHR` | VSync enabled |
| Image count | `minImageCount + 1` | Потрійна буферизація |

## Depth Buffer

```
Format: VK_FORMAT_D32_SFLOAT
        (32-bit float depth, опціонально: VK_FORMAT_D24_UNORM_S8_UINT)

Використання:
- Depth testing при рендерингу вокселів
- Z-buffer для правильного порядку відображення
```

## Descriptor Sets

### MVP Uniform Buffer (3D Pipeline)

```cpp
// Binding 0: MVP matrix
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
```

### Оновлення кожен кадр

```cpp
void WorldRenderer::updateUniformBuffer(uint32_t currentImage) {
    // CPU оновлює UBO дані
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    
    // Копіюємо в GPU-visible memory
    memcpy(uniformBuffers[currentImage].mappedMemory, &ubo, sizeof(ubo));
}
```

## Vertex Formats

### 3D Voxel Vertex

```cpp
struct VoxelVertex {
    glm::vec3 position;  // Location 0
    glm::vec3 color;     // Location 1
    // float padding;   // Alignment (if needed)
};

// Vertex Input Binding:
// - Binding: 0
// - Stride: sizeof(VoxelVertex)
// - InputRate: VK_VERTEX_INPUT_RATE_VERTEX
```

### 2D UI Vertex

```cpp
struct UIVertex {
    glm::vec2 position;  // Location 0 (screen space: -1 to 1)
    glm::vec4 color;     // Location 1 (RGBA)
    glm::vec2 texCoord;  // Location 2 (0-1, 0,0 = no texture)
};
```

## Pipeline States

### 3D Voxel Pipeline

| State | Значення |
|-------|----------|
| Primitive topology | `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` |
| Polygon mode | `VK_POLYGON_MODE_FILL` |
| Culling | `VK_CULL_MODE_BACK_BIT` |
| Front face | `VK_FRONT_FACE_COUNTER_CLOCKWISE` |
| Depth test | `VK_TRUE` (`VK_COMPARE_OP_LESS`) |
| Depth write | `VK_TRUE` |
| Blending | Alpha blending для прозорих вокселів |

### 2D UI Pipeline

| State | Значення |
|-------|----------|
| Primitive topology | `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` |
| Polygon mode | `VK_POLYGON_MODE_FILL` |
| Culling | `VK_CULL_MODE_NONE` (вимкнено) |
| Depth test | `VK_FALSE` (UI поверх усього) |
| Blending | Alpha blending обов'язкова |

## Shader Files

```
shaders/
├── voxel.vert      # 3D vertex shader (MVP transform)
├── voxel.frag      # 3D fragment shader (color output)
├── ui.vert         # 2D vertex shader (screen space)
├── ui.frag         # 2D fragment shader (color + texture)
└── compile.sh      # Компілятор glslangValidator
```

### Компіляція шейдерів

```bash
#!/bin/bash
# compile.sh
glslangValidator -V shader.vert -o shader.vert.spv
glslangValidator -V shader.frag -o shader.frag.spv
```

## Debug & Validation

### Vulkan Validation Layers (рекомендовано для debug)

```cpp
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Увімкнути в Instance::init() для debug builds
```

### Корисні функції для відлагодження

```cpp
// Перевірка результату Vulkan операцій
#define VK_CHECK(x) { VkResult res = (x); if (res != VK_SUCCESS) { \
    std::cerr << "Vulkan error: " << res << " at " << __LINE__ << std::endl; \
} }
```

## Дивіться також

- [[Instance\|Instance]] — точка входу Vulkan
- [[Device\|Device]] — вибір та налаштування GPU
- [[Swapchain\|Swapchain]] — ланцюг зображень
- [[Pipeline\|Pipeline]] — графічні пайплайни

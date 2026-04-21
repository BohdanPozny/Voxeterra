# Архітектура Voxeterra

## Діаграма компонентів

```
┌─────────────────────────────────────────────────────────────┐
│                        Engine (Фасад)                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │   Window    │  │   Config    │  │StateManager │          │
│  │  (GLFW)     │  │  (JSON)     │  │             │          │
│  └──────┬──────┘  └─────────────┘  └─────────────┘          │
│         │                                                   │
│  ┌──────┴───────────────────────────────────────────┐       │
│  │              Vulkan Subsystem                    │       │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ │       │
│  │  │Instance │ │ Device  │ │Swapchain│ │Pipeline │ │       │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ │       │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ │       │
│  │  │RenderPass│ │Framebuffer│ │CmdPool │ │FrameSync│ │      │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ │      │
│  └──────────────────────────────────────────────────┘      │
│                                                             │
│  ┌──────────────────────┐  ┌──────────────────────────────┐ │
│  │     WorldRenderer    │  │         UIRenderer           │ │
│  │  ┌────────────────┐  │  │  ┌────────────────────────┐  │ │
│  │  │  World         │  │  │  │  UIElement (ієрархія)  │  │ │
│  │  │  ├── Chunk[]   │  │  │  │  ├── UIPanel           │  │ │
│  │  │  │   └── Voxel │  │  │  │  ├── UIButton          │  │ │
│  │  │  └── VoxelData │  │  │  │  └── UILabel           │  │ │
│  │  └────────────────┘  │  │  │  └── TextRenderer      │  │ │
│  └──────────────────────┘  │  └────────────────────────┘  │ │
│                             │                              │ │
│  ┌──────────────────────────────────────────────────────┐  │ │
│  │              Camera + InputManager                   │  │ │
│  │    (FPS-style camera з обробкою вводу)               │  │ │
│  └──────────────────────────────────────────────────────┘  │ │
└─────────────────────────────────────────────────────────────┘
```

## Потік виконання

### Ініціалізація

```
main()
  └── Engine::init()
      ├── Window::init() ──> GLFW window + Vulkan surface
      ├── Instance::init() ──> VkInstance
      ├── Device::init() ──> GPU selection + VkDevice
      ├── Swapchain::init() ──> VkSwapchainKHR + image views
      ├── DepthBuffer::init() ──> Z-buffer для depth testing
      ├── RenderPass::init() ──> VkRenderPass (color + depth)
      ├── Framebuffer::init() ──> VkFramebuffer[] для кожного image
      ├── CommandPool::init() ──> VkCommandPool + VkCommandBuffer[]
      ├── FrameSync::init() ──> Semaphores + Fences
      ├── Pipeline::init() ──> 3D voxel pipeline
      ├── Pipeline2D::init() ──> 2D UI pipeline
      ├── UIRenderer::init() ──> TextRenderer + UI shaders
      ├── WorldRenderer::init() ──> Voxel rendering setup
      └── StateManager ──> Register states (MainMenu, Playing, etc.)
```

### Головний цикл

```
Engine::run()
  └── while (!shouldClose)
      ├── glfwPollEvents()
      ├── InputManager::beginFrame()
      ├── StateManager::update(deltaTime)
      │   └── currentState->update(deltaTime)
      ├── processInput()
      │   └── Camera::processKeyboard/Mouse()
      ├── updateWorld()
      │   └── World::update() ──> regenerate dirty chunks
      └── drawFrame()
          ├── FrameSync::waitForFence(currentFrame)
          ├── Swapchain::acquireNextImage()
          ├── WorldRenderer::updateUniformBuffer()
          │   └── MVP matrices (Model-View-Projection)
          ├── recordCommandBuffer()
          │   ├── vkCmdBeginRenderPass()
          │   ├── WorldRenderer::recordDrawCommands()
          │   └── UIRenderer::renderUI() ──> UI quads
          ├── submitCommandBuffer()
          └── presentFrame()
```

## Взаємодія компонентів

### State Machine Pattern

```cpp
// IGameState - базовий інтерфейс
class IGameState {
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual void handleInput() = 0;
};

// Конкретні стани
MainMenuState ──> UI + Buttons
PlayingState  ──> 3D World + FPS лічильник
PausedState   ──> Overlay меню
SettingsState ──> Налаштування з Config
```

### Vulkan Resource Flow

```
Application
    │
    ▼
┌─────────────────────────────────────┐
│           Engine::drawFrame()        │
│  ┌─────────────┐  ┌───────────────┐ │
│  │ FrameSync   │  │ CommandBuffer │ │
│  │  (fences)   │  │  (recording)  │ │
│  └─────────────┘  └───────────────┘ │
│           │                         │
│           ▼                         │
│  ┌─────────────────────────────┐   │
│  │      RenderPass (Begin)     │   │
│  │  ┌─────────────────────┐    │   │
│  │  │ WorldRenderer::draw │    │   │
│  │  │  └── bindPipeline() │    │   │
│  │  │  └── bindDescriptor│    │   │
│  │  │  └── drawIndexed()  │    │   │
│  │  └─────────────────────┘    │   │
│  │  ┌─────────────────────┐    │   │
│  │  │ UIRenderer::draw    │    │   │
│  │  │  └── 2D pipeline    │    │   │
│  │  └─────────────────────┘    │   │
│  └─────────────────────────────┘   │
│           │                        │
│           ▼                        │
│  Submit → Queue → Present        │
└────────────────────────────────────┘
```

### UI System Hierarchy

```
UIElement (abstract base)
    ├── UIPanel ──> Container для дочірніх елементів
    │                └── update/render children recursively
    │
    ├── UIButton ──> Clickable з callback
    │                ├── normal/hover/pressed colors
    │                └── m_onClick callback
    │
    └── UILabel ──> Текстовий елемент
                     └── Bitmap або TTF шрифт

Input Flow:
    InputManager::getNormalizedMousePosition()
            │
            ▼
    UIElement::handleInput(mousePos, clicked)
            │
            ├── contains(mousePos) ──> hit test
            ├── m_hovered = true
            └── if (clicked && m_onClick) m_onClick()
```

## Data Flow

### Voxel Rendering Pipeline

```
CPU Side:
    World::generateTestWorld()
            │
            ▼
    Chunk::generateMesh()
            │
            ├── Face culling (visible faces only)
            ├── Build vertex buffer (position + color)
            └── Build index buffer
            │
            ▼
    WorldRenderer::createVertexBuffer()
            │
            └── VkBuffer + VkDeviceMemory

GPU Side (per frame):
    updateUniformBuffer(MVP matrices)
            │
            ▼
    vkCmdBindPipeline(voxelPipeline)
    vkCmdBindDescriptorSets(UBO with MVP)
    vkCmdBindVertexBuffer(chunkVB)
    vkCmdBindIndexBuffer(chunkIB)
    vkCmdDrawIndexed(indexCount)
```

### Shader Architecture

| Pipeline | Vertex Shader | Fragment Shader | Призначення |
|----------|---------------|-------------------|-------------|
| 3D Voxel | `voxel.vert.spv` | `voxel.frag.spv` | Воксельний світ |
| 2D UI | `ui.vert.spv` | `ui.frag.spv` | UI елементи |
| Text | `ui.vert.spv` | `ui.frag.spv` | Текст (текстурований) |

## Threading & Synchronization

### Frame Synchronization (2 frames in flight)

```
Frame 0: [Acquire] → [Record] → [Submit] → [Present]
         │                    │
         └──── Fence 0 ──────┘ (CPU waits here)

Frame 1: [Acquire] → [Record] → [Submit] → [Present]
         │                    │
         └──── Fence 1 ──────┘
```

### Vulkan Synchronization Primitives

```
┌──────────────────────────────────────────────────────────┐
│  VkSemaphore imageAvailable ──> GPU: acquire next image  │
│  VkSemaphore renderFinished ──> GPU: render done, present  │
│  VkFence inFlightFence ──> CPU: wait before next frame   │
└──────────────────────────────────────────────────────────┘
```

## Memory Management

### Vulkan Object Lifecycle

Всі Vulkan обгортки використовують RAII:

```cpp
class Buffer {
    Buffer() = default;
    bool init(Device& d, VkDeviceSize s, ...) {
        vkCreateBuffer(device, &info, nullptr, &buffer);
        vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    }
    ~Buffer() {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};
```

### Smart Pointers

```cpp
// State ownership
std::map<GameState, std::unique_ptr<IGameState>> m_states;

// UI hierarchy
std::vector<std::unique_ptr<UIElement>> m_children;

// Vulkan resources (explicit cleanup order)
std::unique_ptr<Pipeline2D> m_pipeline2D;
std::unique_ptr<TextRenderer> m_textRenderer;
```

## Конфігураційна система

```
config.json
    │
    ├── graphics.fov ──> Camera::updateProjection()
    ├── graphics.renderDistance ──> World::chunk loading radius
    ├── controls.mouseSensitivity ──> Camera::mouse movement
    ├── controls.movementSpeed ──> Camera::keyboard movement
    └── ui.font ──> TextRenderer::loadFont()
```

## Події та Callbacks

### GLFW Callbacks → InputManager

```
glfwSetKeyCallback() ──> InputManager::keyCallback (internal)
glfwSetCursorPosCallback() ──> mouse position tracking
glfwSetMouseButtonCallback() ──> button state tracking
```

### UI Callbacks

```cpp
// Lambda callbacks for buttons
newGameBtn->setOnClick([this]() {
    m_nextState = GameState::PLAYING;
    m_shouldChangeState = true;
});
```

---

## Дивіться також

- [[vulkan/index\|Vulkan Підсистема]] — деталі графічного API
- [[world/index\|Світ та Вокселі]] — генерація та зберігання
- [[ui/index\|UI Система]] — компоненти інтерфейсу

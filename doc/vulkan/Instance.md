# Vulkan Instance

## Огляд

`Instance` — це найперший Vulkan об'єкт, який створюється. Він представляє з'єднання з Vulkan драйвером та є точкою входу до всіх інших Vulkan функцій.

## Залежності

- **GLFW** — для отримання необхідних розширень (`glfwGetRequiredInstanceExtensions`)

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>

class Instance {
public:
    Instance() = default;
    ~Instance();

    bool init();
    void cleanup();
    VkInstance getInstance() const { return m_instance; }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
};
```

## Ініціалізація

```cpp
bool Instance::init() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Voxterra";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Voxterra Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Отримуємо розширення від GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    return result == VK_SUCCESS;
}
```

## Cleanup

```cpp
Instance::~Instance() {
    cleanup();
}

void Instance::cleanup() {
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}
```

## Параметри ApplicationInfo

| Поле | Значення | Опис |
|------|----------|------|
| `pApplicationName` | "Voxterra" | Ім'я додатку для драйверу |
| `applicationVersion` | VK_MAKE_VERSION(1, 0, 0) | Версія додатку |
| `pEngineName` | "Voxterra Engine" | Ім'я движка |
| `engineVersion` | VK_MAKE_VERSION(1, 0, 0) | Версія движка |
| `apiVersion` | VK_API_VERSION_1_0 | Цільова версія Vulkan API |

## Enabled Extensions

Список розширень залежить від платформи та отримується автоматично від GLFW:

- **Linux (X11)**: `VK_KHR_surface`, `VK_KHR_xlib_surface`
- **Linux (Wayland)**: `VK_KHR_surface`, `VK_KHR_wayland_surface`
- **Windows**: `VK_KHR_surface`, `VK_KHR_win32_surface`
- **macOS**: `VK_KHR_surface`, `VK_KHR_metal_surface`

## Використання в Engine

```cpp
// Engine::init()
if (!m_instance.init()) {
    throw std::runtime_error("Failed to create Vulkan instance!");
}
```

## Залежні компоненти

```
Instance
    │
    ├── Window::createSurface() ──> створює VkSurfaceKHR
    │
    └── Device ──> вибирає GPU сумісний з surface
```

## Дивіться також

- [[Device\|Device]] — вибір та налаштування GPU
- [[Window\|Window]] — створення Vulkan surface

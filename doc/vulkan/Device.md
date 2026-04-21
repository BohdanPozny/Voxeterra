# Vulkan Device

## Огляд

`Device` клас управляє фізичним (GPU) та логічним пристроєм Vulkan. Відповідає за:
- Вибір підходящої відеокарти
- Створення логічного пристрою (`VkDevice`)
- Отримання черг (queues) для графіки та відображення

## API

```cpp
#pragma once
#include <vulkan/vulkan.h>

class Instance;  // Forward declaration

class Device {
public:
    Device() = default;
    ~Device();

    bool init(Instance& instance, VkSurfaceKHR surface);
    void cleanup();

    // Getters
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getLogicalDevice() const { return m_logicalDevice; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }

private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_logicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
};
```

## Вибір фізичного пристрою

```cpp
bool Device::init(Instance& instance, VkSurfaceKHR surface) {
    // 1. Отримуємо список усіх GPU
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.getInstance(), &deviceCount, nullptr);
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.getInstance(), &deviceCount, devices.data());

    // 2. Вибираємо перший підходящий
    for (const auto& device : devices) {
        if (isDeviceSuitable(device, surface)) {
            m_physicalDevice = device;
            break;
        }
    }
    // ...
}
```

## Критерії придатності (isDeviceSuitable)

```cpp
bool Device::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    // 1. Перевіряємо підтримку графічної черги
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    if (!indices.isComplete()) return false;

    // 2. Перевіряємо підтримку розширень
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    if (!extensionsSupported) return false;

    // 3. Перевіряємо підтримку swapchain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
    if (swapChainSupport.formats.empty() || 
        swapChainSupport.presentModes.empty()) return false;

    return true;
}
```

## Пошук черг (Queue Families)

```cpp
struct QueueFamilyIndices {
    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t presentFamily = UINT32_MAX;
    
    bool isComplete() {
        return graphicsFamily != UINT32_MAX && 
               presentFamily != UINT32_MAX;
    }
};

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Графічна черга
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        
        // Черга для presentation (може збігатися з graphics)
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        
        if (indices.isComplete()) break;
    }
    
    return indices;
}
```

## Створення логічного пристрою

```cpp
// Збір унікальних family індексів
std::set<uint32_t> uniqueQueueFamilies = {
    indices.graphicsFamily, 
    indices.presentFamily
};

// Створення info для кожної черги
std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
float queuePriority = 1.0f;

for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
}

// Фізичні пристрої features
VkPhysicalDeviceFeatures deviceFeatures{};

// Створення логічного пристрою
VkDeviceCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
createInfo.pQueueCreateInfos = queueCreateInfos.data();
createInfo.pEnabledFeatures = &deviceFeatures;
createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
createInfo.ppEnabledExtensionNames = deviceExtensions.data();

vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice);

// Отримуємо черги
vkGetDeviceQueue(m_logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
vkGetDeviceQueue(m_logicalDevice, indices.presentFamily, 0, &m_presentQueue);
```

## Потрібні розширення пристрою

```cpp
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME  // "VK_KHR_swapchain"
};
```

## Cleanup

```cpp
Device::~Device() {
    cleanup();
}

void Device::cleanup() {
    if (m_logicalDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(m_logicalDevice, nullptr);
        m_logicalDevice = VK_NULL_HANDLE;
    }
    // physicalDevice не потребує явного знищення
}
```

## Використання в Engine

```cpp
// Engine::init()
if (!m_device.init(m_instance, m_window.getSurface())) {
    throw std::runtime_error("Failed to initialize Vulkan device!");
}
```

## Підсумок характеристик

| Характеристика | Значення |
|----------------|----------|
| Graphics Queue | Одна черга для всіх графічних команд |
| Present Queue | Може збігатися з graphics (single queue family) |
| Extensions | VK_KHR_swapchain |
| Features | Базові (depthClamp, samplerAnisotropy — не використовуються) |

## Дивіться також

- [[Instance\|Instance]] — VkInstance для переліку GPU
- [[Swapchain\|Swapchain]] — використовує queues для presentation

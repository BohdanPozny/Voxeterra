#include "Vulkan/Device.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <vector>

namespace {
const char* kPipelineCacheFile = "pipeline_cache.bin";
}

Device::~Device() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        if (m_pipelineCache != VK_NULL_HANDLE) vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
}

// Returns true if extensionName is present in the physical device's extension list.
static bool hasDeviceExtension(VkPhysicalDevice device, const char* extensionName) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, props.data());
    for (const auto& p : props) {
        if (std::strcmp(p.extensionName, extensionName) == 0) return true;
    }
    return false;
}

bool Device::init(VkInstance instance, VkSurfaceKHR surface) noexcept {
    if (!pickPhysicalDevice(instance, surface)) {
        return false;
    }

    float queuePriority = 1.0f;
    std::set<uint32_t> uniqueFamilies = {
        m_queueIndices.graphicsFamily.value(),
        m_queueIndices.presentFamily.value()
    };
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // MoltenVK (macOS) advertises the non-conformant portability subset which
    // must be enabled whenever the device exposes it.
    if (hasDeviceExtension(m_physicalDevice, "VK_KHR_portability_subset")) {
        deviceExtensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device) != VK_SUCCESS) {
        std::cerr << "[Device] vkCreateDevice failed" << std::endl;
        return false;
    }

    vkGetDeviceQueue(m_device, m_queueIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueIndices.presentFamily.value(),  0, &m_presentQueue);
    createPipelineCache();
    return true;
}

void Device::createPipelineCache() {
    std::vector<char> data;
    if (std::ifstream file{kPipelineCacheFile, std::ios::binary}) {
        data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    // Data from another GPU or driver version is discarded rather than handed to the driver.
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    VkPipelineCacheHeaderVersionOne header{};
    if (data.size() >= sizeof(header)) std::memcpy(&header, data.data(), sizeof(header));
    const bool compatible = data.size() >= sizeof(header) &&
        header.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
        header.vendorID == props.vendorID && header.deviceID == props.deviceID &&
        std::memcmp(header.pipelineCacheUUID, props.pipelineCacheUUID, VK_UUID_SIZE) == 0;

    VkPipelineCacheCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = compatible ? data.size() : 0;
    info.pInitialData = compatible ? data.data() : nullptr;
    if (vkCreatePipelineCache(m_device, &info, nullptr, &m_pipelineCache) != VK_SUCCESS) {
        m_pipelineCache = VK_NULL_HANDLE;  // pipelines are still created, just without caching
    }
}

void Device::savePipelineCache() {
    if (m_pipelineCache == VK_NULL_HANDLE) return;
    size_t size = 0;
    if (vkGetPipelineCacheData(m_device, m_pipelineCache, &size, nullptr) != VK_SUCCESS || size == 0) return;
    std::vector<char> data(size);
    if (vkGetPipelineCacheData(m_device, m_pipelineCache, &size, data.data()) != VK_SUCCESS) return;
    std::ofstream(kPipelineCacheFile, std::ios::binary).write(data.data(), static_cast<std::streamsize>(size));
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        bool graphics = queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        // A single family that does both avoids cross-queue ownership transfers.
        if (graphics && presentSupport) {
            indices.graphicsFamily = i;
            indices.presentFamily = i;
            return indices;
        }
        if (graphics && !indices.graphicsFamily) indices.graphicsFamily = i;
        if (presentSupport && !indices.presentFamily) indices.presentFamily = i;
    }

    return indices;
}

bool Device::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const {
    if (!findQueueFamilies(device, surface).isComplete()) return false;
    if (!hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) return false;

    uint32_t formatCount = 0, presentModeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    return formatCount > 0 && presentModeCount > 0;
}

int Device::rateDevice(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 3;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 1;
        default:                                     return 0;
    }
}

bool Device::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) noexcept {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "[Device] No Vulkan-capable GPU found" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (const auto& device : devices) {
        if (!isDeviceSuitable(device, surface)) continue;
        int score = rateDevice(device);
        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "[Device] No GPU satisfies required queue families" << std::endl;
        return false;
    }

    m_queueIndices = findQueueFamilies(m_physicalDevice, surface);
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    std::cout << "[Device] Selected GPU: " << properties.deviceName << std::endl;
    return true;
}

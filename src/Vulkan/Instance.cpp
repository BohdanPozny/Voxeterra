#include "Vulkan/Instance.hpp"

#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

Instance::~Instance() noexcept {
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

bool Instance::init(std::string name) noexcept {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str();
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pEngineName = "Voxterra Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);

    // Required extensions reported by GLFW (platform-dependent surface ext).
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifdef VK_USE_PORTABILITY_ENUMERATION
    // MoltenVK (macOS) requires the portability extension and flag.
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back("VK_KHR_get_physical_device_properties2");
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "[Instance] vkCreateInstance failed (code " << result << ")" << std::endl;
        return false;
    }
    return true;
}

VkInstance Instance::getInstance() noexcept {
    return m_instance;
}

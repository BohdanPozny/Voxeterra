#include "Vulkan/Instance.hpp"

#include <GLFW/glfw3.h>
#include <iostream>

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
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pEngineName = "Voxterra Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    // debug
    std::cout << "Знайдено розширень GLFW: " << glfwExtensionCount << std::endl;
    for(uint32_t i = 0; i < glfwExtensionCount; i++) {
        std::cout << " - " << glfwExtensions[i] << std::endl;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        std::cout << "[Error Vulkan Instance] Not create instance. \nError code: " << result << std::endl;
        return false;
    }

    return true;
}

VkInstance Instance::getInstance() noexcept {
    return m_instance;
}

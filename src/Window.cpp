#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

Window::~Window() noexcept {
    // m_window автоматично знищується через unique_ptr
    // glfwTerminate викликається останнім
    glfwTerminate();
}

bool Window::init(int w, int h, std::string name) noexcept {
    width = w;
    height = h;
    nameWindow = name;

    // Ініціалізуємо GLFW тільки якщо ще не ініціалізований
    if (!glfwInit()) {
        std::cout << "[Error Window] Not init GLFW" << std::endl;
        return false; 
    }

    //settings window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);  // Дозволяємо resize для Wayland tiling

    m_window.reset(glfwCreateWindow(width, height, nameWindow.c_str(), nullptr, nullptr));
    if (m_window.get() == nullptr) {
        std::cout << "[Error window] Not create window" << std::endl;
        return false; 
    }
    
    // Логування початкового розміру вікна
    int actualWidth, actualHeight;
    glfwGetWindowSize(m_window.get(), &actualWidth, &actualHeight);
    std::cout << "[Window] Requested: " << width << "x" << height 
              << ", Actual: " << actualWidth << "x" << actualHeight << std::endl;
    
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(m_window.get(), &fbWidth, &fbHeight);
    std::cout << "[Window] Framebuffer: " << fbWidth << "x" << fbHeight << std::endl;

    return true;
}

bool Window::shouldClose() noexcept {
    return glfwWindowShouldClose(m_window.get());
}

bool Window::createWindowSurface(VkInstance m_instance, VkSurfaceKHR* surface) const noexcept {
    VkResult result = glfwCreateWindowSurface(m_instance, m_window.get(), nullptr, surface);
    if (result != VK_SUCCESS) {
        std::cout << "[Error Window] Not create window surface. \nError code: " << result << std::endl;
        return false;
    }
    return true;
}

void Window::getFramebufferSize(int* w, int* h) const {
    glfwGetFramebufferSize(m_window.get(), w, h);
}


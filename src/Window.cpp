#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

Window::~Window() noexcept {
    // unique_ptr destroys the GLFWwindow; terminate the library last.
    glfwTerminate();
}

bool Window::init(int w, int h, std::string name) noexcept {
    width = w;
    height = h;
    nameWindow = name;

    if (!glfwInit()) {
        std::cerr << "[Window] glfwInit failed" << std::endl;
        return false;
    }

    // Disable the OpenGL context creation; resizable for tiling WMs / dev ergonomics.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window.reset(glfwCreateWindow(width, height, nameWindow.c_str(), nullptr, nullptr));
    if (!m_window) {
        std::cerr << "[Window] glfwCreateWindow failed" << std::endl;
        return false;
    }

    // Framebuffer resize callback sets the dirty flag consumed by the engine.
    glfwSetWindowUserPointer(m_window.get(), this);
    glfwSetFramebufferSizeCallback(m_window.get(), framebufferResizeCallback);
    return true;
}

bool Window::shouldClose() noexcept {
    return glfwWindowShouldClose(m_window.get());
}

bool Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const noexcept {
    VkResult result = glfwCreateWindowSurface(instance, m_window.get(), nullptr, surface);
    if (result != VK_SUCCESS) {
        std::cerr << "[Window] glfwCreateWindowSurface failed (code " << result << ")" << std::endl;
        return false;
    }
    return true;
}

void Window::getFramebufferSize(int* w, int* h) const {
    glfwGetFramebufferSize(m_window.get(), w, h);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_framebufferResized = true;
    }
}


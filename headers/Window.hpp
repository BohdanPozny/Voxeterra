#pragma once

// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <memory>

// deleter for unique_ptr
struct GLFWwindowDeleter {
    void operator()(GLFWwindow* ptr) const {
        if (ptr != nullptr) {
            glfwDestroyWindow(ptr); 
        }
    }
};

class Window {
private:
    int width;
    int height;
    std::string nameWindow;

    std::unique_ptr<GLFWwindow, GLFWwindowDeleter> m_window;

public:
    Window() = default;
    ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool init(int w, int h, std::string name) noexcept;

    bool shouldClose() noexcept;
    bool createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const noexcept;

    void getFramebufferSize(int* w, int* h) const;
    GLFWwindow* getWindow() const noexcept { return m_window.get(); }
};

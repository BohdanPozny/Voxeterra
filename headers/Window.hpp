#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <memory>

// Custom deleter so a GLFW window can live inside a unique_ptr.
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

    // Create the window. If fullscreen is true the primary monitor's native
    // resolution is used and (w, h) are ignored. Otherwise a maximized
    // windowed surface at (w, h) is created so it actually fills the screen.
    bool init(int w, int h, std::string name, bool fullscreen = false) noexcept;

    bool shouldClose() noexcept;
    bool createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const noexcept;

    void getFramebufferSize(int* w, int* h) const;
    GLFWwindow* getWindow() const noexcept { return m_window.get(); }

    bool wasResized() const noexcept { return m_framebufferResized; }
    void resetResizedFlag() noexcept { m_framebufferResized = false; }

private:
    bool m_framebufferResized = false;
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};

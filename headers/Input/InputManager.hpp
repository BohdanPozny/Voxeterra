#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

// Centralised keyboard + mouse state tracker with edge detection.
//   isKeyDown     : the key is currently pressed.
//   isKeyPressed  : the key transitioned to pressed this frame (edge).
//   isKeyReleased : the key transitioned to released this frame (edge).
// Also exposes mouse position/delta and button states.
class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Bind to an existing GLFW window (no ownership).
    void init(GLFWwindow* window);

    // Must be called at the top of every frame, before glfwPollEvents().
    void beginFrame();

    // --- Keyboard ---
    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;
    bool isKeyReleased(int key) const;

    // --- Mouse buttons ---
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;    // Edge: just clicked
    bool isMouseButtonReleased(int button) const;

    // --- Mouse position ---
    glm::vec2 getMousePosition() const { return m_mousePos; }
    glm::vec2 getMouseDelta() const { return m_mouseDelta; }
    glm::vec2 getNormalizedMousePosition() const;  // 0..1 within the window

    // Cursor mode
    void setCursorEnabled(bool enabled);
    bool isCursorEnabled() const { return m_cursorEnabled; }

private:
    GLFWwindow* m_window = nullptr;

    // Current + previous key states (used for edge detection).
    std::unordered_map<int, bool> m_keysCurrent;
    std::unordered_map<int, bool> m_keysPrevious;

    // Mouse button states.
    std::unordered_map<int, bool> m_mouseCurrent;
    std::unordered_map<int, bool> m_mousePrevious;

    // Mouse cursor state.
    glm::vec2 m_mousePos{0.0f};
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    bool m_firstMouse = true;

    bool m_cursorEnabled = true;
};

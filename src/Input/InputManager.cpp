#include "Input/InputManager.hpp"

void InputManager::init(GLFWwindow* window) {
    m_window = window;

    // Seed the mouse position to avoid a large delta on the first frame.
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_mousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    m_lastMousePos = m_mousePos;
}

void InputManager::beginFrame() {
    if (!m_window) return;

    // Snapshot previous state so edge detection works.
    m_keysPrevious = m_keysCurrent;
    m_mousePrevious = m_mouseCurrent;

    // Only poll keys that callers have shown interest in.
    for (auto& [key, _] : m_keysCurrent) {
        m_keysCurrent[key] = glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    // Mouse buttons.
    for (auto& [btn, _] : m_mouseCurrent) {
        m_mouseCurrent[btn] = glfwGetMouseButton(m_window, btn) == GLFW_PRESS;
    }

    // Mouse position and delta.
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    glm::vec2 newPos(static_cast<float>(x), static_cast<float>(y));

    if (m_firstMouse) {
        m_lastMousePos = newPos;
        m_firstMouse = false;
    }

    m_mouseDelta = newPos - m_lastMousePos;
    m_lastMousePos = m_mousePos;
    m_mousePos = newPos;
}

bool InputManager::isKeyDown(int key) const {
    if (!m_window) return false;
    // Lazily register the key so subsequent frames poll it.
    auto it = m_keysCurrent.find(key);
    if (it == m_keysCurrent.end()) {
        const_cast<InputManager*>(this)->m_keysCurrent[key] =
            glfwGetKey(m_window, key) == GLFW_PRESS;
        return m_keysCurrent.at(key);
    }
    return it->second;
}

bool InputManager::isKeyPressed(int key) const {
    bool current = isKeyDown(key);
    auto it = m_keysPrevious.find(key);
    bool previous = (it != m_keysPrevious.end()) ? it->second : false;
    return current && !previous;
}

bool InputManager::isKeyReleased(int key) const {
    bool current = isKeyDown(key);
    auto it = m_keysPrevious.find(key);
    bool previous = (it != m_keysPrevious.end()) ? it->second : false;
    return !current && previous;
}

bool InputManager::isMouseButtonDown(int button) const {
    if (!m_window) return false;
    auto it = m_mouseCurrent.find(button);
    if (it == m_mouseCurrent.end()) {
        const_cast<InputManager*>(this)->m_mouseCurrent[button] =
            glfwGetMouseButton(m_window, button) == GLFW_PRESS;
        return m_mouseCurrent.at(button);
    }
    return it->second;
}

bool InputManager::isMouseButtonPressed(int button) const {
    bool current = isMouseButtonDown(button);
    auto it = m_mousePrevious.find(button);
    bool previous = (it != m_mousePrevious.end()) ? it->second : false;
    return current && !previous;
}

bool InputManager::isMouseButtonReleased(int button) const {
    bool current = isMouseButtonDown(button);
    auto it = m_mousePrevious.find(button);
    bool previous = (it != m_mousePrevious.end()) ? it->second : false;
    return !current && previous;
}

glm::vec2 InputManager::getNormalizedMousePosition() const {
    if (!m_window) return {0.0f, 0.0f};
    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    if (width <= 0 || height <= 0) return {0.0f, 0.0f};
    return {m_mousePos.x / static_cast<float>(width),
            m_mousePos.y / static_cast<float>(height)};
}

void InputManager::setCursorEnabled(bool enabled) {
    if (!m_window) return;
    m_cursorEnabled = enabled;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    // Resetting avoids a large mouse delta when the cursor visibility toggles.
    m_firstMouse = true;
}

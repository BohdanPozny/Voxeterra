# Input Manager

## Огляд

`InputManager` — централізована система обробки вводу від клавіатури та миші. Підтримує edge detection (натискання "тільки що") для клавіш та кнопок миші.

## API

```cpp
#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    // Ініціалізація
    void init(GLFWwindow* window);

    // Викликати на початку кожного кадру (ПЕРЕД glfwPollEvents)
    void beginFrame();

    // === Keyboard ===
    bool isKeyDown(int key) const;        // Клавіша тримається
    bool isKeyPressed(int key) const;     // Тільки що натиснута (edge)
    bool isKeyReleased(int key) const;    // Тільки що відпущена (edge)

    // === Mouse buttons ===
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;    // Edge: just clicked
    bool isMouseButtonReleased(int button) const;

    // === Mouse position ===
    glm::vec2 getMousePosition() const { return m_mousePos; }
    glm::vec2 getMouseDelta() const { return m_mouseDelta; }
    glm::vec2 getNormalizedMousePosition() const;  // 0..1 у межах вікна

    // Cursor mode
    void setCursorEnabled(bool enabled);
    bool isCursorEnabled() const { return m_cursorEnabled; }

private:
    GLFWwindow* m_window = nullptr;

    // Стан клавіш (поточний та попередній для edge detection)
    std::unordered_map<int, bool> m_keysCurrent;
    std::unordered_map<int, bool> m_keysPrevious;

    // Стан кнопок миші
    std::unordered_map<int, bool> m_mouseCurrent;
    std::unordered_map<int, bool> m_mousePrevious;

    // Позиція миші
    glm::vec2 m_mousePos{0.0f};
    glm::vec2 m_lastMousePos{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    bool m_firstMouse = true;

    bool m_cursorEnabled = true;
};
```

## Edge Detection

```cpp
void InputManager::beginFrame() {
    if (!m_window) return;

    // Зберігаємо попередній стан
    m_keysPrevious = m_keysCurrent;
    m_mousePrevious = m_mouseCurrent;

    // Оновлюємо поточний стан тільки для відстежуваних клавіш
    for (auto& [key, _] : m_keysCurrent) {
        m_keysCurrent[key] = glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    // Оновлюємо кнопки миші
    for (auto& [btn, _] : m_mouseCurrent) {
        m_mouseCurrent[btn] = glfwGetMouseButton(m_window, btn) == GLFW_PRESS;
    }

    // Оновлюємо позицію миші
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
```

## Приклади використання

### Перевірка стану клавіші

```cpp
// Утримується
if (input.isKeyDown(GLFW_KEY_W)) {
    // Рухатися вперед
}

// Тільки що натиснута (один раз на натискання)
if (input.isKeyPressed(GLFW_KEY_SPACE)) {
    // Стрибок
}

// Тільки що відпущена
if (input.isKeyReleased(GLFW_KEY_ESCAPE)) {
    // Відкрити меню
}
```

### Миша

```cpp
// Клік
if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
    // Обробити клік
}

// Рух миші для камери
glm::vec2 delta = input.getMouseDelta();
camera.processMouseMovement(delta.x, -delta.y);

// Нормалізована позиція для UI
glm::vec2 normalized = input.getNormalizedMousePosition();
// x, y в діапазоні [0, 1]
```

## Режим курсора

```cpp
void InputManager::setCursorEnabled(bool enabled) {
    if (!m_window) return;
    m_cursorEnabled = enabled;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    // Скидаємо delta після зміни режиму
    m_firstMouse = true;
}
```

| Режим | Опис | Використання |
|-------|------|--------------|
| `GLFW_CURSOR_NORMAL` | Видимий курсор, звичайна поведінка | Меню, UI |
| `GLFW_CURSOR_HIDDEN` | Прихований курсор | — |
| `GLFW_CURSOR_DISABLED` | Прихований + необмежений рух | Геймплей, камера |

## Lazy Key Tracking

```cpp
bool InputManager::isKeyDown(int key) const {
    if (!m_window) return false;
    
    auto it = m_keysCurrent.find(key);
    if (it == m_keysCurrent.end()) {
        // Клавіша ще не відстежується — додаємо
        const_cast<InputManager*>(this)->m_keysCurrent[key] =
            glfwGetKey(m_window, key) == GLFW_PRESS;
        return m_keysCurrent.at(key);
    }
    return it->second;
}
```

Переваги lazy tracking:
- Не потрібно реєструвати всі клавіші заздалегідь
- Економія пам'яті (тільки використані клавіші)
- Простий API

## Інтеграція в головний цикл

```cpp
void Engine::run() {
    while (!glfwWindowShouldClose(m_window.getWindow())) {
        // 1. Розпочинаємо кадр вводу
        m_input.beginFrame();
        
        // 2. GLFW обробляє події
        glfwPollEvents();
        
        // 3. Обробляємо ввід для поточного стану
        m_stateManager.handleInput();
        
        // 4. Оновлення та рендеринг
        update(deltaTime);
        drawFrame();
    }
}
```

## GLFW Key Codes

```cpp
// Рух
GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D
GLFW_KEY_SPACE        // Стрибок
GLFW_KEY_LEFT_SHIFT   // Біг

// Навігація
GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT
GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_TAB

// Функціональні
GLFW_KEY_F1 - GLFW_KEY_F12

// Миша
GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE
```

## Дивіться також

- [[Camera\|Camera]] — використовує InputManager для управління
- [[states/index\|Стани гри]] — обробка вводу в різних станах

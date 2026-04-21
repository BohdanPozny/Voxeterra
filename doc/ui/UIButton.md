# UIButton

## Огляд

`UIButton` — клікабельний UI елемент з підтримкою станів (normal, hover, pressed) та callback функцій.

## API

```cpp
#pragma once
#include "UI/UIElement.hpp"
#include <string>
#include <functional>

class UIButton : public UIElement {
private:
    std::string m_text;
    glm::vec4 m_normalColor;
    glm::vec4 m_hoverColor;
    glm::vec4 m_pressedColor;
    std::function<void()> m_onClick;
    bool m_wasPressed = false;

public:
    UIButton(const glm::vec2& position, const glm::vec2& size,
             const std::string& text,
             const glm::vec4& normalColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
             const glm::vec4& hoverColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
             const glm::vec4& pressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));

    // Реалізація віртуальних методів
    void update(float deltaTime) override;
    void render() override;
    void handleInput(const glm::vec2& mousePos, bool mousePressed) override;

    // Специфічні методи
    void setOnClick(std::function<void()> callback) { m_onClick = callback; }
    void setText(const std::string& text) { m_text = text; }
    std::string getText() const { return m_text; }
};
```

## Конструктор

```cpp
UIButton::UIButton(const glm::vec2& position, const glm::vec2& size,
                   const std::string& text,
                   const glm::vec4& normalColor,
                   const glm::vec4& hoverColor,
                   const glm::vec4& pressedColor)
    : UIElement(position, size, normalColor)
    , m_text(text)
    , m_normalColor(normalColor)
    , m_hoverColor(hoverColor)
    , m_pressedColor(pressedColor) {}
```

## Стани кнопки

| Стан | Умова | Колір |
|------|-------|-------|
| Normal | Не під курсором | `m_normalColor` |
| Hover | Під курсором, не натиснута | `m_hoverColor` |
| Pressed | Під курсором, натиснута | `m_pressedColor` |
| Disabled | `m_enabled = false` | Сірий, прозорий |

## Оновлення кольору

```cpp
void UIButton::update(float deltaTime) {
    if (!m_enabled) {
        m_color = glm::vec4(0.2f, 0.2f, 0.2f, 0.5f);  // Disabled
    } else if (m_wasPressed) {
        m_color = m_pressedColor;
    } else if (m_hovered) {
        m_color = m_hoverColor;
    } else {
        m_color = m_normalColor;
    }
}
```

## Обробка вводу

```cpp
void UIButton::handleInput(const glm::vec2& mousePos, bool mousePressed) {
    if (!m_enabled) return;

    // Перевіряємо чи миша над кнопкою
    m_hovered = contains(mousePos);

    // Логіка натискання
    if (m_hovered && mousePressed && !m_wasPressed) {
        // Початок натискання
        m_wasPressed = true;
    } else if (!mousePressed && m_wasPressed) {
        // Кінець натискання (click event)
        if (m_hovered && m_onClick) {
            m_onClick();  // Викликаємо callback
        }
        m_wasPressed = false;
    }
}
```

## Callback

```cpp
// Lambda як callback
button->setOnClick([this]() {
    std::cout << "Button clicked!" << std::endl;
    m_nextState = GameState::PLAYING;
    m_shouldChangeState = true;
});

// Функція як callback
void onExitClick() {
    glfwSetWindowShouldClose(m_engine->getWindow().getWindow(), GLFW_TRUE);
}

button->setOnClick(onExitClick);
```

## Приклад використання

### Кнопка New Game

```cpp
auto newGameBtn = std::make_unique<UIButton>(
    glm::vec2(0.4f, 0.4f),    // Позиція (центр по X, 40% від верху)
    glm::vec2(0.2f, 0.08f),   // Розмір (20% ширини, 8% висоти)
    "New Game",                // Текст
    glm::vec4(0.2f, 0.8f, 0.2f, 1.0f),  // Зелений normal
    glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),  // Світліший hover
    glm::vec4(0.1f, 0.6f, 0.1f, 1.0f)   // Темніший pressed
);

newGameBtn->setOnClick([this]() {
    std::cout << "[UI] New Game clicked!" << std::endl;
    m_nextState = GameState::PLAYING;
    m_shouldChangeState = true;
});

m_menuPanel->addChild(std::move(newGameBtn));
```

### Кнопка Exit

```cpp
auto exitBtn = std::make_unique<UIButton>(
    glm::vec2(0.4f, 0.52f),   // Позиція (трохи нижче)
    glm::vec2(0.2f, 0.08f),   // Розмір
    "Exit",
    glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),  // Червоний
    glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),  // Hover
    glm::vec4(0.6f, 0.1f, 0.1f, 1.0f)   // Pressed
);

exitBtn->setOnClick([this]() {
    if (m_engine) {
        glfwSetWindowShouldClose(m_engine->getWindow().getWindow(), GLFW_TRUE);
    }
});

m_menuPanel->addChild(std::move(exitBtn));
```

## Дивіться також

- [[UIElement\|UIElement]] — базовий клас
- [[UIPanel\|UIPanel]] — контейнер для кнопок

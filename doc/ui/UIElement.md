# UIElement

## Огляд

`UIElement` — базовий абстрактний клас для всіх UI компонентів. Реалізує поліморфізм та композицію для побудови ієрархії інтерфейсу.

## API

```cpp
#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class UIElement {
protected:
    // Позиція та розмір в нормалізованих координатах (0-1)
    glm::vec2 m_position;
    glm::vec2 m_size;
    glm::vec4 m_color;
    
    // Стан
    bool m_visible = true;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_focused = false;
    
    // Ієрархія (композиція)
    UIElement* m_parent = nullptr;
    std::vector<std::unique_ptr<UIElement>> m_children;

public:
    UIElement(const glm::vec2& position, const glm::vec2& size, 
              const glm::vec4& color)
        : m_position(position), m_size(size), m_color(color) {}
    
    virtual ~UIElement() = default;

    // Віртуальні методи (ПОЛІМОРФІЗМ)
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void handleInput(const glm::vec2& mousePos, bool mousePressed) = 0;

    // Getters/Setters
    void setPosition(const glm::vec2& pos) { m_position = pos; }
    void setSize(const glm::vec2& size) { m_size = size; }
    void setColor(const glm::vec4& color) { m_color = color; }
    void setVisible(bool visible) { m_visible = visible; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    glm::vec2 getPosition() const { return m_position; }
    glm::vec2 getSize() const { return m_size; }
    glm::vec4 getColor() const { return m_color; }
    bool isVisible() const { return m_visible; }
    bool isEnabled() const { return m_enabled; }
    bool isHovered() const { return m_hovered; }
    bool isFocused() const { return m_focused; }

    // Ієрархія
    void addChild(std::unique_ptr<UIElement> child) {
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    const std::vector<std::unique_ptr<UIElement>>& getChildren() const {
        return m_children;
    }

    // Перевірка чи точка всередині елемента
    bool contains(const glm::vec2& point) const {
        return point.x >= m_position.x && 
               point.x <= m_position.x + m_size.x &&
               point.y >= m_position.y && 
               point.y <= m_position.y + m_size.y;
    }
};
```

## Поліморфізм

```
UIElement (abstract)
    ├── update(deltaTime)     // Оновлення стану
    ├── render()              // Рендеринг
    └── handleInput(pos, click) // Обробка вводу

Конкретні реалізації:
    ├── UIPanel   ──> update children recursively
    ├── UIButton  ──> handle hover/click, change color
    └── UILabel   ──> (порожня реалізація)
```

## Ієрархія та композиція

```cpp
// Створення ієрархії
auto panel = std::make_unique<UIPanel>(
    glm::vec2(0.0f, 0.0f),     // Позиція
    glm::vec2(1.0f, 1.0f),     // Розмір (весь екран)
    glm::vec4(0.0f)            // Прозорий
);

auto button = std::make_unique<UIButton>(
    glm::vec2(0.4f, 0.4f),
    glm::vec2(0.2f, 0.08f),
    "Click Me",
    normalColor, hoverColor, pressedColor
);

panel->addChild(std::move(button));
```

## Hit Testing

```cpp
// Перевірка чи миша над елементом
bool UIElement::contains(const glm::vec2& point) const {
    // Нормалізовані координати: (0,0) - лівий верх, (1,1) - правий низ
    return point.x >= m_position.x && 
           point.x <= m_position.x + m_size.x &&
           point.y >= m_position.y && 
           point.y <= m_position.y + m_size.y;
}

// Використання
if (button->contains(mousePos)) {
    button->m_hovered = true;
}
```

## Життєвий цикл

```
Створення
    │
    ├── constructor(pos, size, color)
    ├── addChild() ──> (для панелей)
    │
    ▼
Оновлення (кожен кадр)
    │
    ├── update(dt)
    │   └── (успадкована реалізація)
    │
    ├── handleInput(mousePos, clicked)
    │   └── contains() ──> hit test
    │   └── (успадкована логіка)
    │
    └── render() ──> (викликається через UIRenderer)
```

## Наслідування

Для створення нового UI елемента:

```cpp
class MyCustomElement : public UIElement {
public:
    MyCustomElement(const glm::vec2& pos, const glm::vec2& size)
        : UIElement(pos, size, glm::vec4(1.0f)) {}

    void update(float deltaTime) override {
        // Ваша логіка оновлення
    }

    void render() override {
        // Ваша логіка рендерингу
        // (зазвичай викликається через UIRenderer)
    }

    void handleInput(const glm::vec2& mousePos, bool mousePressed) override {
        // Ваша логіка вводу
        if (contains(mousePos)) {
            // Миша над елементом
        }
    }
};
```

## Дивіться також

- [[UIPanel\|UIPanel]] — контейнер для елементів
- [[UIButton\|UIButton]] — клікабельна кнопка
- [[UILabel\|UILabel]] — текстовий елемент

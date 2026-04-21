# UI Система

## Огляд

UI система Voxeterra побудована на принципах **ООП**, **поліморфізму** та **композиції**. Всі UI елементи наслідуються від базового класу `UIElement`.

## Структура модулів

```
ui/
├── [[UIElement\|UIElement]]     # Базовий абстрактний клас
├── [[UIPanel\|UIPanel]]        # Контейнер для інших елементів
├── [[UIButton\|UIButton]]       # Клікабельна кнопка
├── [[UILabel\|UILabel]]         # Текстовий елемент
└── [[UIRenderer\|UIRenderer]]   # Рендеринг UI через Vulkan
```

## Ієрархія UI

```
UIElement (abstract)
    ├── UIPanel ──> Container (може містити інші елементи)
    ├── UIButton ──> Clickable (з callback)
    └── UILabel ──> Text display

Приклад ієрархії:

MainMenuPanel (UIPanel)
    ├── NewGameButton (UIButton)
    ├── SettingsButton (UIButton)
    └── ExitButton (UIButton)

HUDPanel (UIPanel)
    └── FPSLabel (UILabel)
```

## Координатна система

UI використовує **нормалізовані координати** (0.0 - 1.0):

```
┌─────────────────────────────┐ Y=0
│                             │
│    (0.5, 0.5)               │
│       ● center              │
│                             │
└─────────────────────────────┘ Y=1
X=0                         X=1
```

### Приклади позицій

| Позиція | X | Y | Опис |
|---------|---|---|------|
| Лівий верхній | 0.0 | 0.0 | Кут вікна |
| Центр | 0.5 | 0.5 | Центр вікна |
| Правий низ | 1.0 | 1.0 | Кут вікна |

## Життєвий цикл UI

```
1. Створення (в конструкторі State)
   └── createUI() ──> створюємо панелі та кнопки

2. Оновлення (кожен кадр)
   └── update(deltaTime) ──> анімації, hover ефекти

3. Ввід (кожен кадр)
   └── handleInput(mousePos, clicked) ──> обробка кліків

4. Рендеринг (через UIRenderer)
   └── renderUI(rootElement, commandBuffer)
```

## Взаємодія з InputManager

```cpp
void MainMenuState::handleInput() {
    // Отримуємо позицію миші в нормалізованих координатах
    glm::vec2 normalizedMouse = m_engine->getInput().getNormalizedMousePosition();
    
    // Перевіряємо чи був клік
    bool justClicked = m_engine->getInput().isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    
    // Передаємо в UI систему
    m_menuPanel->handleInput(normalizedMouse, justClicked);
}
```

## Рендеринг UI

### UIRenderer Pipeline

```cpp
void UIRenderer::renderUI(UIElement* root, VkCommandBuffer cmd) {
    // 1. Збираємо всі елементи
    collectUIElements(root);
    
    // 2. Генеруємо вершини для квадів
    for (auto* element : collectedElements) {
        addQuad(element->getPosition(), element->getSize(), element->getColor());
    }
    
    // 3. Рендеринг тексту (якщо UILabel)
    // ... render text bitmap ...
    
    // 4. Upload to GPU
    copyToVertexBuffer();
    
    // 5. Draw
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}
```

## Шрифти

### Bitmap Font (Hardcoded)

```cpp
// 5x7 пікселів на символ (hardcoded patterns)
static const int font_F[7] = {0b11110, 0b10000, 0b11100, 0b10000, 0b10000, 0b10000, 0b10000};
// F виглядає так:
// █████
// █
// ████
// █
// █
// █
// █
```

### TTF Font (FreeType)

```cpp
TextRenderer::init(device, "fonts/default.ttf", fontSize);
// Генеруємо bitmap atlas з усіма символами
// Рендеринг через текстуровані квадри
```

## Стилізація

### Кольори

```cpp
// Формат: glm::vec4(r, g, b, a)  // 0.0 - 1.0

// Приклади:
glm::vec4 red    = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
glm::vec4 green  = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
glm::vec4 blue   = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
glm::vec4 white  = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
glm::vec4 transparent = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
```

### Button States

```cpp
class UIButton : public UIElement {
    glm::vec4 m_normalColor;   // Звичайний стан
    glm::vec4 m_hoverColor;    // Наведення миші
    glm::vec4 m_pressedColor;  // Натиснутий
};
```

## Приклади використання

### Кнопка з callback

```cpp
auto button = std::make_unique<UIButton>(
    glm::vec2(0.4f, 0.4f),    // Позиція
    glm::vec2(0.2f, 0.08f),   // Розмір (20% ширини, 8% висоти)
    "Click Me",               // Текст
    glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),  // Normal
    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),  // Hover
    glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)   // Pressed
);

button->setOnClick([]() {
    std::cout << "Button clicked!" << std::endl;
});

panel->addChild(std::move(button));
```

### Текстовий елемент

```cpp
auto label = std::make_unique<UILabel>(
    glm::vec2(0.02f, 0.02f),  // Позиція (верхній лівий)
    glm::vec2(0.1f, 0.05f),   // Розмір
    "FPS: 60",                // Текст
    16,                       // Розмір шрифту
    glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)  // Жовтий колір
);

panel->addChild(std::move(label));
```

## Дивіться також

- [[UIElement\|UIElement]] — базовий клас
- [[UIPanel\|UIPanel]] — контейнер
- [[UIButton\|UIButton]] — кнопка
- [[UILabel\|UILabel]] — текст
- [[UIRenderer\|UIRenderer]] — рендеринг

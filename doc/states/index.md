# Система Станів (State Machine)

## Огляд

Система станів реалізує паттерн **State Machine** для управління різними станами гри (меню, гра, пауза, налаштування). Кожен стан — це окремий клас з чітко визначеним життєвим циклом.

## Структура модулів

```
states/
├── [[index\|index]]              # Цей файл — огляд системи
├── [[GameState\|GameState]]       # Базовий інтерфейс IGameState
├── [[StateManager\|StateManager]] # Управління переходами між станами
├── [[MainMenuState\|MainMenuState]] # Головне меню
├── [[PlayingState\|PlayingState]]   # Ігровий процес
├── [[PausedState\|PausedState]]     # Меню паузи
└── [[SettingsState\|SettingsState]] # Налаштування
```

## Діаграма переходів станів

```
                    ┌─────────────┐
    ┌──────────────>│  MAIN_MENU  │<──────────────┐
    │               └──────┬──────┘               │
    │                      │ New Game             │
    │    Exit              │                      │ Main Menu
    │                      ▼                      │
    │               ┌─────────────┐    ESC        │
    └───────────────│  PLAYING    │───────────────┘
                    └──────┬──────┘
                           │ ESC
                           ▼
                    ┌─────────────┐    Resume    ┌─────────────┐
                    │   PAUSED    │─────────────>│  PLAYING    │
                    └──────┬──────┘              └─────────────┘
                           │ Settings
                           ▼
                    ┌─────────────┐    Back      ┌─────────────┐
                    │  SETTINGS   │─────────────>│   PAUSED    │
                    └──────┬──────┘              └─────────────┘
                           │ Back
                           │
                           └─────────────────────>│  MAIN_MENU  │
                                                   └─────────────┘
```

## Інтерфейс IGameState

```cpp
#pragma once
#include <array>

// Forward declarations
class UIElement;

enum class GameState {
    MAIN_MENU,
    PLAYING,
    PAUSED,
    SETTINGS
};

class IGameState {
public:
    virtual ~IGameState() = default;

    // Життєвий цикл
    virtual void onEnter() = 0;   // Вхід в стан
    virtual void onExit() = 0;    // Вихід зі стану
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void handleInput() = 0;

    // Перехід між станами
    virtual GameState getNextState() const = 0;
    virtual bool shouldChangeState() const = 0;

    // Поліморфні хуки для рендерингу (замість if-else в Engine)
    virtual std::array<float, 4> getClearColor() const {
        return {0.2f, 0.2f, 0.25f, 1.0f};  // Темно-сірий за замовчуванням
    }
    
    virtual bool shouldRenderWorld() const { return false; }
    
    virtual UIElement* getUIRoot() { return nullptr; }
    
    virtual void processGameplayInput(float deltaTime) {}
};
```

## StateManager

```cpp
#pragma once
#include "GameState.hpp"
#include <memory>
#include <map>

class StateManager {
private:
    std::map<GameState, std::unique_ptr<IGameState>> m_states;
    GameState m_currentState;
    GameState m_nextState;
    bool m_shouldChangeState = false;

public:
    StateManager();
    ~StateManager() = default;

    void registerState(GameState state, std::unique_ptr<IGameState> stateObj);
    void changeState(GameState newState);

    void update(float deltaTime);
    void render();
    void handleInput();

    GameState getCurrentState() const { return m_currentState; }
    IGameState* getState(GameState state) const;
};
```

## Життєвий цикл стану

```
registerState(MAIN_MENU, new MainMenuState())
         │
         ▼
    ┌─────────┐
    │ onEnter │ ──> Ініціалізація UI, показ курсора, повідомлення
    └────┬────┘
         │
         ▼
┌─────────────────┐
│ update(dt)      │ ──> Кожен кадр: оновлення UI, логіка
│ handleInput()   │ ──> Обробка клавіш/миші
│ render()        │ ──> (опціонально) консольний рендеринг
└────────┬────────┘
         │
         │ shouldChangeState() == true
         ▼
    ┌─────────┐
    │ onExit  │ ──> Збереження даних, очищення, приховування курсора
    └────┬────┘
         │
         ▼
    onEnter() нового стану
```

## Приклад реалізації: MainMenuState

```cpp
class MainMenuState : public IGameState {
private:
    Engine* m_engine;
    GameState m_nextState = GameState::MAIN_MENU;
    bool m_shouldChangeState = false;
    bool m_shouldExit = false;

    std::unique_ptr<UIPanel> m_menuPanel;
    UIButton* m_newGameButton = nullptr;
    UIButton* m_exitButton = nullptr;

    void createUI();

public:
    MainMenuState(Engine* engine);

    void onEnter() override {
        std::cout << "[MainMenuState] Entering main menu" << std::endl;
        // Показуємо курсор для кліків
        glfwSetInputMode(m_engine->getWindow().getWindow(), 
                        GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onExit() override {
        std::cout << "[MainMenuState] Leaving main menu" << std::endl;
    }

    void update(float deltaTime) override {
        if (m_menuPanel) {
            m_menuPanel->update(deltaTime);
        }
    }

    void render() override {
        // Рендеринг відбувається через UIRenderer в Engine
    }

    void handleInput() override {
        if (!m_engine || !m_menuPanel) return;

        auto& input = m_engine->getInput();
        glm::vec2 mousePos = input.getNormalizedMousePosition();
        bool clicked = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

        m_menuPanel->handleInput(mousePos, clicked);
    }

    GameState getNextState() const override { return m_nextState; }
    bool shouldChangeState() const override { return m_shouldChangeState; }

    UIElement* getUIRoot() override { return m_menuPanel.get(); }
};
```

## Таблиця станів

| Стан | Clear Color | Рендерить світ | UI | Геймплей інпут |
|------|-------------|----------------|----|----------------|
| MainMenu | Темно-сірий | ❌ | ✅ Кнопки | ❌ |
| Playing | Темно-синій | ✅ | ✅ FPS лічильник | ✅ |
| Paused | Темно-сірий | ✅ (заморожено) | ❌ (консоль) | ❌ |
| Settings | Темно-сірий | ❌ | ❌ (консоль) | ❌ |

## Інтеграція з Engine

```cpp
void Engine::init() {
    // ... Vulkan init ...

    // Реєструємо стани
    m_stateManager.registerState(
        GameState::MAIN_MENU, 
        std::make_unique<MainMenuState>(this)
    );
    m_stateManager.registerState(
        GameState::PLAYING, 
        std::make_unique<PlayingState>(this)
    );
    m_stateManager.registerState(
        GameState::PAUSED, 
        std::make_unique<PausedState>(this)
    );
    m_stateManager.registerState(
        GameState::SETTINGS, 
        std::make_unique<SettingsState>(this, &m_config)
    );
}

void Engine::run() {
    while (!glfwWindowShouldClose(m_window.getWindow())) {
        // Оновлення стану
        float deltaTime = calculateDeltaTime();
        m_stateManager.update(deltaTime);
        m_stateManager.handleInput();

        // Рендеринг через поліморфні хуки
        IGameState* currentState = m_stateManager.getState(
            m_stateManager.getCurrentState()
        );

        if (currentState->shouldRenderWorld()) {
            drawFrame();  // Рендерить 3D світ
        }

        // Рендеринг UI
        if (UIElement* uiRoot = currentState->getUIRoot()) {
            // UI рендериться в drawFrame
        }
    }
}
```

## Дивіться також

- [[GameState\|GameState]] — базовий інтерфейс
- [[StateManager\|StateManager]] — управління станами
- [[MainMenuState\|MainMenuState]] — головне меню
- [[PlayingState\|PlayingState]] — ігровий процес
- [[PausedState\|PausedState]] — меню паузи
- [[SettingsState\|SettingsState]] — налаштування

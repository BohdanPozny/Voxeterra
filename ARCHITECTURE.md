# Voxterra - Архітектура з підтримкою модів

## Концепція

Voxterra розроблена як **модульна система**, де основний код є **Core Mod (Ядро)**, а вся функціональність може бути розширена через моди.

## ООП Принципи

### 1. ПОЛІМОРФІЗМ
- Базові абстрактні класи з віртуальними методами
- `UIElement` - базовий клас для всіх UI елементів
- `IGameState` - інтерфейс для станів гри
- `IMod` (TODO) - інтерфейс для модів

### 2. НАСЛІДУВАННЯ
- `UIButton : public UIElement`
- `UILabel : public UIElement`
- `UIPanel : public UIElement`
- `MainMenuState : public IGameState`
- `PlayingState : public IGameState`

### 3. КОМПОЗИЦІЯ
- `UIElement` містить `vector<unique_ptr<UIElement>>` (ієрархія UI)
- `StateManager` містить `map<GameState, unique_ptr<IGameState>>`
- `Engine` містить всі системи (Vulkan, World, StateManager)

### 4. ІНКАПСУЛЯЦІЯ
- Private/protected поля
- Public getters/setters
- Контрольований доступ до API

## Поточна архітектура (для курсової)

### Структура проекту
```
Voxterra/
├── src/
│   ├── Engine.cpp          # Головний движок
│   ├── Vulkan/             # Vulkan рендеринг
│   ├── World/              # Воксельний світ (генерація)
│   ├── UI/                 # UI система (ООП)
│   └── States/             # Стани гри (меню, гра, пауза)
├── headers/
│   ├── UI/                 # UI класи
│   ├── States/             # Стани
│   └── Vulkan/             # Vulkan обгортки
└── config.json             # Налаштування
```

### Основні компоненти
1. **Vulkan рендеринг** - 3D воксельний світ
2. **UI система** - меню з ООП (наслідування, поліморфізм)
3. **State Machine** - управління станами гри
4. **Config система** - JSON налаштування
5. **Генерація світу** - процедурні чанки

## Поточна структура

### UI Система (ООП)
```
UIElement (abstract)
├── UILabel
├── UIButton
└── UIPanel (композиція)
```

### State Machine
```
IGameState (interface)
├── MainMenuState
├── PlayingState
├── PausedState
└── SettingsState
```

### Vulkan Pipeline
```
Engine
├── Instance
├── Device
├── Swapchain
├── RenderPass
├── Pipeline
├── CommandPool
└── UIRenderer (NEW)
```

## Наступні кроки

1. ✅ UI система з ООП
2. ✅ UIRenderer (базовий)
3. 🔄 Інтеграція UI в меню
4. ⏳ ModLoader система
5. ⏳ Mod API
6. ⏳ Plugin система для блоків
7. ⏳ Event система для модів

## Переваги архітектури

- **Розширюваність** - легко додавати нову функціональність
- **Модульність** - кожна система незалежна
- **Підтримка модів** - community content
- **ООП** - чистий, зрозумілий код
- **Поліморфізм** - гнучкість через інтерфейси

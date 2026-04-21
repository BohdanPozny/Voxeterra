# Voxeterra Documentation

## Документація проєкту Voxeterra

Ця документація призначена для імпорту в **Obsidian** або перегляду як Markdown файлів.

## Структура документації

```
doc/
├── README.md           # Цей файл
├── index.md            # Головна сторінка з оглядом
├── architecture.md     # Архітектура системи
├── Engine.md           # Центральний клас Engine
│
├── vulkan/             # Vulkan підсистема
│   ├── index.md        # Огляд Vulkan модулів
│   ├── Instance.md
│   ├── Device.md
│   ├── Swapchain.md
│   ├── RenderPass.md
│   ├── Framebuffer.md
│   ├── DepthBuffer.md
│   ├── CommandPool.md
│   ├── FrameSync.md
│   ├── Pipeline.md
│   └── WorldRenderer.md
│
├── world/              # Воксельний світ
│   ├── index.md        # Огляд системи світу
│   ├── World.md
│   ├── Chunk.md
│   └── Voxel.md
│
├── ui/                 # UI система
│   ├── index.md        # Огляд UI
│   ├── UIElement.md
│   └── UIButton.md
│
├── states/             # Стани гри
│   └── index.md        # State Machine
│
├── input/              # Ввід
│   └── index.md        # InputManager
│
└── config/             # Конфігурація
    └── index.md        # Config клас
```

## Як використовувати в Obsidian

1. **Відкрийте папку `doc/` як Vault в Obsidian:**
   - `File` → `Open folder as vault`
   - Виберіть `/home/bohdanpoz/myProgram/cpp/Voxeterra/doc`

2. **Навігація:**
   - Використовуйте вікі-посилання `[[filename]]` для переходу між сторінками
   - Граф (Graph view) показує зв'язки між компонентами

3. **Почніть з:**
   - `[[index]]` — огляд всього проєкту
   - `[[architecture]]` — архітектурна діаграма
   - `[[Engine]]` — центральний клас

## Формат документації

- **Вікі-посилання:** `[[PageName]]` для навігації
- **Код:** Блоки коду з підсвіткою
- **Таблиці:** Для параметрів та порівнянь
- **Діаграми:** ASCII-art для структур

## Конвенції

- Всі посилання на файли використовують абсолютні шляхи від кореня `doc/`
- Назви файлів відповідають назвам класів
- Індексні файли (`index.md`) містять огляд розділу

## Корисні посилання для початку

- [[index\|Головна сторінка]]
- [[architecture\|Архітектура системи]]
- [[Engine\|Клас Engine]]
- [[vulkan/index\|Vulkan підсистема]]
- [[world/index\|Воксельний світ]]

---

*Документація згенерована автоматично з аналізу кодової бази Voxeterra.*

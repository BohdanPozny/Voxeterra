# Voxeterra - Документація

## Огляд проєкту

**Voxeterra** — це C++ voxel engine, побудований на базі **Vulkan API**. Проєкт реалізує 3D воксельний світ з генерацією чанків, системою станів гри, UI інтерфейсом та повноцінним рендерингом через Vulkan.

## Структура документації

| Розділ                              | Опис                                               |
| ----------------------------------- | -------------------------------------------------- |
| [[architecture\|Архітектура]]       | Загальна архітектура системи, діаграми компонентів |
| [[vulkan/index\|Vulkan Підсистема]] | Всі Vulkan-обгортки та графічні компоненти         |
| [[world/index\|Світ та Вокселі]]    | Генерація світу, чанки, типи вокселів              |
| [[ui/index\|UI Система]]            | Ієрархія UI елементів, рендеринг інтерфейсу        |
| [[states/index\|Стани Гри]]         | State Machine: MainMenu, Playing, Paused, Settings |
| [[input/index\|Система Вводу]]      | InputManager, обробка клавіатури та миші           |
| [[config/index\|Конфігурація]]      | Config клас, JSON налаштування                     |

## Швидкий старт

### Збірка проєкту

```bash
cd /home/bohdanpoz/myProgram/cpp/Voxeterra
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./Voxterra
```

### Залежності

- **Vulkan** — графічний API
- **GLFW** — віконна система та ввід
- **GLM** — математична бібліотека для 3D

### Файлова структура

```
Voxeterra/
├── headers/          # Заголовкові файли (.hpp)
│   ├── Core/        # Основні класи (Engine, Config, etc.)
│   ├── Vulkan/      # Vulkan обгортки
│   ├── World/       # Світ та вокселі
│   ├── States/      # Стани гри
│   ├── UI/          # UI компоненти
│   └── Input/       # Ввід
├── src/             # Реалізація (.cpp)
│   ├── Vulkan/
│   ├── World/
│   ├── States/
│   ├── UI/
│   └── Input/
├── shaders/         # SPIR-V шейдери
├── fonts/           # TTF шрифти
├── main.cpp         # Точка входу
└── config.json      # Файл налаштувань
```

## Ключові концепції

### 1. Vulkan Resource Management

Всі Vulkan ресурси обгорнуті в класи з RAII підходом:
- `Instance` — VkInstance
- `Device` — VkPhysicalDevice + VkDevice
- `Swapchain` — VkSwapchainKHR + VkImageView[]
- `Pipeline` — VkPipeline + VkPipelineLayout
- `Buffer` — VkBuffer + VkDeviceMemory

### 2. Frame Synchronization

Подвійна буферизація (max_frames_in_flight = 2):
- Semaphores: `imageAvailable`, `renderFinished`
- Fences для CPU-GPU синхронізації

### 3. Voxel World

- Чанк: 16×16×16 вокселів
- Зберігання: `std::unordered_map<ChunkPos, Chunk>`
- Mesh generation: face culling для видимих граней

### 4. State Machine

```
┌──────────┐    New Game     ┌──────────┐
│MainMenu  │ ───────────────> │ Playing  │
└──────────┘                  └────┬─────┘
      ↑                            │
      └────────────────────────────┘
           ESC (Pause Menu)
```

## Конфігурація (config.json)

```json
{
  "graphics": {
    "windowWidth": 800,
    "windowHeight": 600,
    "fov": 70.0,
    "renderDistance": 8,
    "vsync": true,
    "fullscreen": false
  },
  "controls": {
    "mouseSensitivity": 0.1,
    "movementSpeed": 10.0
  },
  "audio": {
    "masterVolume": 1.0,
    "musicVolume": 0.7,
    "sfxVolume": 0.8
  },
  "ui": {
    "font": "fonts/default.ttf",
    "fontSize": 16
  }
}
```

## API Константи

| Константа | Значення | Опис |
|-----------|----------|------|
| `CHUNK_SIZE` | 16 | Розмір чанку по кожній осі |
| `MAX_FRAMES_IN_FLIGHT` | 2 | Кількість кадрів "в польоті" |
| `VOXEL_SIZE` | 1.0f | Розмір одного вокселя |

## Ліцензія

Проєкт розповсюджується під відкритою ліцензією. Деталі дивіться у файлі LICENSE.

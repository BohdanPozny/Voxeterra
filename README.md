# Voxeterra - Воксельний рушій

Воксельний рушій на базі Vulkan SDK та GLFW.

## Залежності

- Vulkan SDK
- GLFW3
- GLM
- CMake 3.15+
- C++20 компілятор

## Збірка проєкту

### NixOS (рекомендовано)

```bash
# Увійти в nix-shell
nix-shell

# Компіляція шейдерів
cd shaders
chmod +x compile.sh
./compile.sh
cd ..

# Збірка проєкту
rm -rf build && mkdir build
cmake -S . -B build
cmake --build build

# Запуск
./build/Voxterra
```

### Інші системи

```bash
# Компіляція шейдерів
cd shaders
glslc triangle.vert -o triangle.vert.spv
glslc triangle.frag -o triangle.frag.spv
cd ..

# Збірка
mkdir build
cd build
cmake ..
make

# Запуск
./Voxterra
```

## Структура проєкту

```
Voxeterra/
├── headers/           # Заголовкові файли
│   ├── Vulkan/       # Vulkan компоненти
│   ├── Window.hpp
│   └── Engine.hpp
├── src/              # Реалізація
│   └── Vulkan/
├── shaders/          # GLSL шейдери
├── main.cpp          # Точка входу
└── CMakeLists.txt
```

## Поточний стан

### Реалізовано:
- ✅ Engine клас з композицією всіх компонентів
- ✅ Window система (GLFW)
- ✅ Vulkan Instance
- ✅ Device (фізичний та логічний)
- ✅ Swapchain
- ✅ Render Pass
- ✅ Framebuffers
- ✅ Graphics Pipeline
- ✅ Shader система
- ✅ Command Buffers
- ✅ Synchronization objects
- ✅ Render loop з базовим трикутником

### В розробці:
- Vertex/Index buffers
- Uniform buffers
- Camera система
- Тестова геометрія

### Заплановано:
- Воксельна система
- Chunk manager
- Procedural generation
- Оптимізації рендерингу

## Ліцензія

GPL-3.0

# Voxeterra

A Vulkan-based voxel engine written in modern C++20. Cross-platform: builds on
Linux, Windows, and macOS (via MoltenVK).

## Features

- Vulkan 1.2 renderer with separate 3D world and 2D UI pipelines
- Chunked world with binary greedy meshing
- Streaming chunk loader around the camera
- TTF font atlas + text rendering
- Simple state machine (main menu, playing, paused, settings)
- JSON-backed user configuration

## Dependencies

- Vulkan SDK (includes `glslc`)
- GLFW 3
- GLM
- CMake 3.16+
- A C++20 compiler (GCC 11+, Clang 13+, MSVC 19.3+)
- macOS: MoltenVK (shipped with the Vulkan SDK for macOS)

## Building

Shaders are compiled automatically by CMake, and runtime assets (`fonts/`,
`config.json`) are copied next to the executable.

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/Voxterra
```

### Windows (Visual Studio)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./build/Release/Voxterra.exe
```

Make sure `VULKAN_SDK` is set so CMake can locate `glslc`.

### NixOS

```bash
nix-shell
cmake -S . -B build && cmake --build build -j
./build/Voxterra
```

## Project layout

```
Voxeterra/
├── headers/          # Public headers
│   ├── Vulkan/       # Vulkan wrappers (Instance, Device, Swapchain, ...)
│   ├── World/        # Voxel, Chunk, World, WorldRenderer
│   ├── UI/           # UIElement, UIPanel, UIButton, UILabel
│   ├── States/       # MainMenu / Playing / Paused / Settings
│   ├── Input/        # InputManager
│   └── utils/        # BitUtils (portable ctz)
├── src/              # Implementation mirroring headers/
├── shaders/          # GLSL sources (compiled to SPIR-V at build time)
├── fonts/            # TTF fonts copied next to the executable
├── config.json       # User configuration
├── main.cpp          # Entry point
└── CMakeLists.txt
```

## Configuration

`config.json` is loaded on startup and saved when the settings screen exits.
Delete the file to regenerate it with defaults.

## License

GPL-3.0. See `LICENSE`.

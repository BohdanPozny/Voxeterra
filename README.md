# Voxeterra

A Vulkan-based voxel engine written in modern C++20. Cross-platform: builds on
Linux, Windows, and macOS (via MoltenVK).

## Features

- Vulkan 1.2 renderer with separate 3D world and 2D UI pipelines
- Chunked world with binary greedy meshing; one voxel is 0.1 m
- Streaming chunk loader around the camera
- TTF font atlas + text rendering
- Simple state machine (main menu, playing, paused, settings)
- Settings screen: view distance, FPS limit, VSync, movement speed, key bindings
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

## Profiling (Tracy)

Instrumentation is compiled out by default. To enable it (CMake fetches the
Tracy v0.13.1 client, so the first configure needs network access):

```bash
cmake -S . -B build-tracy -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOXETERRA_TRACY=ON
cmake --build build-tracy -j
./build-tracy/Voxterra &
tracy            # GUI from shell.nix; must be the same version (0.13.1)
```

CPU zones cover the frame loop, chunk streaming, generation and meshing; GPU
zones time the world and UI passes; plots track loaded/visible chunks, and
the "Vulkan buffers" memory pool tracks buffer allocations.

## Project layout

```
Voxeterra/
├── headers/          # Public headers
│   ├── Vulkan/       # Vulkan wrappers (Instance, Device, Swapchain, ...)
│   ├── World/        # Voxel, Chunk, World, WorldRenderer
│   ├── UI/           # UIElement, UIPanel, UIButton, UILabel
│   ├── States/       # MainMenu / Playing / Paused / Settings
│   ├── Input/        # InputManager, key names
│   └── utils/        # BitUtils (portable ctz)
├── src/              # Implementation mirroring headers/
├── shaders/          # GLSL sources (compiled to SPIR-V at build time)
├── fonts/            # TTF fonts copied next to the executable
├── config.json       # User configuration
├── main.cpp          # Entry point
└── CMakeLists.txt
```

## Controls

Movement defaults to W/A/S/D, Space (up) and Left Shift (down); the mouse
looks around. Esc opens the pause menu, F3 toggles the debug overlay.

## Configuration

`config.json` is loaded on startup from the executable's directory. If it is
missing, a file with default values is written there. The Settings screen
(main menu or pause menu) edits it and applies changes immediately:

- **View distance**: 2 chunks and up, no upper limit; a chunk is 64³ voxels,
  i.e. 6.4 m. Steps by 1 up to 16, then by 4. Memory grows with the square of
  the distance (~260 KB per non-empty chunk, 4 chunks per column).
- **FPS limit**: Unlimited or 30..240 (`fpsLimit`, 0 = unlimited).
- **Speed**: camera movement speed, 1..200 m/s (`movementSpeed`).
- **VSync**: FIFO when on, mailbox/immediate when off.
- **Keys**: click a binding, then press a key (Esc cancels). Binding a key
  that is already used swaps the two actions. Stored as GLFW key codes in
  `controls.keys`.

World units are metres: camera position, `movementSpeed` (m/s) and terrain
are expressed in metres, and the voxel grid is scaled by 0.1 when rendered.
Debug builds enable `VK_LAYER_KHRONOS_validation` when it is installed.

On a fresh clone `config.json` does not exist yet; CMake copies
`config.json.example` instead.

## License

GPL-3.0. See `LICENSE`.

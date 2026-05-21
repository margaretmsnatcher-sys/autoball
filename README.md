# Autoball

**Codename: AUTOBALL**

A Rocket League-style 3v3 carball game written in C11 using SDL3 and the SDL3 GPU API.
Supports human players and bot AI. Future vision: merge a Twisted Metal-style weapon/combat
layer to make Autoball its own unique game.

Inspired by [Hacker League](https://github.com/moritztng/hacker-league).

---

## Tech Stack

| Layer | Choice |
|---|---|
| Language | C11 |
| Windowing / Input | SDL3 |
| 3-D Rendering | SDL3 GPU API (Vulkan / D3D12 / Metal) |
| Physics | Custom (no external lib) |
| Networking | TBD |
| Build System | CMake 3.20+ |

---

## Prerequisites

- **CMake** 3.20+
- **SDL3** (installed or built from source)
- **glslc** (from the Vulkan SDK) — for compiling shaders
- **Visual Studio 2022** or **Clang** on Windows

### Install SDL3 (Windows, vcpkg)
```
vcpkg install sdl3:x64-windows
```

Or download pre-built binaries from https://github.com/libsdl-org/SDL/releases

---

## Build

### 1. Compile shaders first
```
compile_shaders.bat
```
This produces `shaders/compiled/world.vert.spv` and `shaders/compiled/world.frag.spv`.

### 2. Configure and build
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### 3. Run
```
build\bin\autoball.exe
```

---

## Controls (Keyboard)

| Key | Action |
|---|---|
| W / S | Throttle forward / reverse |
| A / D | Steer left / right |
| Space | Jump (press twice for double-jump) |
| Left Shift | Boost |
| X | Handbrake / powerslide |
| Arrow Up/Down | Pitch (in air) |
| Arrow Left/Right | Roll (in air) |
| R | Restart match |
| ESC | Quit |
| F3 | Toggle debug HUD |

Gamepad is also supported (Xbox layout).

---

## Project Structure

```
autoball/
├── CMakeLists.txt
├── include/            # All public headers
├── src/                # C source files
├── shaders/            # GLSL source + compiled SPIR-V
├── assets/             # Textures, sounds (future)
└── docs/               # Design documents
```

---

## Roadmap

- **Phase 1** (current): Core carball — physics, 3v3 bots, SDL3 GPU rendering
- **Phase 2**: Polish — particle effects, proper models, sound assets, replay system
- **Phase 3**: Networking — online multiplayer
- **Phase 4**: Twisted Metal combat layer — weapons, health, explosions
- **Phase 5**: Game modes — standard, combat, deathmatch, rumble
- **Phase 6**: Release

---

## License

MIT — see LICENSE file.

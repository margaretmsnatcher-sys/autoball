# Autoball – Project Steering

## What is Autoball?
Codename **AUTOBALL**. A Rocket League-style **3v3 carball** game written in **C (C11)** using **SDL3** and the **SDL3 GPU API** for 3-D rendering. Supports both human players and bot AI. Inspired by the open-source [Hacker League](https://github.com/moritztng/hacker-league) codebase.

**Future vision:** Merge a **Twisted Metal**-style weapon/combat layer into the carball concept to make Autoball its own unique game — not a Rocket League clone.

---

## Tech Stack
| Layer | Choice |
|---|---|
| Language | C11 |
| Windowing / Input | SDL3 |
| 3-D Rendering | SDL3 GPU API (Vulkan / D3D12 / Metal backend) |
| Physics | Custom (in-engine, no external lib) |
| Networking | TBD (future milestone) |
| Build System | CMake 3.20+ |
| Platform | Windows first, cross-platform ready |

---

## Project Structure
```
autoball/
├── CMakeLists.txt          # Build system
├── include/                # All public headers
│   ├── autoball.h          # Master include + all constants
│   ├── math3d.h            # Vec2/3/4, Mat4, Quat, AABB
│   ├── physics.h           # Rigid body, collision API
│   ├── car.h               # Car entity, CarInput, TeamID
│   ├── ball.h              # Ball entity
│   ├── arena.h             # Arena geometry, goals, boost pads
│   ├── bot.h               # Bot AI (role-based FSM)
│   ├── match.h             # Match state machine
│   ├── input.h             # Keyboard + gamepad
│   ├── renderer.h          # SDL3 GPU renderer, Camera
│   ├── hud.h               # Scoreboard, timer, boost meter
│   └── audio.h             # SDL3 audio, SFX
├── src/                    # Implementations
│   ├── main.c              # Game loop (fixed timestep)
│   ├── math3d.c            # Mat4 + Quat math  ✅ DONE
│   ├── physics.c           # TODO
│   ├── car.c               # TODO
│   ├── ball.c              # TODO
│   ├── arena.c             # TODO
│   ├── bot.c               # TODO
│   ├── match.c             # TODO
│   ├── input.c             # TODO
│   ├── renderer.c          # TODO
│   ├── hud.c               # TODO
│   └── audio.c             # TODO
├── shaders/
│   ├── world.vert.glsl     # TODO
│   ├── world.frag.glsl     # TODO
│   └── compiled/           # SPIR-V / DXIL output
├── assets/
│   ├── textures/
│   └── sounds/
└── docs/
```

---

## Session 1 – Completed (May 21, 2026)

### All headers ✅ (`include/`)
- `autoball.h` — master constants: arena dims, ball, car, match, bot skill levels, future weapon slots
- `math3d.h` — full linear algebra: Vec2, Vec3, Vec4, Mat4 (column-major), Quat, AABB, all inline ops
- `physics.h` — RigidBody struct, CollisionResult, full collision + resolution API signatures
- `car.h` — Car struct (physics body, boost, jump, double-jump, air control, stats, team, type), CarInput
- `ball.h` — Ball struct
- `arena.h` — Arena struct, 34 boost pads (6 large / 28 small), goal detection, GoalResult enum
- `bot.h` — BotState (role FSM: attacker/defender/support), skill scaling, ball prediction API
- `match.h` — MatchState (phase enum: pregame/kickoff/play/goal/overtime/postgame), full 3v3 roster
- `input.h` — InputContext (keyboard + SDL_Gamepad), key bindings documented
- `renderer.h` — Renderer (SDL3 GPU device, pipelines, depth buffer, procedural meshes), Camera (chase-cam)
- `hud.h` — HUD (bitmap font atlas, debug overlay toggle)
- `audio.h` — AudioContext, SFXId enum (6 sound effects)

### All source files ✅ (`src/`)
- `main.c` — full game loop: fixed timestep (60 Hz), SDL event handling, restart (R key), ESC to quit
- `math3d.c` — Mat4 (identity, mul, translate, scale, rotate XYZ, perspective, look-at), Quat (axis-angle, mul, norm, rotate, to-mat4, slerp)
- `physics.c` — RigidBody integrate (semi-implicit Euler), impulse/force, sphere-plane/sphere/AABB collision, Baumgarte resolution, arena collision helpers
- `ball.c` — ball_init/reset/update with sub-stepped physics
- `arena.c` — 34 boost pads at world positions, respawn timers, goal detection, boost pickup
- `car.c` — full car controller: ground driving, steering, boost, handbrake, jump, double-jump, air pitch/roll/yaw, wheel contact detection
- `bot.c` — role-based FSM (attacker/defender/support), ball prediction, proportional steering, boost-seeking, skill-scaled noise
- `match.c` — full state machine: pregame→kickoff→play→goal→overtime→postgame, car/ball/boost integration, goal scoring, bot role rebalancing
- `input.c` — keyboard + SDL_Gamepad polling, deadzone, gamepad hot-plug
- `renderer.c` — SDL3 GPU device + swapchain, SPIR-V shader loading, procedural mesh generation (sphere, box, floor), GPU buffer upload, chase camera, full draw frame
- `hud.c` — prototype stdout HUD (Phase 2: bitmap font GPU rendering)
- `audio.c` — SDL3 audio device, procedural sine-wave SFX per event type

### Shaders ✅ (`shaders/`)
- `world.vert.glsl` — MVP transform, normal to world space, color tint
- `world.frag.glsl` — Blinn-Phong lighting (ambient + diffuse + specular), directional sun light

### Build files ✅
- `CMakeLists.txt` — SDL3, asset/shader copy, Windows DLL copy, compiler warnings
- `compile_shaders.bat` — glslc GLSL → SPIR-V compilation script
- `README.md` — build instructions, controls, roadmap

---

## What's Next (Resume Here)

The codebase is **feature-complete for a compilable prototype**. Next steps to get it running:

### 1. Get SDL3 installed
```
vcpkg install sdl3:x64-windows
```
Or download from https://github.com/libsdl-org/SDL/releases

### 2. Get Vulkan SDK (for glslc shader compiler)
Download from https://vulkan.lunarg.com/

### 3. Compile shaders
```
compile_shaders.bat
```

### 4. Build and run
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
build\bin\autoball.exe
```

### 5. First things to fix/tune after first run
- Physics feel: tweak `CAR_TURN_SPEED`, `CAR_BOOST_ACCEL`, `BALL_RESTITUTION` in `autoball.h`
- Bot difficulty: adjust `BOT_SKILL_MEDIUM` constant or pass different value to `match_init`
- Camera: tune `cam->distance` and `cam->pitch` in `camera_init()` in `renderer.c`
- Arena wall collision: currently simplified planes; add corner/ramp geometry in Phase 2

### 6. Phase 2 priorities
- Proper bitmap font for HUD (score, timer, boost bar on screen)
- Particle effects: boost flame trail, goal explosion
- Better car model (not just a box)
- Sound asset files (WAV) replacing procedural tones

---

## Future Roadmap

### Phase 2 – Polish & Playability
- [ ] Proper 3-D car model (OBJ loader or procedural)
- [ ] Particle effects: boost flame, ball sparks, goal explosion
- [ ] Replay system (record inputs, play back)
- [ ] Bot difficulty tuning and smarter aerial play
- [ ] Proper sound assets (WAV files)
- [ ] Settings menu (resolution, volume, key rebinding)

### Phase 3 – Networking
- [ ] Choose networking library (ENet, GameNetworkingSockets, or raw UDP)
- [ ] Client-server or peer-to-peer architecture decision
- [ ] Input prediction + rollback or lockstep
- [ ] Online 3v3 with mixed human/bot lobbies

### Phase 4 – Twisted Metal Combat Layer ⚔️
This is what makes Autoball **unique**. Layered on top of the carball core:
- [ ] Weapon pickups on the arena floor (replace or supplement boost pads)
- [ ] Weapon types: homing missile, machine gun, mine, shield, EMP
- [ ] Car health + armor system (cars can be "demoed" more dramatically)
- [ ] Weapon hardpoints already stubbed in `car.h` (`WEAPON_SLOTS`)
- [ ] Projectile physics (new `projectile.h/c` module)
- [ ] Explosion radius + force impulse (affects ball and other cars)
- [ ] Visual: fire, smoke, damage decals on cars
- [ ] Balance: weapons powerful enough to be fun, not so powerful they break carball

### Phase 5 – Game Modes
- [ ] Standard 3v3 Autoball (pure carball)
- [ ] Combat 3v3 (carball + weapons enabled)
- [ ] Deathmatch (Twisted Metal style, no ball)
- [ ] Rumble mode (random weapon pickups mid-match, like RL Rumble)
- [ ] Custom match settings (time, score limit, weapon toggle)

### Phase 6 – Content & Release
- [ ] Multiple arena skins
- [ ] Car customization (colors, decals)
- [ ] Career / progression system
- [ ] Steam / itch.io release consideration

---

## Key Design Decisions (Locked)
- **C11** — no C++, keeps it lean and educational for father/son project
- **SDL3 GPU** — modern cross-platform GPU API, no OpenGL/Vulkan boilerplate
- **No external physics lib** — custom physics keeps it understandable and hackable
- **Procedural geometry** — no asset pipeline needed for prototype
- **Fixed 60 Hz timestep** — deterministic, replay-friendly, bot-friendly

## Key Design Decisions (Open)
- Networking library (ENet vs GameNetworkingSockets vs custom UDP)
- Shader compilation strategy (pre-compiled SPIR-V vs runtime SDL_ShaderCross)
- Whether to add a scripting layer for bot AI (Lua?) in later phases

---

## Build Instructions (Windows)
```bash
# Prerequisites: CMake 3.20+, Visual Studio 2022 or Clang, SDL3 installed
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
build\bin\autoball.exe
```

## Controls (Default)
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


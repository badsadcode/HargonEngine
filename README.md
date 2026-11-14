# Hargon 3D Game Engine

A cross-platform C++20 3D Game Engine and Visual Editor built around the architecture:
- **Language & Build System**: C++20 & CMake (`CMakeLists.txt`)
- **Application & Windowing**: **GLFW** (`engine/core/Window.h`, `engine/core/Window.cpp`)
- **3D Graphics & RHI**: **NVIDIA NVRHI** (`nvrhi_vk` Vulkan 1.2+ primary cross-platform backend & `nvrhi_d3d11` Direct3D 11 backend on Windows, plus `nvrhi` validation layer) (`engine/rhi/RHIDevice.h`, `engine/renderer/Renderer3D.h`)
- **Audio Engine**: **miniaudio** (`engine/audio/AudioEngine.h`) with procedural 16-bit PCM WAV synthesizer and 2D/3D spatial sound playback
- **Scripting**: **Lua 5.4** (`engine/scripting/ScriptEngine.h`) with per-entity isolated environments, live hot-reloading, and interactive Lua REPL
- **Visual Editor**: **Dear ImGui (Docking) + Custom NVRHI Backend** (`editor/EditorApp.h`, `engine/renderer/ImGuiNVRHI.h`) supporting **"Make the game"** (Scene Hierarchy, Entity Inspector, 3D Viewport with interactive 3D Translate/Rotate/Scale Gizmos, Built-in Lua Script Editor, Live Play/Pause/Stop mode) and **"Export the game"** (One-click standalone game packager).

---

## Playable Game Included: *Starforge Outpost: Crystal Defense*

Rather than stopping at a static rendering demo, Hargon ships with and exports a complete 3D sci-fi action-defense game:
- **Player Hover-Rover (`assets/scripts/player_rover.lua`)**: Drive and steer across the Starforge arena (`W/A/S/D` or Arrow keys), engage turbo boost (`Left Shift`), and fire dual plasma cannons (`Space` or `F`).
- **Enemy Seeker Drones (`assets/scripts/enemy_drone.lua`)**: Multi-wave AI drones that pursue the Player Rover or attack the central Starforge Reactor Core, strafing and firing enemy plasma bolts.
- **Automated Defense Turrets (`assets/scripts/defense_turret.lua`)**: Allied Lua-scripted turrets that scan the arena, lock onto the nearest Enemy Drone, and fire support bursts.
- **Energy Crystals (`assets/scripts/crystal_pickup.lua`)**: Rotating, bobbing crystal clusters with magnetic pull that restore Rover hull/shield and award score points.
- **3D PBR Lighting, Skybox & Particles**: Cook-Torrance GGX PBR shader, procedural starfield/nebula skybox, up to 8 dynamic point lights, emissive materials, and 3D particle bursts.

---

## Building from Source

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Binaries Produced
1. **`build/Release/HargonEditor.exe`** — The Visual Editor (*Make the game* & *Export the game*).
2. **`build/Release/HargonRuntime.exe`** — The Standalone Game Runtime.
3. **`ExportedGame/StarforgeDefender.exe`** — The self-contained exported game distribution produced by the Editor's Exporter.

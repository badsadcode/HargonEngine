# Hargon Engine

A cross-platform **C++20 general-purpose 3D Game Engine & Visual Studio Editor** built around a modular architecture (`GLFW` + `NVIDIA NVRHI` + `miniaudio` + `Lua 5.4` + `Dear ImGui`), supporting independent multi-project development, live in-editor Play/Stop testing, and standalone game packaging.

> Looking for the entertaining, non-technical story of how Hargon was built (including upside-down menus, disappearing universes, and Uncle Wispr)? Read **[DEVLOG.md](DEVLOG.md)**.

---

## Engine Architecture

| Layer | Technology & Role | Location |
|---|---|---|
| **Window & Input** | `GLFW` + Godot-inspired `InputMap` actions + automated test key injection | [Window.h](engine/core/Window.h), [InputMap.h](engine/core/InputMap.h) |
| **Graphics RHI** | `NVIDIA NVRHI` (**Vulkan 1.2+** primary + **D3D11** fallback on Windows, live validation layers) | [RHIDevice.h](engine/rhi/RHIDevice.h) |
| **3D Renderer** | Cook-Torrance GGX PBR shader, 15 procedural surface patterns, **2048×1024 2-Cascade PSSM Directional Shadows**, **3072×1024 8-Slot 6-Face 360° `OmniPointShadowAtlas`**, **Half-Res Bilateral Depth-Prepass SSAO**, procedural skybox, `CPUParticles3D`, `AgX` / `ACES` / `Filmic` / `Reinhard` tonemapping, Godot `PlaneSign` Frustum Culling | [Renderer3D.h](engine/renderer/Renderer3D.h), [PBRLit.hlsl](assets/shaders/PBRLit.hlsl), [SSAO.hlsl](assets/shaders/SSAO.hlsl), [ShadowDepth.hlsl](assets/shaders/ShadowDepth.hlsl) |
| **3D Physics & Nav** | 32-bit `collisionLayer`/`collisionMask` filtering, Sphere-Sphere / Sphere-AABB / AABB-AABB narrowphase, `Area3D` space overrides (gravity/wind/damping) & trigger enter/exit diffing, continuous `RayCast3D` node sensors, `useGravity` + `isOnFloor` platform grounding, XZ Grid A\* navigation (`FindPathGrid`), `SpringArm3D` camera collision avoidance | [PhysicsSystem.h](engine/physics/PhysicsSystem.h) |
| **Scene & Prefabs** | Hierarchical parent-child transforms, `ProcessMode` pause tree filtering, JSON `.hscene` & `.hprefab` (`PackedScene`) serialization, signal dispatch, `UndoRedo` command history | [Scene.h](engine/scene/Scene.h), [SceneSerializer.h](engine/scene/SceneSerializer.h) |
| **Animation & Audio** | `TweenSystem` easing + procedural `AnimationPlayer` tracks; `miniaudio` 32-voice pooled SFX + 3D spatial attenuation (`InverseDistance`, `InverseSquare`, `Logarithmic`) + procedural 16-bit WAV synthesis | [TweenSystem.h](engine/animation/TweenSystem.h), [AudioEngine.h](engine/audio/AudioEngine.h) |
| **Scripting** | **Lua 5.4** with isolated per-entity `_ENV`, hot-reloading, `Engine.raycast`, `Engine.raycast_node`, `Engine.find_path`, `Engine.is_on_floor`, `on_trigger_enter`/`on_trigger_exit` hooks, Godot-style named timers (`create_timer`/`is_timer_ready`), signals, tweens, `play_sound_3d` | [ScriptEngine.h](engine/scripting/ScriptEngine.h) |
| **Project & Modules** | Independent project configs (`.hproject`), starter `SceneTemplates`, pluggable `IGameModule` interface + optional read-only `BadLevelImporter` bridge | [ProjectManager.h](engine/project/ProjectManager.h), [GameModule.h](engine/runtime/GameModule.h), [BadLevelImporter.h](engine/world/BadLevelImporter.h) |
| **Visual Editor** | Dear ImGui Docking Studio (`HargonEditor`): Project Manager (create/open/switch/configure), Hierarchy, Inspector, 3D Transform/Rotate/Scale Gizmos, WorldEnvironment panel, Lua Editor, Prefabs, Export packager | [EditorApp.h](editor/EditorApp.h) |
| **General Runtime** | `HargonRuntime`: Interactive project selector launcher or direct `--project <slug>` / local `project.hproject` standalone execution | [main_runtime.cpp](runtime_app/main_runtime.cpp) |

---

## Independent Projects in Workspace (`projects/`)

| Project Slug | Genre | Module | Highlights |
|---|---|---|---|
| **`ElderSlipgate`** | Single-Player Retro FPS | `ElderSlipgateFPS` | Self-contained 2-campaign (10-level) FPS using optional read-only `external/BadLevel`: 5 weapons, 10 enemy types, Quake-inspired blood & gibs, atmospheric props, menu, settings, QuickSave/QuickLoad. Also builds dedicated `ElderSlipgateFPS.exe`. |
| **`SkywayTrial`** | 3D Gravity Platformer & Time-Trial | `SceneAndLua` | Non-combat elevated-platform obstacle course built from HargonEditor's Template #4 using `skyway_runner.lua`, gravity + `is_on_floor` jumping, platform shadows, moving bumpers, and Solar Beacons. |
| **`StarforgeOutpost`** | Sci-Fi Hover-Rover Wave Defense | `SceneAndLua` | Top-down arena rover & defense turret game protecting a central energy core across waves. |
| **`AetherRealms3D`** | 3D Floating-Island Platformer | `SceneAndLua` | Bright floating sky-sanctum demo showcasing AgX tonemapping, parented Wind Tower rotors, and `SpringArm3D` third-person orbit camera. |

> `external/BadLevel` remains **strictly read-only** (`external/BadLevel`) and is completely optional (`enableBadLevel` flag per project). Non-FPS exports never bundle BadLevel assets.

---

## Building, Running & Exporting

### Prerequisites (Windows)
- CMake 3.20+, Visual Studio 2022 (MSVC C++20), Vulkan SDK (tested with `VulkanSDK 1.4.363.0` + `dxc.exe` for SPIR-V regeneration).

### Compile Release Binaries
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -j 8
```

### Key Executables & CLI Flags
- **Visual Studio Editor**: `.\build\Release\HargonEditor.exe`
  - Optional: `--project <slug>`, `--create-project <slug> <templateIdx>`, `--d3d11`, `--autotest`
- **General-Purpose Runtime**: `.\build\Release\HargonRuntime.exe`
  - Launches interactive **Project Selector** if run with no arguments outside an exported game folder, or run directly with `--project SkywayTrial` / `--project StarforgeOutpost` / `--project ElderSlipgate`
- **Dedicated FPS Standalone Binary**: `.\build\Release\ElderSlipgateFPS.exe`

### Recompiling HLSL Shaders to SPIR-V (when modifying `.hlsl` files)
NVRHI maps Vulkan bindings with register shifts (`b+256`, `t+0`, `s+128`, `u+384`):
```powershell
dxc.exe -T vs_6_0 -E VSMain -spirv -fvk-b-shift 256 0 -fvk-t-shift 0 0 -fvk-s-shift 128 0 -fvk-u-shift 384 0 -fvk-use-dx-layout -Vn g_PBRLit_VS_SPV -Fh assets/shaders/PBRLit_vs_spv.h assets/shaders/PBRLit.hlsl
```

---

## Known Limitations
- **Verified Platform**: Tested and visually verified on Windows x64 (MSVC Release + NVIDIA RTX 3080 Vulkan + D3D11); Linux/macOS targets share standard CMake/GLFW/Vulkan setup but remain untested on real hardware here.
- **Shadows & Skeletal Meshes**: Real-time shadows combine 2-Cascade PSSM directional cascades, an 8-slot 6-face 360° `OmniPointShadowAtlas`, and half-res depth-prepass bilateral SSAO across Vulkan & D3D11; entity geometry uses built-in primitives, procedural structures, and static `.obj` meshes rather than skeletal bone animation.
- **Pre-Evolution Archive**: The pre-Godot v1 source tree is preserved untouched under `backups/`.

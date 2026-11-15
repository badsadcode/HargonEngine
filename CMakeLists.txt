cmake_minimum_required(VERSION 3.20)
project(HargonEngine VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/MP /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# 1. Vulkan SDK
find_package(Vulkan REQUIRED)

# 2. GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# 3. NVIDIA NVRHI (Vulkan + Validation + D3D11 on Windows)
set(NVRHI_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12 OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(NVRHI_WITH_DX11 ON CACHE BOOL "" FORCE)
else()
    set(NVRHI_WITH_DX11 OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(external/nvrhi)

# 4. Embedded Lua 5.4 Static Library
file(GLOB LUA_SOURCES "external/lua/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*(lua\\.c|luac\\.c|onelua\\.c|ltests\\.c)$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC external/lua)

# 5. Dear ImGui (Docking branch + GLFW backend)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
add_library(imgui_static STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
)
target_include_directories(imgui_static PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
target_link_libraries(imgui_static PUBLIC glfw)

# 6. Hargon 3D Game Engine General-Purpose Core Library (Zero Game-Specific Code!)
add_library(HargonEngine STATIC
    engine/core/Log.cpp
    engine/core/Window.cpp
    engine/core/InputMap.cpp
    engine/core/UndoRedo.cpp
    engine/animation/TweenSystem.cpp
    engine/rhi/RHIDevice.cpp
    engine/renderer/Mesh.cpp
    engine/renderer/Renderer3D.cpp
    engine/renderer/ImGuiNVRHI.cpp
    engine/scene/Scene.cpp
    engine/scene/SceneSerializer.cpp
    engine/physics/PhysicsSystem.cpp
    engine/audio/AudioEngine.cpp
    engine/scripting/ScriptEngine.cpp
    engine/project/SceneTemplates.cpp
    engine/project/ProjectManager.cpp
    engine/world/BadLevelImporter.cpp
    engine/runtime/GameSystem.cpp
    engine/runtime/GameModule.cpp
)

target_include_directories(HargonEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/miniaudio"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/nvrhi/include"
)

target_link_libraries(HargonEngine PUBLIC
    nvrhi
    nvrhi_vk
    Vulkan::Vulkan
    Vulkan::Headers
    glfw
    imgui_static
    lua_static
)

if(WIN32)
    target_link_libraries(HargonEngine PUBLIC
        nvrhi_d3d11
        d3d11
        dxgi
        d3dcompiler
        dxguid
        comdlg32
    )
elseif(UNIX)
    target_link_libraries(HargonEngine PUBLIC dl pthread m)
endif()

# 7. Independent Game Project: Elder Slipgate FPS (registers into GameModuleRegistry AND builds its own standalone binary)
add_library(ElderSlipgateProject STATIC
    projects/ElderSlipgate/src/FPSGameManager.cpp
    projects/ElderSlipgate/src/ElderSlipgateModule.cpp
)
target_include_directories(ElderSlipgateProject PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/projects/ElderSlipgate/src"
)
target_link_libraries(ElderSlipgateProject PUBLIC HargonEngine)

add_executable(ElderSlipgateFPS
    projects/ElderSlipgate/src/main_fps.cpp
)
target_link_libraries(ElderSlipgateFPS PRIVATE HargonEngine ElderSlipgateProject)

# 8. Hargon Visual Editor Executable (Zero direct project header dependencies; loads registered modules)
add_executable(HargonEditor
    editor/EditorApp.cpp
    editor/EditorWorkspace.cpp
    editor/commands/EditorCommandSystem.cpp
    editor/panels/HierarchyPanel.cpp
    editor/panels/InspectorPanel.cpp
    editor/panels/ViewportPanel.cpp
    editor/panels/AssetBrowserPanel.cpp
    editor/panels/ScriptEditorPanel.cpp
    editor/panels/ConsolePanel.cpp
    editor/panels/ProjectManagerWindow.cpp
    editor/main_editor.cpp
)
target_link_libraries(HargonEditor PRIVATE
    HargonEngine
    $<LINK_LIBRARY:WHOLE_ARCHIVE,ElderSlipgateProject>
)

# 9. Hargon General-Purpose Standalone Game Runtime Executable (Zero direct project header dependencies)
add_executable(HargonRuntime
    runtime_app/main_runtime.cpp
)
target_link_libraries(HargonRuntime PRIVATE
    HargonEngine
    $<LINK_LIBRARY:WHOLE_ARCHIVE,ElderSlipgateProject>
)

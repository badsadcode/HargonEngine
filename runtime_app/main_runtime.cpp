#include "../engine/core/Window.h"
#include "../engine/core/Log.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/runtime/GameSystem.h"
#include "../engine/runtime/FPSGameManager.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <algorithm>

int main(int argc, char** argv)
{
    Hargon::Log::Init();

    Hargon::GameProjectConfig projectCfg;
    projectCfg.gameTitle = "Elder Slipgate: Quake-Style Single-Player FPS";
    if (std::filesystem::exists("game.json"))
        Hargon::SceneSerializer::LoadProjectConfig(projectCfg, "game.json");
    else if (std::filesystem::exists("assets/game.json"))
        Hargon::SceneSerializer::LoadProjectConfig(projectCfg, "assets/game.json");

    Hargon::GraphicsBackend backend = (projectCfg.preferredBackend == "D3D11")
        ? Hargon::GraphicsBackend::D3D11
        : Hargon::GraphicsBackend::Vulkan;

    bool enableValidation = true;
    bool autoTest = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc)
        {
            std::string b = argv[++i];
            backend = (b == "d3d11" || b == "dx11") ? Hargon::GraphicsBackend::D3D11 : Hargon::GraphicsBackend::Vulkan;
        }
        else if (arg == "--d3d11")
        {
            backend = Hargon::GraphicsBackend::D3D11;
        }
        else if (arg == "--autotest")
        {
            autoTest = true;
        }
    }

    Hargon::WindowConfig winCfg;
    winCfg.title = "Elder Slipgate: Quake-Style Single-Player FPS (Hargon Engine + BadLevel)";
    winCfg.width = std::max(1280u, projectCfg.windowWidth);
    winCfg.height = std::max(720u, projectCfg.windowHeight);
    winCfg.resizable = true;

    Hargon::Window window;
    if (!window.Init(winCfg))
        return 1;

    Hargon::RHIDevice rhi;
    if (!rhi.Init(&window, backend, enableValidation))
        return 1;

    Hargon::Renderer3D renderer;
    if (!renderer.Init(&rhi))
        return 1;

    Hargon::ImGuiNVRHI imgui;
    if (!imgui.Init(&rhi, &window))
        return 1;

    Hargon::AudioEngine audio;
    audio.Init("assets/audio");

    Hargon::Scene scene;
    Hargon::ScriptEngine scripts;
    scripts.Init(&scene, &window, &audio);
    scripts.SetParticleCallback([&renderer](const glm::vec3& pos, const glm::vec3& col, int count, float speed) {
        renderer.SpawnParticleBurst(pos, col, count, speed);
    });

    Hargon::FPSGameManager fpsGame;
    fpsGame.Init(&scene, &renderer, &audio, &window, true);

    Hargon::CameraView gameCamera;
    fpsGame.UpdateFirstPersonCamera(gameCamera, 0.0f);

    nvrhi::CommandListHandle cmd = rhi.GetDevice()->createCommandList();
    auto lastTime = std::chrono::steady_clock::now();
    float totalTime = 0.0f;
    int frameCount = 0;

    while (!window.ShouldClose())
    {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        dt = std::clamp(dt, 0.001f, 0.05f);
        totalTime += dt;
        ++frameCount;

        window.PollEvents();

        if (window.WasResized())
        {
            rhi.Resize(window.GetWidth(), window.GetHeight());
            window.ClearResizedFlag();
        }

        if (autoTest)
        {
            dt = 1.0f / 60.0f; // deterministic timestep for autotest

            if (frameCount == 5)
            {
                // Show Main Menu / Settings / Save-Load Menu over the 3D BadLevel Gothic hall
                fpsGame.SetMenuState(Hargon::FPSMenuState::MainMenu);
            }
            else if (frameCount == 20)
            {
                // Enter active gameplay in Campaign 1 Map 1 (c1m1: The Slipgate Sanctum)
                fpsGame.SetMenuState(Hargon::FPSMenuState::None);
                fpsGame.UnlockAllWeaponsForShowcase();
                fpsGame.SelectWeapon(1); // Double-Barrel Buckshot Shotgun
            }
            else if (frameCount >= 22 && frameCount <= 80)
            {
                fpsGame.AimAtNearestVisibleEnemy();
                if (frameCount == 42)
                    fpsGame.SelectWeapon(3); // Ogre Rocket Launcher (gibbing + blood spatter!)
                window.SetSimulatedKey(GLFW_KEY_F, (frameCount % 10) == 2);
                window.SetSimulatedKey(GLFW_KEY_W, (frameCount % 20) < 6);
            }
            else if (frameCount == 82)
            {
                // Test Save Game & Load Game round-trip
                std::string saveMsg, loadMsg;
                fpsGame.SaveGame(1, &saveMsg);
                fpsGame.LoadGame(1, &loadMsg);
            }
            else if (frameCount == 88)
            {
                // Switch to Campaign 2 Map 2 (c2m2: Slag Processing Works - Industrial style)
                fpsGame.LoadCampaignLevel(2, 2, false);
                fpsGame.SetMenuState(Hargon::FPSMenuState::None);
                fpsGame.UnlockAllWeaponsForShowcase();
                fpsGame.SelectWeapon(2); // Super Perforator Nailgun
            }
            else if (frameCount >= 90 && frameCount <= 145)
            {
                fpsGame.AimAtNearestVisibleEnemy();
                if (frameCount == 115)
                    fpsGame.SelectWeapon(4); // Thunderbolt Lightning Shaft
                window.SetSimulatedKey(GLFW_KEY_F, true);
            }
            else if (frameCount >= 150)
            {
                window.RequestClose();
            }
        }

        fpsGame.Step(dt);
        fpsGame.UpdateFirstPersonCamera(gameCamera, dt);

        if (!rhi.BeginFrame())
            continue;

        imgui.BeginFrame();

        // Render FPS HUD & Menu overlays
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        fpsGame.RenderUI(ImVec2(0.0f, 0.0f), displaySize, ImGui::GetBackgroundDrawList());

        cmd->open();

        nvrhi::IFramebuffer* mainFb = rhi.GetCurrentFramebuffer();
        gameCamera.aspectRatio = static_cast<float>(rhi.GetWidth()) / std::max(1.0f, static_cast<float>(rhi.GetHeight()));

        renderer.RenderScene(
            cmd,
            mainFb,
            scene,
            gameCamera,
            totalTime,
            false,
            Hargon::kInvalidEntity,
            Hargon::GizmoMode::Translate,
            -1,
            false);

        imgui.Render(cmd, mainFb);

        cmd->close();
        rhi.FinishFrameAndPresent(cmd);

        if (autoTest)
        {
            if (frameCount == 15)
            {
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_menu_settings.bmp");
            }
            else if (frameCount == 70)
            {
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign1_combat.bmp");
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_exported_game.bmp");
            }
            else if (frameCount == 135)
            {
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign2_combat.bmp");
            }
        }
    }

    rhi.WaitForIdle();
    cmd = nullptr;

    std::cout << "========================================================\n";
    std::cout << "  Elder Slipgate FPS Session Summary (Hargon + BadLevel)\n";
    std::cout << "  RHI Backend       : " << rhi.GetBackendName() << " (" << rhi.GetGPUName() << ")\n";
    std::cout << "  Last Campaign/Map : C" << fpsGame.GetCurrentCampaign() << "M" << fpsGame.GetCurrentLevelIndex() << " (" << fpsGame.GetCurrentLevelTitle() << ")\n";
    std::cout << "  Score             : " << scene.score << "\n";
    std::cout << "  Monsters Slain    : " << scene.enemiesKilled << "\n";
    std::cout << "  Sounds Played     : " << audio.GetPlayedSoundCount() << "\n";
    std::cout << "  Engine Errors     : " << Hargon::Log::GetErrorCount() << "\n";
    std::cout << "========================================================\n";

    scripts.Shutdown();
    audio.Shutdown();
    imgui.Shutdown();
    renderer.Shutdown();
    rhi.Shutdown();
    window.Shutdown();

    return (autoTest && Hargon::Log::GetErrorCount() > 0) ? 1 : 0;
}

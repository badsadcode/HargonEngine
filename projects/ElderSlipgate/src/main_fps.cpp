#include "engine/core/Window.h"
#include "engine/core/Log.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/renderer/Renderer3D.h"
#include "engine/renderer/ImGuiNVRHI.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/project/ProjectManager.h"
#include "engine/runtime/GameModule.h"
#include "engine/audio/AudioEngine.h"
#include "FPSGameManager.h"

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
    Hargon::ProjectManager::InitializeWorkingDirectory(argc > 0 ? argv[0] : nullptr);

    bool autoTest = false;
    bool enableValidation = true;
    Hargon::GraphicsBackend backend = Hargon::GraphicsBackend::Vulkan;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--d3d11" || (arg == "--backend" && i + 1 < argc && std::string(argv[i + 1]) == "d3d11"))
            backend = Hargon::GraphicsBackend::D3D11;
        else if (arg == "--autotest")
            autoTest = true;
    }

    Hargon::GameProjectConfig cfg;
    if (std::filesystem::exists("project.hproject"))
        Hargon::SceneSerializer::LoadProjectConfig(cfg, "project.hproject");
    else if (std::filesystem::exists("projects/ElderSlipgate/project.hproject"))
        Hargon::SceneSerializer::LoadProjectConfig(cfg, "projects/ElderSlipgate/project.hproject");
    else
    {
        cfg.projectName = "ElderSlipgate";
        cfg.gameTitle = "Elder Slipgate";
        cfg.genre = "Single-Player Retro FPS";
        cfg.windowWidth = 1440;
        cfg.windowHeight = 850;
    }

    Hargon::WindowConfig winCfg;
    winCfg.title = cfg.gameTitle + " — Powered by Hargon Engine";
    winCfg.width = std::max(1280u, cfg.windowWidth);
    winCfg.height = std::max(720u, cfg.windowHeight);
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
    Hargon::FPSGameManager fps;
    fps.SetHostManagesCursor(true);
    fps.SetProjectRootDir(cfg.projectRootDir);
    if (!fps.Init(&scene, &renderer, &audio, &window, true))
    {
        Hargon::Log::Error("Failed to initialize Elder Slipgate campaign level!");
        return 1;
    }

    Hargon::CameraView cam;
    fps.UpdateFirstPersonCamera(cam, 0.0f);

    auto cmd = rhi.GetDevice()->createCommandList();
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
            dt = 1.0f / 60.0f;
            if (frameCount == 5)
                fps.SetMenuState(Hargon::FPSMenuState::MainMenu);
            else if (frameCount == 20)
            {
                fps.SetMenuState(Hargon::FPSMenuState::None);
                fps.UnlockAllWeaponsForShowcase();
                fps.SelectWeapon(1);
            }
            else if (frameCount >= 22 && frameCount <= 80)
            {
                fps.AimAtNearestVisibleEnemy();
                if (frameCount == 42) fps.SelectWeapon(3);
                window.SetSimulatedKey(GLFW_KEY_F, (frameCount % 10) == 2);
                window.SetSimulatedKey(GLFW_KEY_W, (frameCount % 20) < 6);
            }
            else if (frameCount == 82)
            {
                std::string s, l;
                fps.SaveGame(1, &s);
                fps.LoadGame(1, &l);
            }
            else if (frameCount == 88)
            {
                fps.LoadCampaignLevel(2, 2, false);
                fps.SetMenuState(Hargon::FPSMenuState::None);
                fps.UnlockAllWeaponsForShowcase();
                fps.SelectWeapon(2);
            }
            else if (frameCount >= 90 && frameCount <= 145)
            {
                fps.AimAtNearestVisibleEnemy();
                if (frameCount == 104) fps.SelectWeapon(4);
                window.SetSimulatedKey(GLFW_KEY_F, true);
                window.SetSimulatedKey(GLFW_KEY_W, false);
            }
            else if (frameCount >= 152)
            {
                window.RequestClose();
            }
        }

        fps.Step(dt);
        window.SetCursorCaptured(!autoTest && window.IsFocused() && fps.WantsCursorCapture());
        fps.UpdateFirstPersonCamera(cam, dt);
        cam.aspectRatio = static_cast<float>(rhi.GetWidth()) / std::max(1.0f, static_cast<float>(rhi.GetHeight()));

        if (!rhi.BeginFrame())
            continue;

        imgui.BeginFrame();
        fps.RenderUI(ImVec2(0.0f, 0.0f), ImVec2(static_cast<float>(rhi.GetWidth()), static_cast<float>(rhi.GetHeight())), ImGui::GetBackgroundDrawList());

        cmd->open();
        nvrhi::IFramebuffer* fb = rhi.GetCurrentFramebuffer();
        renderer.RenderScene(cmd, fb, scene, cam, totalTime, false, Hargon::kInvalidEntity, Hargon::GizmoMode::Translate, -1, false);
        imgui.Render(cmd, fb);
        cmd->close();
        rhi.FinishFrameAndPresent(cmd);

        if (autoTest)
        {
            if (frameCount == 12)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_menu_settings.bmp");
            else if (frameCount == 46)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign1_combat.bmp");
            else if (frameCount == 124)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign2_combat.bmp");
        }
    }

    rhi.WaitForIdle();
    cmd = nullptr;
    audio.Shutdown();
    imgui.Shutdown();
    renderer.Shutdown();
    rhi.Shutdown();
    window.Shutdown();
    return Hargon::Log::HasErrors() ? 1 : 0;
}

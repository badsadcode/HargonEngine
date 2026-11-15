#include "../engine/core/Window.h"
#include "../engine/core/Log.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/project/ProjectManager.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/runtime/GameModule.h"
#include "../engine/runtime/GameSystem.h"
#include <nvrhi/utils.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <memory>

int main(int argc, char** argv)
{
    Hargon::Log::Init();
    // Standalone exports ship their own project.hproject; only seed workspace projects/ in dev checkouts.
    const bool isStandaloneExport = std::filesystem::exists("project.hproject") || std::filesystem::exists("game.json");
    if (!isStandaloneExport && std::filesystem::exists("engine"))
        Hargon::ProjectManager::EnsureBuiltInProjects();

    std::string requestedProject;
    bool enableValidation = true;
    bool autoTest = false;
    std::string backendOverride;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc)
        {
            requestedProject = argv[++i];
        }
        else if (arg == "--backend" && i + 1 < argc)
        {
            backendOverride = argv[++i];
        }
        else if (arg == "--d3d11")
        {
            backendOverride = "d3d11";
        }
        else if (arg == "--autotest")
        {
            autoTest = true;
        }
    }

    Hargon::GameProjectConfig projectCfg;
    bool loadedCfg = false;

    if (!requestedProject.empty())
    {
        std::filesystem::path p(requestedProject);
        if (std::filesystem::is_regular_file(p))
            loadedCfg = Hargon::SceneSerializer::LoadProjectConfig(projectCfg, p.string());
        else if (std::filesystem::is_directory(p) && std::filesystem::exists(p / "project.hproject"))
            loadedCfg = Hargon::SceneSerializer::LoadProjectConfig(projectCfg, (p / "project.hproject").string());
        else if (std::filesystem::exists(std::filesystem::path("projects") / requestedProject / "project.hproject"))
            loadedCfg = Hargon::SceneSerializer::LoadProjectConfig(
                projectCfg, (std::filesystem::path("projects") / requestedProject / "project.hproject").string());
    }
    else if (std::filesystem::exists("project.hproject"))
    {
        loadedCfg = Hargon::SceneSerializer::LoadProjectConfig(projectCfg, "project.hproject");
    }
    else if (std::filesystem::exists("game.json"))
    {
        loadedCfg = Hargon::SceneSerializer::LoadProjectConfig(projectCfg, "game.json");
    }

    std::vector<Hargon::GameProjectConfig> availableProjects;
    if (!loadedCfg && std::filesystem::exists("projects"))
    {
        availableProjects = Hargon::ProjectManager::DiscoverProjects("projects");
        if (autoTest && !availableProjects.empty())
        {
            projectCfg = availableProjects.front();
            loadedCfg = true;
        }
    }

    if (!loadedCfg && availableProjects.empty())
    {
        projectCfg.projectName = "Sandbox";
        projectCfg.gameTitle = "Hargon Engine Runtime";
        projectCfg.genre = "3D Engine";
        projectCfg.gameplayModule = "SceneAndLua";
        projectCfg.windowWidth = 1440;
        projectCfg.windowHeight = 850;
        loadedCfg = true;
    }

    bool inProjectLauncher = !loadedCfg;

    Hargon::GraphicsBackend backend = (projectCfg.preferredBackend == "D3D11")
        ? Hargon::GraphicsBackend::D3D11
        : Hargon::GraphicsBackend::Vulkan;

    if (!backendOverride.empty())
    {
        backend = (backendOverride == "d3d11" || backendOverride == "dx11")
            ? Hargon::GraphicsBackend::D3D11
            : Hargon::GraphicsBackend::Vulkan;
    }

    Hargon::WindowConfig winCfg;
    winCfg.title = inProjectLauncher
        ? "Hargon Runtime — Select Project"
        : (projectCfg.gameTitle + " [" + projectCfg.genre + "] — Powered by Hargon Engine");
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
    Hargon::PhysicsSystem physics;
    Hargon::ScriptEngine scripts;
    scripts.Init(&scene, &window, &audio);
    scripts.SetParticleCallback([&renderer](const glm::vec3& pos, const glm::vec3& col, int count, float speed) {
        renderer.SpawnParticleBurst(pos, col, count, speed);
    });

    Hargon::CameraView gameCamera;
    std::unique_ptr<Hargon::IGameModule> activeModule;

    auto launchProject = [&](const Hargon::GameProjectConfig& cfg) {
        projectCfg = cfg;
        activeModule = Hargon::GameModuleRegistry::Get().CreateModule(projectCfg.gameplayModule);
        Hargon::GameModuleContext ctx{ &scene, &physics, &scripts, &audio, &renderer, &window, &projectCfg };
        if (activeModule)
            activeModule->OnProjectOpen(ctx, true, gameCamera);
        inProjectLauncher = false;
        std::string newTitle = projectCfg.gameTitle + " [" + projectCfg.genre + "] — Powered by Hargon Engine";
        window.SetTitle(newTitle);
        Hargon::Log::Info("HargonRuntime launched project '" + projectCfg.gameTitle + "' (Module: " + projectCfg.gameplayModule + ")");
    };

    if (loadedCfg)
    {
        launchProject(projectCfg);
    }

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

        Hargon::GameModuleContext ctx{ &scene, &physics, &scripts, &audio, &renderer, &window, &projectCfg };

        if (!inProjectLauncher)
        {
            if (autoTest && activeModule)
            {
                dt = 1.0f / 60.0f;
                bool dummyDone = false;
                activeModule->RunAutoTestStep(ctx, rhi, frameCount, dummyDone);
            }

            if (activeModule)
                activeModule->OnStepPlay(ctx, dt, gameCamera);
        }

        gameCamera.aspectRatio = static_cast<float>(rhi.GetWidth()) / std::max(1.0f, static_cast<float>(rhi.GetHeight()));

        if (!rhi.BeginFrame())
            continue;

        imgui.BeginFrame();

        if (inProjectLauncher)
        {
            float w = static_cast<float>(rhi.GetWidth());
            float h = static_cast<float>(rhi.GetHeight());
            ImGui::SetNextWindowPos(ImVec2(w * 0.5f - 320.0f, h * 0.5f - 220.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(640.0f, 440.0f), ImGuiCond_Always);
            if (ImGui::Begin("Hargon Engine Runtime — Select Game Project", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
            {
                ImGui::TextColored(ImVec4(0.4f, 0.88f, 1.0f, 1.0f), "Available Game Projects (projects/)");
                ImGui::Separator();
                ImGui::BeginChild("LauncherProjects", ImVec2(0.0f, 320.0f), true);
                for (const auto& proj : availableProjects)
                {
                    ImGui::PushID(proj.projectName.c_str());
                    if (ImGui::Button("LAUNCH GAME", ImVec2(120.0f, 34.0f)))
                    {
                        launchProject(proj);
                    }
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImGui::Text("%s", proj.gameTitle.c_str());
                    ImGui::TextColored(ImVec4(0.65f, 0.78f, 0.90f, 1.0f), "%s  |  Module: %s", proj.genre.c_str(), proj.gameplayModule.c_str());
                    ImGui::EndGroup();
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                if (ImGui::Button("Exit Runtime", ImVec2(-1.0f, 32.0f)))
                    window.RequestClose();
            }
            ImGui::End();
        }
        else if (activeModule)
        {
            ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();
            activeModule->OnRenderHUD(
                ctx,
                ImVec2(0.0f, 0.0f),
                ImVec2(static_cast<float>(rhi.GetWidth()), static_cast<float>(rhi.GetHeight())),
                bgDrawList);
        }

        cmd->open();
        nvrhi::IFramebuffer* fb = rhi.GetCurrentFramebuffer();

        if (inProjectLauncher)
        {
            nvrhi::utils::ClearColorAttachment(cmd, fb, 0, nvrhi::Color(0.04f, 0.06f, 0.10f, 1.0f));
        }
        else
        {
            renderer.RenderScene(
                cmd,
                fb,
                scene,
                gameCamera,
                totalTime,
                false,
                Hargon::kInvalidEntity,
                Hargon::GizmoMode::Translate,
                -1,
                false);
        }

        imgui.Render(cmd, fb);

        cmd->close();
        rhi.FinishFrameAndPresent(cmd);
    }

    if (!inProjectLauncher)
    {
        std::cout << "========================================================\n";
        std::cout << "  Hargon Engine Project Session Summary\n";
        std::cout << "  Project           : " << projectCfg.gameTitle << " (" << projectCfg.projectName << ")\n";
        std::cout << "  Genre             : " << projectCfg.genre << "\n";
        std::cout << "  Module            : " << projectCfg.gameplayModule << "\n";
        std::cout << "  RHI Backend       : " << rhi.GetBackendName() << " (" << rhi.GetGPUName() << ")\n";
        std::cout << "  Score             : " << scene.score << "\n";
        std::cout << "  Sounds Played     : " << audio.GetPlayedSoundCount() << "\n";
        std::cout << "  Engine Errors     : " << (Hargon::Log::HasErrors() ? "YES" : "0") << "\n";
        std::cout << "========================================================\n";
    }

    rhi.WaitForIdle();
    cmd = nullptr;
    activeModule.reset();
    scripts.Shutdown();
    audio.Shutdown();
    imgui.Shutdown();
    renderer.Shutdown();
    rhi.Shutdown();
    window.Shutdown();

    return Hargon::Log::HasErrors() ? 1 : 0;
}

#include "EditorApp.h"
#include "../engine/core/Log.h"
#include "../engine/core/InputMap.h"
#include "../engine/project/ProjectManager.h"
#include "../engine/project/SceneTemplates.h"
#include "../engine/animation/TweenSystem.h"
#include "../engine/runtime/GameSystem.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace Hargon
{
    bool EditorApp::Init(
        GraphicsBackend backend,
        bool enableValidation,
        bool autoTest,
        const std::string& initialProject,
        int autoCreateTemplate)
    {
        m_AutoTestMode = autoTest;
        Log::Init();

        WindowConfig winCfg;
        winCfg.title = "Hargon Editor";
        winCfg.width = 1600;
        winCfg.height = 920;
        winCfg.resizable = true;
        winCfg.vsync = true;
        if (!m_Window.Init(winCfg))
            return false;

        if (!m_RHI.Init(&m_Window, backend, enableValidation))
        {
            Log::Error("Failed to initialize RHI device.");
            return false;
        }

        m_Audio.Init("assets/audio");
        if (!m_Renderer3D.Init(&m_RHI))
        {
            Log::Error("Failed to initialize Renderer3D.");
            return false;
        }

        if (!m_ImGui.Init(&m_RHI, &m_Window))
        {
            Log::Error("Failed to initialize ImGuiNVRHI.");
            return false;
        }

        m_Scripts.Init(&m_Scene, &m_Window, &m_Audio);
        m_Scripts.SetParticleCallback([this](const glm::vec3& pos, const glm::vec3& col, int count, float speed) {
            m_Renderer3D.SpawnParticleBurst(pos, col, count, speed);
        });

        m_EditorCamera.position = glm::vec3(0.0f, 11.0f, 22.0f);
        m_EditorCamera.forward = glm::normalize(glm::vec3(0.0f, -0.38f, -0.92f));
        m_EditorCamera.fovDegrees = 60.0f;

        // Wire EditorContext before workspace init
        m_Ctx.window = &m_Window;
        m_Ctx.rhi = &m_RHI;
        m_Ctx.audio = &m_Audio;
        m_Ctx.renderer3D = &m_Renderer3D;
        m_Ctx.physics = &m_Physics;
        m_Ctx.scripts = &m_Scripts;
        m_Ctx.imguiLayer = &m_ImGui;
        m_Ctx.commands = &m_Commands;

        m_Ctx.projectConfig = &m_ProjectConfig;
        m_Ctx.scene = &m_Scene;
        m_Ctx.gameModule = &m_GameModule;
        m_Ctx.currentScenePath = &m_CurrentScenePath;
        m_Ctx.playState = &m_PlayState;

        m_Ctx.editorCamera = &m_EditorCamera;
        m_Ctx.editorCamYaw = &m_EditorCamYaw;
        m_Ctx.editorCamPitch = &m_EditorCamPitch;
        m_Ctx.smoothedFPS = &m_SmoothedFPS;

        m_Commands.SetOnMutatedCallback([this]() {
            m_Ctx.PruneInvalidSelection();
        });

        SetupContextCallbacks();
        m_Workspace.Init(m_Ctx);

        // Open initial or default project
        bool opened = false;
        if (!initialProject.empty() && autoCreateTemplate >= 0)
        {
            GameProjectConfig createdCfg;
            std::string status;
            if (ProjectManager::CreateNewProject(initialProject, initialProject, "Custom 3D", autoCreateTemplate, autoCreateTemplate == 3, createdCfg, &status))
                opened = OpenProject(createdCfg);
        }
        else if (!initialProject.empty())
        {
            opened = OpenProjectByInput(initialProject);
        }

        if (!opened)
        {
            auto projects = ProjectManager::DiscoverProjects("projects");
            if (!projects.empty())
            {
                // Prefer AetherRealms3D / non-FPS generic project first, otherwise first discovered
                size_t chosen = 0;
                for (size_t i = 0; i < projects.size(); ++i)
                {
                    if (projects[i].gameplayModule == "SceneAndLua" || projects[i].gameplayModule.empty())
                    {
                        chosen = i;
                        break;
                    }
                }
                opened = OpenProject(projects[chosen]);
            }
        }

        if (!opened)
        {
            m_ProjectConfig.gameTitle = "Untitled Hargon Game";
            m_ProjectConfig.projectName = "UntitledGame";
            m_ProjectConfig.projectRootDir = ".";
            SceneTemplates::BuildAetherRealmsScene(m_Scene);
            m_CurrentScenePath = "untitled.hscene";
            m_Commands.Clear();
            m_Commands.MarkSaved();
        }

        // Strict Editor cursor ownership at startup
        UpdateCursorOwnership();
        UpdateWindowTitle();

        Log::Info("Hargon Editor initialized (" + m_RHI.GetBackendName() + ").");
        return true;
    }

    void EditorApp::SetupContextCallbacks()
    {
        m_Ctx.saveScene = [this]() { return SaveCurrentScene(); };
        m_Ctx.saveSceneAs = [this]() { return SaveCurrentSceneAs(); };
        m_Ctx.openProjectByPath = [this](const std::string& p) { return OpenProjectByInput(p); };
        m_Ctx.openProjectConfig = [this](const GameProjectConfig& cfg) { return OpenProject(cfg); };
        m_Ctx.startPlayMode = [this]() { StartPlayMode(); };
        m_Ctx.pausePlayMode = [this]() { PausePlayMode(); };
        m_Ctx.stopPlayMode = [this]() { StopPlayMode(); };
        m_Ctx.stepPlayMode = [this]() { StepPlayMode(); };

        m_Ctx.openScriptTab = [this](const std::string& relPath, int jumpLine) {
            m_ScriptEditorPanel.OpenScript(m_Ctx, relPath, jumpLine);
            m_Ctx.showScriptEditor = true;
        };
        m_Ctx.instantiateAssetInScene = [this](const std::string& relPath, const glm::vec3* pos) {
            return InstantiateAsset(relPath, pos);
        };
        m_Ctx.applyWorkspacePreset = [this](WorkspacePreset preset) {
            m_Workspace.RequestPreset(preset);
            m_Ctx.SetToast(
                preset == WorkspacePreset::SceneEditing ? "Layout: Scene Editing" :
                preset == WorkspacePreset::Scripting ? "Layout: Scripting" : "Layout: Materials & Lighting");
        };
        m_Ctx.saveNamedLayout = [this](const std::string& name) {
            m_Workspace.SaveNamedLayout(name);
        };
        m_Ctx.loadNamedLayout = [this](const std::string& name) {
            return m_Workspace.LoadNamedLayout(name);
        };
        m_Ctx.resetDefaultLayout = [this]() {
            m_Workspace.RequestPreset(WorkspacePreset::SceneEditing);
            m_Ctx.SetToast("Reset to Default Layout");
        };
        m_Ctx.rebuildUIStyleAndSaveSettings = [this]() {
            m_Workspace.ApplyStyle(m_Ctx.settings.uiScale);
            m_Workspace.SaveSettings(m_Ctx);
        };
        m_Ctx.requestGuardedAction = [this](const PendingUnsavedAction& act) {
            RequestGuardedAction(act);
        };
        m_Ctx.focusSelectedInViewport = [this]() {
            FrameSelectedEntity();
        };
        m_Ctx.hasAnyDirtyScriptTabs = [this]() {
            return m_ScriptEditorPanel.HasAnyDirtyTabs();
        };
        m_Ctx.saveAllDirtyScriptTabs = [this]() {
            return m_ScriptEditorPanel.SaveAllDirtyTabs(m_Ctx);
        };
        m_Ctx.discardDirtyScriptTabs = [this]() {
            m_ScriptEditorPanel.DiscardAllDirtyTabs();
        };
        m_Ctx.getActiveSceneBackupDir = [this]() -> std::string {
            if (m_ProjectConfig.projectRootDir.empty() || m_ProjectConfig.projectRootDir == ".")
                return {};
            return (fs::path(m_ProjectConfig.projectRootDir) / ".hargon" / "backups").generic_string();
        };
        m_Ctx.isSceneDirty = [this]() {
            return m_Commands.IsDirty();
        };
        m_Ctx.buildModuleContext = [this]() {
            return BuildModuleContext();
        };
    }

    GameModuleContext EditorApp::BuildModuleContext()
    {
        GameModuleContext ctx{};
        ctx.scene = &m_Scene;
        ctx.physics = &m_Physics;
        ctx.scripts = &m_Scripts;
        ctx.audio = &m_Audio;
        ctx.renderer = &m_Renderer3D;
        ctx.window = &m_Window;
        ctx.projectConfig = &m_ProjectConfig;
        return ctx;
    }

    bool EditorApp::OpenProjectByInput(const std::string& inputPath)
    {
        GameProjectConfig cfg;
        std::string err;
        if (ProjectManager::LoadProjectFromInput(inputPath, cfg, &err))
            return OpenProject(cfg);

        m_Ctx.SetToast("Failed to load project: " + (err.empty() ? inputPath : err));
        return false;
    }

    bool EditorApp::OpenProject(const GameProjectConfig& config)
    {
        StopPlayMode();
        m_GameModule.reset();

        m_ProjectConfig = config;
        ProjectManager::EnsureProjectFolders(m_ProjectConfig);

        m_Renderer3D.ClearCustomMeshes();
        m_Physics.Reset();
        TweenSystem::Get().Clear();
        m_Scene.Clear();
        m_ScriptEditorPanel.ClearAllTabs();
        m_Ctx.ClearSelection();
        m_Ctx.gameplayMouseLockActive = false;

        m_Scripts.SetProjectRootDir(m_ProjectConfig.projectRootDir);
        if (m_ProjectConfig.gameplayModule != "ElderSlipgateFPS")
            ScriptEngine::EnsureDefaultLuaScripts((fs::path(m_ProjectConfig.projectRootDir) / "scripts").generic_string());

        m_CurrentScenePath = ProjectManager::ResolveProjectAssetPath(m_ProjectConfig, m_ProjectConfig.startupScene);
        if (!m_CurrentScenePath.empty() && fs::exists(m_CurrentScenePath))
        {
            SceneSerializer::LoadFromFile(m_Scene, m_CurrentScenePath);
        }
        else
        {
            m_CurrentScenePath = (fs::path(m_ProjectConfig.projectRootDir) / "scenes" / "main.hscene").generic_string();
            SceneTemplates::BuildEmptyTemplateScene(m_Scene, m_ProjectConfig.gameTitle);
        }

        // Default editor camera framing before module customization
        m_EditorCamera.position = glm::vec3(0.0f, 12.0f, 22.0f);
        m_EditorCamYaw = -90.0f;
        m_EditorCamPitch = -24.0f;
        {
            float yr = glm::radians(m_EditorCamYaw);
            float pr = glm::radians(m_EditorCamPitch);
            m_EditorCamera.forward = glm::normalize(glm::vec3(std::cos(yr) * std::cos(pr), std::sin(pr), std::sin(yr) * std::cos(pr)));
        }

        m_GameModule = GameModuleRegistry::Get().CreateModule(m_ProjectConfig.gameplayModule);
        if (!m_GameModule)
            m_GameModule = GameModuleRegistry::Get().CreateModule("SceneAndLua");

        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            m_GameModule->OnProjectOpen(ctx, false, m_EditorCamera);

            float prefYaw = m_EditorCamYaw;
            float prefPitch = m_EditorCamPitch;
            if (m_GameModule->HasPreferredEditorCameraPose(prefYaw, prefPitch))
            {
                m_EditorCamYaw = prefYaw;
                m_EditorCamPitch = prefPitch;
                float yr = glm::radians(m_EditorCamYaw);
                float pr = glm::radians(m_EditorCamPitch);
                m_EditorCamera.forward = glm::normalize(glm::vec3(std::cos(yr) * std::cos(pr), std::sin(pr), std::sin(yr) * std::cos(pr)));
            }
            else
            {
                // Frame the visible scene geometry nicely from a 3/4 elevated view
                glm::vec3 sumPos(0.0f);
                int meshCount = 0;
                for (const auto& e : m_Scene.GetEntities())
                {
                    if (!e.alive || !e.HasMesh()) continue;
                    float maxS = std::max({ std::abs(e.transform.scale.x), std::abs(e.transform.scale.y), std::abs(e.transform.scale.z) });
                    if (maxS >= 80.0f) continue; // Ignore giant ocean/cloud backdrops
                    sumPos += m_Scene.GetWorldPosition(e);
                    ++meshCount;
                }
                glm::vec3 targetCenter = (meshCount > 0) ? (sumPos / static_cast<float>(meshCount)) : glm::vec3(0.0f);
                targetCenter.y = std::max(0.5f, targetCenter.y);
                m_EditorCamera.position = targetCenter + glm::vec3(12.0f, 15.0f, -30.0f);
                glm::vec3 toTarget = targetCenter - m_EditorCamera.position;
                if (glm::length(toTarget) > 0.01f)
                    m_EditorCamera.forward = glm::normalize(toTarget);

                m_EditorCamPitch = glm::degrees(std::asin(std::clamp(m_EditorCamera.forward.y, -0.999f, 0.999f)));
                m_EditorCamYaw = glm::degrees(std::atan2(m_EditorCamera.forward.z, m_EditorCamera.forward.x));
            }

            EntityID defSel = m_GameModule->PickDefaultSelectedEntity(ctx, m_EditorCamera);
            if (defSel != kInvalidEntity)
                m_Ctx.SelectSingle(defSel);
        }

        m_Scripts.ResetForNewProject(m_Scene);
        m_Commands.Clear();
        m_Commands.MarkSaved();

        // Crucially: switching projects NEVER captures cursor in Editor
        m_Ctx.gameplayMouseLockActive = false;
        UpdateCursorOwnership();
        m_Workspace.UpdateSessionLock(m_Ctx);
        UpdateWindowTitle();
        m_Ctx.SetToast("Opened project: " + m_ProjectConfig.gameTitle);
        return true;
    }

    void EditorApp::UpdateCursorOwnership()
    {
        if (m_AutoTestMode)
        {
            if (m_Window.IsCursorCaptured())
                m_Window.SetCursorCaptured(false);
            return;
        }

        if (!m_Window.IsFocused() || m_PlayState != PlayState::Playing)
            m_Ctx.gameplayMouseLockActive = false;

        bool anyPopup = ImGui::GetCurrentContext() &&
                        ImGui::IsPopupOpen(static_cast<const char*>(nullptr), ImGuiPopupFlags_AnyPopupId);
        if (anyPopup)
            m_Ctx.gameplayMouseLockActive = false;

        bool shouldCapture =
            (m_PlayState == PlayState::Playing) &&
            m_Window.IsFocused() &&
            !anyPopup &&
            m_Ctx.gameplayMouseLockActive &&
            m_GameModule &&
            m_GameModule->WantsCursorCapture();

        if (m_Window.IsCursorCaptured() != shouldCapture)
            m_Window.SetCursorCaptured(shouldCapture);
    }

    bool EditorApp::HasAnyUnsavedWork() const
    {
        return m_Commands.IsDirty() || m_ScriptEditorPanel.HasAnyDirtyTabs();
    }

    void EditorApp::RequestGuardedAction(const PendingUnsavedAction& action)
    {
        if (!HasAnyUnsavedWork())
        {
            ExecutePendingAction(action);
            return;
        }
        m_Ctx.pendingAction = action;
        m_Ctx.showUnsavedPromptModal = true;
    }

    void EditorApp::ExecutePendingAction(const PendingUnsavedAction& action)
    {
        switch (action.type)
        {
        case PendingActionType::OpenProject:
            if (!action.projectConfigArg.projectRootDir.empty())
                OpenProject(action.projectConfigArg);
            else if (!action.stringArg.empty())
                OpenProjectByInput(action.stringArg);
            break;

        case PendingActionType::NewEmptyScene:
            StopPlayMode();
            m_Renderer3D.ClearCustomMeshes();
            m_Physics.Reset();
            TweenSystem::Get().Clear();
            SceneTemplates::BuildEmptyTemplateScene(m_Scene, "Untitled 3D Scene");
            m_CurrentScenePath = (fs::path(m_ProjectConfig.projectRootDir) / "scenes" / "untitled.hscene").generic_string();
            m_Scripts.ResetForNewProject(m_Scene);
            m_Ctx.ClearSelection();
            m_Commands.Clear();
            m_Commands.MarkSaved();
            m_Ctx.SetToast("Created empty 3D scene");
            break;

        case PendingActionType::ApplyTemplate:
            StopPlayMode();
            m_Renderer3D.ClearCustomMeshes();
            m_Physics.Reset();
            TweenSystem::Get().Clear();
            if (action.intArg == 1) SceneTemplates::BuildAetherRealmsScene(m_Scene);
            else if (action.intArg == 2) SceneTemplates::BuildSkywayCourseScene(m_Scene, m_ProjectConfig.gameTitle);
            else if (action.intArg == 3) SceneTemplates::BuildStarforgeOutpostScene(m_Scene);
            else SceneTemplates::BuildEmptyTemplateScene(m_Scene, m_ProjectConfig.gameTitle);
            m_Scripts.ResetForNewProject(m_Scene);
            m_Ctx.ClearSelection();
            m_Commands.Clear();
            m_Commands.MarkDirtyExternally();
            m_Ctx.SetToast("Applied scene template (save to persist)");
            break;

        case PendingActionType::LoadScene:
            LoadSceneFile(action.stringArg);
            break;

        case PendingActionType::ReloadScene:
            LoadSceneFile(m_CurrentScenePath);
            break;

        case PendingActionType::ExitApp:
            m_ExitConfirmed = true;
            m_Window.RequestClose();
            break;

        default:
            break;
        }
        UpdateWindowTitle();
    }

    bool EditorApp::SaveCurrentScene()
    {
        if (m_CurrentScenePath.empty())
            return SaveCurrentSceneAs();

        std::error_code ec;
        fs::path p(m_CurrentScenePath);
        if (p.has_parent_path())
            fs::create_directories(p.parent_path(), ec);

        if (SceneSerializer::SaveToFile(m_Scene, m_CurrentScenePath))
        {
            m_Commands.MarkSaved();
            UpdateWindowTitle();
            m_Ctx.SetToast("Saved scene: " + p.filename().string());
            return true;
        }
        m_Ctx.SetToast("Failed to save scene");
        return false;
    }

    bool EditorApp::SaveCurrentSceneAs()
    {
        std::string defaultDir = (fs::path(m_ProjectConfig.projectRootDir) / "scenes").generic_string();
        std::string chosenPath;
        if (!ProjectManager::BrowseSaveSceneFile(chosenPath, defaultDir))
            return false;

        m_CurrentScenePath = chosenPath;
        if (SceneSerializer::SaveToFile(m_Scene, m_CurrentScenePath))
        {
            m_Commands.MarkSaved();
            m_Workspace.UpdateSessionLock(m_Ctx);
            UpdateWindowTitle();
            m_Ctx.SetToast("Saved scene as: " + fs::path(chosenPath).filename().string());
            return true;
        }
        return false;
    }

    bool EditorApp::LoadSceneFile(const std::string& scenePath)
    {
        if (scenePath.empty()) return false;
        std::string resolved = ProjectManager::ResolveProjectAssetPath(m_ProjectConfig, scenePath);
        if (resolved.empty() || !fs::exists(resolved))
            resolved = scenePath;

        if (!fs::exists(resolved))
            return false;

        StopPlayMode();
        m_Renderer3D.ClearCustomMeshes();
        m_Physics.Reset();
        TweenSystem::Get().Clear();

        if (SceneSerializer::LoadFromFile(m_Scene, resolved))
        {
            m_CurrentScenePath = resolved;
            m_Scripts.ResetForNewProject(m_Scene);
            m_Ctx.ClearSelection();
            m_Commands.Clear();
            m_Commands.MarkSaved();
            m_Workspace.UpdateSessionLock(m_Ctx);
            UpdateWindowTitle();
            m_Ctx.SetToast("Loaded scene: " + fs::path(resolved).filename().string());
            return true;
        }
        return false;
    }

    bool EditorApp::InstantiateAsset(const std::string& relOrAbsPath, const glm::vec3* worldPos)
    {
        std::string resolved = ProjectManager::ResolveProjectAssetPath(m_ProjectConfig, relOrAbsPath);
        if (resolved.empty() || !fs::exists(resolved))
            resolved = relOrAbsPath;

        std::string ext = fs::path(relOrAbsPath).extension().string();
        for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        auto beforeEnts = m_Scene.GetEntities();
        auto beforeSel = m_Ctx.selectedEntities;
        EntityID beforePrimary = m_Ctx.selectedEntity;

        if (ext == ".hprefab")
        {
            glm::vec3 spawnPos = worldPos ? *worldPos : glm::vec3(0.0f);
            EntityID rootId = SceneSerializer::InstantiatePrefab(m_Scene, resolved, spawnPos);
            if (rootId != kInvalidEntity)
            {
                m_Ctx.SelectSingle(rootId);
                m_Commands.RecordSceneSnapshot(
                    m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Instantiate Prefab");
                m_Ctx.SetToast("Instantiated prefab: " + fs::path(relOrAbsPath).filename().string());
                return true;
            }
        }
        else if (ext == ".obj")
        {
            std::string stem = fs::path(relOrAbsPath).stem().string();
            Entity& e = m_Scene.CreateEntity(stem, "Prop");
            e.hasMesh = true;
            e.mesh.enabled = true;
            e.mesh.primitive = MeshPrimitive::CustomOBJ;
            e.mesh.customObjPath = relOrAbsPath;
            if (worldPos)
                e.transform.position = *worldPos;
            m_Ctx.SelectSingle(e.id);
            m_Commands.RecordSceneSnapshot(
                m_Scene, m_Ctx,
                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                "Place OBJ Mesh");
            m_Ctx.SetToast("Placed mesh: " + stem);
            return true;
        }
        return false;
    }

    void EditorApp::StartPlayMode()
    {
        if (m_PlayState == PlayState::Playing)
            return;

        if (m_PlayState == PlayState::Edit)
        {
            m_SavedPlaySceneSnapshot = m_Scene;
            m_HasPlaySnapshot = true;
            m_Physics.Reset();
            TweenSystem::Get().Clear();
            m_Scripts.ResetForNewProject(m_Scene);
        }

        m_PlayState = PlayState::Playing;
        m_Ctx.gameplayMouseLockActive = false;

        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            m_GameModule->OnEnterPlayMode(ctx, true, m_EditorCamera);
        }
        UpdateCursorOwnership();
        m_Ctx.SetToast("Play Mode Started");
    }

    void EditorApp::PausePlayMode()
    {
        if (m_PlayState == PlayState::Playing)
        {
            m_PlayState = PlayState::Paused;
            m_Ctx.gameplayMouseLockActive = false;
            UpdateCursorOwnership();
            m_Ctx.SetToast("Simulation Paused");
        }
        else if (m_PlayState == PlayState::Paused)
        {
            m_PlayState = PlayState::Playing;
            UpdateCursorOwnership();
            m_Ctx.SetToast("Simulation Resumed");
        }
    }

    void EditorApp::StopPlayMode()
    {
        if (m_PlayState == PlayState::Edit)
        {
            UpdateCursorOwnership();
            return;
        }

        m_PlayState = PlayState::Edit;
        m_Ctx.gameplayMouseLockActive = false;

        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            m_GameModule->OnStopPlayMode(ctx);
        }

        if (m_HasPlaySnapshot)
        {
            m_Scene = m_SavedPlaySceneSnapshot;
            m_HasPlaySnapshot = false;
            m_Physics.Reset();
            TweenSystem::Get().Clear();
            m_Scripts.ResetForNewProject(m_Scene);
        }

        // Strict Editor cursor ownership guarantees cursor is visible on stop
        UpdateCursorOwnership();
        m_Ctx.SetToast("Stopped Play Mode (Scene Restored)");
    }

    void EditorApp::StepPlayMode()
    {
        if (m_PlayState == PlayState::Edit)
        {
            StartPlayMode();
            m_PlayState = PlayState::Paused;
        }
        if (m_PlayState == PlayState::Paused)
        {
            float fixedDt = 1.0f / 60.0f;
            if (m_GameModule)
            {
                m_GameModule->OnStepPlay(BuildModuleContext(), fixedDt, m_EditorCamera);
            }
            else
            {
                GameSystem::StepGameplay(m_Scene, m_Physics, m_Scripts, m_Audio, m_Renderer3D, &m_Window, fixedDt);
                GameSystem::UpdateGameCamera(m_Scene, m_EditorCamera, fixedDt);
            }
            m_Ctx.SetToast("Stepped 1 frame (16.7ms)");
        }
    }

    void EditorApp::FrameSelectedEntity()
    {
        if (m_Ctx.selectedEntities.empty())
            return;

        glm::vec3 center(0.0f);
        int count = 0;
        for (EntityID id : m_Ctx.selectedEntities)
        {
            if (const Entity* e = m_Scene.GetEntity(id))
            {
                center += m_Scene.GetWorldPosition(*e);
                ++count;
            }
        }
        if (count > 0)
        {
            center /= static_cast<float>(count);
            m_EditorCamera.position = center - m_EditorCamera.forward * 10.0f;
        }
    }

    void EditorApp::UpdateWindowTitle()
    {
        std::string sceneName = m_CurrentScenePath.empty()
            ? m_Scene.sceneName
            : fs::path(m_CurrentScenePath).filename().string();
        std::string sig = "Hargon Editor - " + m_ProjectConfig.gameTitle + " [" + sceneName + "]" +
                          (HasAnyUnsavedWork() ? "*" : "");
        if (sig != m_LastTitleSignature)
        {
            m_LastTitleSignature = sig;
            m_Window.SetTitle(sig);
        }
    }

    void EditorApp::HandleGlobalShortcuts()
    {
        ImGuiIO& io = ImGui::GetIO();

        // ESC releases FPS mouse capture immediately during Play Mode
        if (m_PlayState == PlayState::Playing && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            m_Ctx.gameplayMouseLockActive = false;
            UpdateCursorOwnership();
        }

        // Play / Stop Play Mode (F5)
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
        {
            if (m_PlayState == PlayState::Edit) StartPlayMode();
            else StopPlayMode();
        }

        if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
            SaveCurrentSceneAs();
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            SaveCurrentScene();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
            m_Ctx.showProjectManagerModal = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false))
            m_Ctx.showExportModal = true;

        if (m_PlayState != PlayState::Edit)
            return;

        // Protect text inputs (Inspector boxes, search filters, Lua Script Editor) from scene hotkeys
        if (io.WantTextInput)
            return;

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (m_Commands.Undo())
                m_Ctx.SetToast("Undo");
        }
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            if (m_Commands.Redo())
                m_Ctx.SetToast("Redo");
        }
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && !m_Ctx.selectedEntities.empty())
        {
            auto beforeEnts = m_Scene.GetEntities();
            auto beforeSel = m_Ctx.selectedEntities;
            EntityID beforePrimary = m_Ctx.selectedEntity;
            std::vector<EntityID> newSel;
            for (EntityID id : m_Ctx.selectedEntities)
            {
                EntityID dupId = m_Scene.DuplicateSubtree(id, glm::vec3(1.5f, 0.0f, 1.5f));
                if (dupId != kInvalidEntity)
                    newSel.push_back(dupId);
            }
            if (!newSel.empty())
            {
                m_Ctx.selectedEntities = newSel;
                m_Ctx.selectedEntity = newSel.back();
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Duplicate Selected");
            }
        }
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false) && !m_Ctx.selectedEntities.empty())
        {
            auto beforeEnts = m_Scene.GetEntities();
            auto beforeSel = m_Ctx.selectedEntities;
            EntityID beforePrimary = m_Ctx.selectedEntity;
            EntityID grp = m_Scene.GroupEntities(m_Ctx.selectedEntities, "Group");
            if (grp != kInvalidEntity)
            {
                m_Ctx.SelectSingle(grp);
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Group Selected");
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !m_Ctx.selectedEntities.empty())
        {
            auto beforeEnts = m_Scene.GetEntities();
            auto beforeSel = m_Ctx.selectedEntities;
            EntityID beforePrimary = m_Ctx.selectedEntity;
            for (EntityID id : m_Ctx.selectedEntities)
                if (m_Scene.GetEntity(id)) m_Scene.DestroySubtree(id);
            m_Ctx.ClearSelection();
            m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                "Delete Selected");
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && m_Ctx.selectedEntity != kInvalidEntity)
        {
            m_Ctx.renameRequestId = m_Ctx.selectedEntity;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !m_Ctx.selectedEntities.empty())
        {
            FrameSelectedEntity();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_End, false) && !m_Ctx.selectedEntities.empty())
        {
            auto beforeEnts = m_Scene.GetEntities();
            auto beforeSel = m_Ctx.selectedEntities;
            EntityID beforePrimary = m_Ctx.selectedEntity;
            auto lookup = m_Ctx.MakeBoundsLookup();
            bool moved = false;
            for (EntityID id : m_Ctx.selectedEntities)
                moved |= m_Scene.DropEntityToSurface(id, lookup);
            if (moved)
            {
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Drop to Surface");
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_Ctx.viewportTools.gizmoMode = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_Ctx.viewportTools.gizmoMode = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_Ctx.viewportTools.gizmoMode = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_X, false)) m_Ctx.viewportTools.localSpace = !m_Ctx.viewportTools.localSpace;
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) m_Ctx.ClearSelection();
        }
    }

    void EditorApp::DrawMainMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open / Create Project...", "Ctrl+O"))
                m_Ctx.showProjectManagerModal = true;

            ImGui::Separator();

            if (ImGui::MenuItem("New Empty 3D Scene"))
                RequestGuardedAction({ PendingActionType::NewEmptyScene });

            if (ImGui::BeginMenu("Apply Template Scene"))
            {
                if (ImGui::MenuItem("Floating-Island 3D Platformer"))
                    RequestGuardedAction({ PendingActionType::ApplyTemplate, "", 1 });
                if (ImGui::MenuItem("Skyway 3D Obstacle Course"))
                    RequestGuardedAction({ PendingActionType::ApplyTemplate, "", 2 });
                if (ImGui::MenuItem("Top-Down Wave Defense Arena"))
                    RequestGuardedAction({ PendingActionType::ApplyTemplate, "", 3 });
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Open Scene (.hscene)..."))
            {
                std::string scenePath;
                if (ProjectManager::BrowseForSceneFile(scenePath))
                    RequestGuardedAction({ PendingActionType::LoadScene, scenePath });
            }

            if (ImGui::MenuItem("Reload Current Scene"))
                RequestGuardedAction({ PendingActionType::ReloadScene });

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                SaveCurrentScene();
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
                SaveCurrentSceneAs();

            ImGui::Separator();
            if (ImGui::MenuItem("Save Viewport / Editor Screenshot (BMP)"))
                m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), "editor_screenshot.bmp");
            if (ImGui::MenuItem("Editor Preferences..."))
                m_Ctx.showSettingsModal = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                m_Window.RequestClose();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            std::string undoLbl = m_Commands.HasUndo() ? ("Undo " + m_Commands.GetNextUndoLabel()) : "Undo";
            std::string redoLbl = m_Commands.HasRedo() ? ("Redo " + m_Commands.GetNextRedoLabel()) : "Redo";
            if (ImGui::MenuItem(undoLbl.c_str(), "Ctrl+Z", false, m_Commands.HasUndo()))
                m_Commands.Undo();
            if (ImGui::MenuItem(redoLbl.c_str(), "Ctrl+Y", false, m_Commands.HasRedo()))
                m_Commands.Redo();

            ImGui::Separator();
            bool hasSel = !m_Ctx.selectedEntities.empty();
            if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false, hasSel))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                std::vector<EntityID> newSel;
                for (EntityID id : m_Ctx.selectedEntities)
                {
                    EntityID dupId = m_Scene.DuplicateSubtree(id, glm::vec3(1.5f, 0.0f, 1.5f));
                    if (dupId != kInvalidEntity) newSel.push_back(dupId);
                }
                if (!newSel.empty())
                {
                    m_Ctx.selectedEntities = newSel;
                    m_Ctx.selectedEntity = newSel.back();
                    m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                        "Duplicate Selected");
                }
            }
            if (ImGui::MenuItem("Group Selected", "Ctrl+G", false, hasSel))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                EntityID grp = m_Scene.GroupEntities(m_Ctx.selectedEntities, "Group");
                if (grp != kInvalidEntity)
                {
                    m_Ctx.SelectSingle(grp);
                    m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                        "Group Selected");
                }
            }
            if (ImGui::MenuItem("Rename Selected", "F2", false, m_Ctx.selectedEntity != kInvalidEntity))
                m_Ctx.renameRequestId = m_Ctx.selectedEntity;
            if (ImGui::MenuItem("Drop to Surface", "End", false, hasSel))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                bool moved = false;
                auto lookup = m_Ctx.MakeBoundsLookup();
                for (EntityID id : m_Ctx.selectedEntities)
                    moved |= m_Scene.DropEntityToSurface(id, lookup);
                if (moved)
                {
                    m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                        "Drop to Surface");
                }
            }
            if (ImGui::MenuItem("Delete Selected", "Del", false, hasSel))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                for (EntityID id : m_Ctx.selectedEntities)
                    if (m_Scene.GetEntity(id)) m_Scene.DestroySubtree(id);
                m_Ctx.ClearSelection();
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Delete Selected");
            }
            if (ImGui::MenuItem("Frame Selected", "F", false, hasSel))
                FrameSelectedEntity();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            auto spawnPrim = [this](const char* name, MeshPrimitive prim, bool hasMesh = true) {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;

                Entity& e = m_Scene.CreateEntity(name, "Default");
                e.hasMesh = hasMesh;
                e.mesh.enabled = hasMesh;
                e.mesh.primitive = prim;
                e.hasMaterial = hasMesh;
                e.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 8.0f;
                e.transform.position.y = std::max(0.6f, e.transform.position.y);
                m_Ctx.SelectSingle(e.id);

                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    std::string("Create ") + name);
            };
            if (ImGui::MenuItem("Empty Node3D")) spawnPrim("Node3D", MeshPrimitive::Cube, false);
            if (ImGui::MenuItem("Cube"))         spawnPrim("Cube", MeshPrimitive::Cube);
            if (ImGui::MenuItem("Sphere"))       spawnPrim("Sphere", MeshPrimitive::Sphere);
            if (ImGui::MenuItem("Cylinder"))     spawnPrim("Cylinder", MeshPrimitive::Cylinder);
            if (ImGui::MenuItem("Cone"))         spawnPrim("Cone", MeshPrimitive::Cone);
            if (ImGui::MenuItem("Torus"))        spawnPrim("Torus", MeshPrimitive::Torus);
            ImGui::Separator();
            if (ImGui::MenuItem("Point Light"))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                Entity& l = m_Scene.CreateEntity("PointLight", "Light");
                l.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 6.0f;
                l.hasMesh = true; l.mesh.enabled = true; l.mesh.primitive = MeshPrimitive::Sphere;
                l.transform.scale = glm::vec3(0.35f);
                l.hasLight = true; l.light.enabled = true;
                l.light.color = glm::vec3(0.35f, 0.85f, 1.0f);
                l.light.intensity = 3.5f; l.light.range = 14.0f;
                m_Ctx.SelectSingle(l.id);
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Create Point Light");
            }
            if (ImGui::MenuItem("Camera3D"))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                Entity& c = m_Scene.CreateEntity("OrbitCamera3D", "Camera");
                c.transform.position = m_EditorCamera.position;
                c.hasMesh = false; c.mesh.enabled = false;
                c.hasCamera = true; c.camera.enabled = true;
                c.camera.mode = CameraMode::ThirdPersonOrbit;
                m_Ctx.SelectSingle(c.id);
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Create Camera3D");
            }
            if (ImGui::MenuItem("CPUParticles3D Fountain"))
            {
                auto beforeEnts = m_Scene.GetEntities();
                auto beforeSel = m_Ctx.selectedEntities;
                EntityID beforePrimary = m_Ctx.selectedEntity;
                Entity& p = m_Scene.CreateEntity("ParticleFountain", "Default");
                p.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 7.0f;
                p.hasMesh = true; p.mesh.enabled = true; p.mesh.primitive = MeshPrimitive::Sphere;
                p.transform.scale = glm::vec3(0.45f);
                p.hasParticles = true; p.particleEmitter.enabled = true; p.particleEmitter.emitting = true;
                p.particleEmitter.emissionShape = 3; p.particleEmitter.emissionRate = 25.0f;
                m_Ctx.SelectSingle(p.id);
                m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                    "Create Particle Fountain");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project"))
        {
            if (ImGui::MenuItem("Project Browser / Create...", "Ctrl+O"))
                m_Ctx.showProjectManagerModal = true;
            if (ImGui::MenuItem("Export Standalone Game...", "Ctrl+E"))
                m_Ctx.showExportModal = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Reload All Lua Scripts"))
            {
                m_Scripts.ReloadAllScripts(m_Scene);
                m_Ctx.SetToast("Reloaded Lua scripts");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem(kWinHierarchy,    nullptr, &m_Ctx.showHierarchy);
            ImGui::MenuItem(kWinInspector,    nullptr, &m_Ctx.showInspector);
            ImGui::MenuItem(kWinViewport,     nullptr, &m_Ctx.showViewport);
            ImGui::MenuItem(kWinAssetBrowser, nullptr, &m_Ctx.showAssetBrowser);
            ImGui::MenuItem(kWinScriptEditor, nullptr, &m_Ctx.showScriptEditor);
            ImGui::MenuItem(kWinConsole,      nullptr, &m_Ctx.showConsole);
            ImGui::MenuItem(kWinWorldEnv,     nullptr, &m_Ctx.showWorldEnv);
            ImGui::MenuItem(kWinSystems,      nullptr, &m_Ctx.showSystems);

            ImGui::Separator();
            if (ImGui::BeginMenu("Workspace Presets"))
            {
                if (ImGui::MenuItem("Scene Editing", nullptr, m_Workspace.GetActivePreset() == WorkspacePreset::SceneEditing))
                    m_Ctx.applyWorkspacePreset(WorkspacePreset::SceneEditing);
                if (ImGui::MenuItem("Scripting", nullptr, m_Workspace.GetActivePreset() == WorkspacePreset::Scripting))
                    m_Ctx.applyWorkspacePreset(WorkspacePreset::Scripting);
                if (ImGui::MenuItem("Materials & Lighting", nullptr, m_Workspace.GetActivePreset() == WorkspacePreset::MaterialsLighting))
                    m_Ctx.applyWorkspacePreset(WorkspacePreset::MaterialsLighting);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Default Layout"))
                    m_Ctx.resetDefaultLayout();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Saved Layouts"))
            {
                if (ImGui::MenuItem("Save Current Layout As..."))
                    m_Workspace.RequestSaveLayoutPopup();
                ImGui::Separator();
                auto layouts = m_Workspace.ListSavedLayouts();
                if (layouts.empty())
                {
                    ImGui::TextDisabled("No custom layouts saved yet");
                }
                else
                {
                    for (const auto& name : layouts)
                    {
                        if (ImGui::MenuItem(("Load: " + name).c_str()))
                        {
                            m_Workspace.LoadNamedLayout(name);
                            m_Ctx.SetToast("Loaded layout: " + name);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        // Optional project-specific module menu
        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            CameraView camBefore = m_EditorCamera;
            m_GameModule->DrawEditorMenu(ctx, m_EditorCamera, m_EditorCamYaw, m_EditorCamPitch);
            if (camBefore.position != m_EditorCamera.position || camBefore.forward != m_EditorCamera.forward)
                StopPlayMode();
        }

        ImGui::EndMainMenuBar();
    }

    void EditorApp::DrawMainToolbar(float toolbarHeight)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, toolbarHeight));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.092f, 0.108f, 0.140f, 1.0f));
        ImGui::Begin("##HargonMainToolbar", nullptr, flags);

        auto toolToggle = [](const char* label, bool active) -> bool {
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.80f, 1.0f));
            bool pressed = ImGui::Button(label);
            if (active)
                ImGui::PopStyleColor();
            return pressed;
        };

        if (toolToggle("Move [W]", m_Ctx.viewportTools.gizmoMode == GizmoMode::Translate))
            m_Ctx.viewportTools.gizmoMode = GizmoMode::Translate;
        ImGui::SameLine();
        if (toolToggle("Rotate [E]", m_Ctx.viewportTools.gizmoMode == GizmoMode::Rotate))
            m_Ctx.viewportTools.gizmoMode = GizmoMode::Rotate;
        ImGui::SameLine();
        if (toolToggle("Scale [R]", m_Ctx.viewportTools.gizmoMode == GizmoMode::Scale))
            m_Ctx.viewportTools.gizmoMode = GizmoMode::Scale;

        ImGui::SameLine(0.0f, 14.0f);
        if (toolToggle(m_Ctx.viewportTools.localSpace ? "Local [X]" : "World [X]", m_Ctx.viewportTools.localSpace))
            m_Ctx.viewportTools.localSpace = !m_Ctx.viewportTools.localSpace;

        ImGui::SameLine();
        if (toolToggle("Snap", m_Ctx.viewportTools.snapEnabled))
            m_Ctx.viewportTools.snapEnabled = !m_Ctx.viewportTools.snapEnabled;
        if (m_Ctx.viewportTools.snapEnabled)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(62.0f);
            ImGui::DragFloat("##MoveSnap", &m_Ctx.viewportTools.moveSnap, 0.05f, 0.05f, 10.0f, "%.2fm");
        }

        ImGui::SameLine();
        ImGui::Checkbox("Grid", &m_Ctx.viewportTools.drawGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Colliders", &m_Ctx.viewportTools.drawColliders);
        ImGui::SameLine();
        ImGui::Checkbox("Overlay", &m_Ctx.viewportTools.showOverlay);

        // Centered Play / Pause / Stop / Step controls
        float centerX = std::max(ImGui::GetCursorPosX() + 14.0f, ImGui::GetWindowWidth() * 0.45f);
        ImGui::SameLine(centerX);

        if (m_PlayState == PlayState::Edit)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.56f, 0.30f, 1.0f));
            if (ImGui::Button("> Play [F5]", ImVec2(98.0f, 0.0f)))
                StartPlayMode();
            ImGui::PopStyleColor();
        }
        else if (m_PlayState == PlayState::Playing)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.52f, 0.14f, 1.0f));
            if (ImGui::Button("|| Pause", ImVec2(82.0f, 0.0f)))
                PausePlayMode();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.20f, 0.20f, 1.0f));
            if (ImGui::Button("[] Stop", ImVec2(78.0f, 0.0f)))
                StopPlayMode();
            ImGui::PopStyleColor();
        }
        else // Paused
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.56f, 0.30f, 1.0f));
            if (ImGui::Button("> Resume", ImVec2(86.0f, 0.0f)))
                PausePlayMode();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button(">| Step", ImVec2(68.0f, 0.0f)))
                StepPlayMode();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.20f, 0.20f, 1.0f));
            if (ImGui::Button("[] Stop", ImVec2(74.0f, 0.0f)))
                StopPlayMode();
            ImGui::PopStyleColor();
        }

        // Right-aligned Workspace Presets & Quick Actions
        float rightGroupW = 365.0f;
        float rightStart = std::max(ImGui::GetCursorPosX() + 14.0f, ImGui::GetWindowWidth() - rightGroupW);
        ImGui::SameLine(rightStart);

        if (toolToggle("Scene", m_Workspace.GetActivePreset() == WorkspacePreset::SceneEditing))
            m_Ctx.applyWorkspacePreset(WorkspacePreset::SceneEditing);
        ImGui::SameLine();
        if (toolToggle("Script", m_Workspace.GetActivePreset() == WorkspacePreset::Scripting))
            m_Ctx.applyWorkspacePreset(WorkspacePreset::Scripting);
        ImGui::SameLine();
        if (toolToggle("Lighting", m_Workspace.GetActivePreset() == WorkspacePreset::MaterialsLighting))
            m_Ctx.applyWorkspacePreset(WorkspacePreset::MaterialsLighting);

        ImGui::SameLine(0.0f, 12.0f);
        if (ImGui::Button("Projects"))
            m_Ctx.showProjectManagerModal = true;
        ImGui::SameLine();
        if (ImGui::Button("Export"))
            m_Ctx.showExportModal = true;

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void EditorApp::Run()
    {
        if (m_AutoTestMode)
        {
            RunAutoTests();
            return;
        }

        auto prevTime = std::chrono::steady_clock::now();

        while (true)
        {
            m_Window.PollEvents();

            // Intercept close request when unsaved changes exist
            if (m_Window.ShouldClose())
            {
                if (!m_ExitConfirmed && HasAnyUnsavedWork())
                {
                    m_Window.ClearCloseRequest();
                    RequestGuardedAction({ PendingActionType::ExitApp });
                }
                else
                {
                    break;
                }
            }

            if (m_Window.WasResized())
            {
                m_RHI.Resize(m_Window.GetWidth(), m_Window.GetHeight());
                m_Window.ClearResizedFlag();
            }

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - prevTime).count();
            prevTime = now;
            dt = std::clamp(dt, 0.0001f, 0.05f);
            m_TotalElapsedTime += dt;
            m_SmoothedFPS = m_SmoothedFPS * 0.92f + (1.0f / dt) * 0.08f;

            // Toast countdown
            if (m_Ctx.statusToastTimer > 0.0f)
            {
                m_Ctx.statusToastTimer -= dt;
                if (m_Ctx.statusToastTimer <= 0.0f)
                    m_Ctx.statusToast = "Ready";
            }

            // Step gameplay or editor particles/tweens
            if (m_PlayState == PlayState::Playing)
            {
                if (m_GameModule)
                {
                    m_GameModule->OnStepPlay(BuildModuleContext(), dt, m_EditorCamera);
                }
                else
                {
                    GameSystem::StepGameplay(m_Scene, m_Physics, m_Scripts, m_Audio, m_Renderer3D, &m_Window, dt);
                    GameSystem::UpdateGameCamera(m_Scene, m_EditorCamera, dt);
                }
                if (m_Scene.restartRequested)
                {
                    m_Scene.restartRequested = false;
                    StopPlayMode();
                    StartPlayMode();
                }
            }
            else if (m_PlayState == PlayState::Edit)
            {
                if (m_GameModule)
                    m_GameModule->OnStepEditorPreview(BuildModuleContext(), dt);
                m_Renderer3D.UpdateParticles(dt);
                m_Workspace.TickAutosave(m_Ctx, dt);
            }

            // Ensure Viewport offscreen RT matches desired size BEFORE command list begins
            m_ViewportPanel.SyncRenderTargetIfNeeded(m_Ctx);

            if (!m_RHI.BeginFrame())
                continue;

            nvrhi::CommandListHandle cmd = m_RHI.GetDevice()->createCommandList();
            cmd->open();

            // 1. Render 3D scene offscreen into Viewport texture
            m_ViewportPanel.RenderSceneToTarget(m_Ctx, cmd, m_TotalElapsedTime);

            // 2. Build Dockable Editor UI
            m_ImGui.BeginFrame();
            HandleGlobalShortcuts();

            const float toolbarH = 40.0f * m_Ctx.settings.uiScale;
            const float statusH  = 28.0f * m_Ctx.settings.uiScale;
            DrawMainMenuBar();
            DrawMainToolbar(toolbarH);
            m_Workspace.BeginMainDockspace(m_Ctx, toolbarH, statusH);

            m_HierarchyPanel.Draw(m_Ctx);
            m_InspectorPanel.Draw(m_Ctx);
            m_InspectorPanel.DrawWorldEnvironment(m_Ctx);
            m_ViewportPanel.Draw(m_Ctx, dt);
            m_AssetBrowserPanel.Draw(m_Ctx);
            m_ScriptEditorPanel.Draw(m_Ctx);
            m_ConsolePanel.Draw(m_Ctx);
            m_ConsolePanel.DrawEngineSystems(m_Ctx);

            {
                std::string focusWin = m_Workspace.ConsumePendingFocusWindow();
                if (!focusWin.empty())
                    ImGui::SetWindowFocus(focusWin.c_str());
            }

            m_Workspace.DrawStatusBar(m_Ctx, statusH);
            m_Workspace.DrawSettingsModal(m_Ctx);
            m_Workspace.DrawSaveLayoutModal();
            m_ProjectManagerWindow.DrawModals(m_Ctx, [this](const PendingUnsavedAction& act) {
                ExecutePendingAction(act);
            });

            UpdateCursorOwnership();
            UpdateWindowTitle();

            // 3. Render ImGui onto swapchain backbuffer
            m_ImGui.Render(cmd, m_RHI.GetCurrentFramebuffer());

            cmd->close();
            m_RHI.FinishFrameAndPresent(cmd);
        }
    }

    void EditorApp::RenderAndCaptureEditorScreenshot(const std::string& outPath, float simTime)
    {
        for (int f = 0; f < 3; ++f)
        {
            m_Window.PollEvents();
            m_ViewportPanel.SyncRenderTargetIfNeeded(m_Ctx);
            if (!m_RHI.BeginFrame()) continue;

            nvrhi::CommandListHandle cmd = m_RHI.GetDevice()->createCommandList();
            cmd->open();

            m_ViewportPanel.RenderSceneToTarget(m_Ctx, cmd, simTime + f * 0.016f);

            m_ImGui.BeginFrame();
            const float toolbarH = 40.0f * m_Ctx.settings.uiScale;
            const float statusH  = 28.0f * m_Ctx.settings.uiScale;
            DrawMainMenuBar();
            DrawMainToolbar(toolbarH);
            m_Workspace.BeginMainDockspace(m_Ctx, toolbarH, statusH);
            m_HierarchyPanel.Draw(m_Ctx);
            m_InspectorPanel.Draw(m_Ctx);
            m_InspectorPanel.DrawWorldEnvironment(m_Ctx);
            m_ViewportPanel.Draw(m_Ctx, 0.016f);
            m_AssetBrowserPanel.Draw(m_Ctx);
            m_ScriptEditorPanel.Draw(m_Ctx);
            m_ConsolePanel.Draw(m_Ctx);
            m_ConsolePanel.DrawEngineSystems(m_Ctx);
            {
                std::string focusWin = m_Workspace.ConsumePendingFocusWindow();
                if (!focusWin.empty())
                    ImGui::SetWindowFocus(focusWin.c_str());
            }
            m_Workspace.DrawStatusBar(m_Ctx, statusH);
            m_ImGui.Render(cmd, m_RHI.GetCurrentFramebuffer());

            cmd->close();
            m_RHI.FinishFrameAndPresent(cmd);
        }
        m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), outPath);
    }

    void EditorApp::RunAutoTests()
    {
        Log::Info("=== Hargon Editor Usability & Functional AutoTest Starting ===");

        // 1. Overwrite refusal test on ProjectManager::CreateNewProject
        {
            GameProjectConfig dummyCfg;
            std::string errStatus;
            bool shouldFail = ProjectManager::CreateNewProject("ElderSlipgate", "Overwrite Test", "FPS", 3, false, dummyCfg, &errStatus);
            if (shouldFail)
                Log::Error("AutoTest FAILED: CreateNewProject overwrote existing ElderSlipgate project!");
            else
                Log::Info("AutoTest PASS: CreateNewProject refused overwrite (" + errStatus + ")");
        }

        // 2. OBB ray-picking + non-16:9 aspect ratio math test
        {
            Scene testPickScene;
            Entity& cube = testPickScene.CreateEntity("RotatedCube", "Default");
            cube.hasMesh = true;
            cube.mesh.enabled = true;
            cube.mesh.primitive = MeshPrimitive::Cube;
            cube.transform.position = glm::vec3(2.0f, 1.0f, 0.0f);
            cube.transform.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
            cube.transform.scale = glm::vec3(2.0f, 2.0f, 2.0f);

            CameraView testCam;
            testCam.position = glm::vec3(2.0f, 1.0f, 8.0f);
            testCam.forward = glm::vec3(0.0f, 0.0f, -1.0f);
            testCam.aspectRatio = 4.0f / 3.0f;

            glm::vec3 ro, rd;
            ViewportPanel::ComputeViewportRay(testCam, ImVec2(400.0f, 300.0f), ImVec2(0.0f, 0.0f), ImVec2(800.0f, 600.0f), ro, rd);
            EntityID picked = testPickScene.Raycast(ro, rd);
            if (picked != cube.id)
                Log::Error("AutoTest FAILED: OBB picking did not hit RotatedCube at 4:3 aspect!");
            else
                Log::Info("AutoTest PASS: OBB picking accurately hit RotatedCube at non-16:9 aspect.");
        }

        // 3. Hierarchy reparent keeping world transform + Undo/Redo dirty tracking
        {
            size_t beforeCount = m_Scene.GetEntities().size();
            auto beforeEnts = m_Scene.GetEntities();
            auto beforeSel = m_Ctx.selectedEntities;
            EntityID beforePrimary = m_Ctx.selectedEntity;

            Entity& child = m_Scene.CreateEntity("AutoTestBox", "Default");
            child.transform.position = glm::vec3(3.0f, 4.0f, 5.0f);
            EntityID childId = child.id;
            m_Ctx.SelectSingle(childId);
            m_Commands.RecordSceneSnapshot(m_Scene, m_Ctx,
                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                m_Scene.GetEntities(), m_Ctx.selectedEntities, m_Ctx.selectedEntity,
                "AutoTest Create");

            if (!m_Commands.IsDirty())
                Log::Error("AutoTest FAILED: Scene should be marked dirty after Create!");

            m_Commands.Undo();
            if (m_Scene.GetEntities().size() != beforeCount || m_Commands.IsDirty())
                Log::Error("AutoTest FAILED: Undo back to save point did not restore entity count & clean dirty state!");
            else
                Log::Info("AutoTest PASS: Undo/Redo & exact save-point dirty state verified.");
        }

        // 4. Cursor ownership verification when opening ElderSlipgate FPS project
        if (OpenProjectByInput("ElderSlipgate"))
        {
            if (m_Window.IsCursorCaptured())
                Log::Error("AutoTest FAILED: Cursor captured inside Editor upon opening ElderSlipgate!");
            else
                Log::Info("AutoTest PASS: Cursor remained visible in Editor with ElderSlipgate open.");

            StartPlayMode();
            if (m_GameModule)
                m_GameModule->OnStepPlay(BuildModuleContext(), 0.016f, m_EditorCamera);
            StopPlayMode();
            if (m_Window.IsCursorCaptured())
                Log::Error("AutoTest FAILED: Cursor still captured after StopPlayMode on ElderSlipgate!");
            else
                Log::Info("AutoTest PASS: Cursor cleanly released after StopPlayMode on ElderSlipgate.");
        }

        // 5. Switch to AetherRealms3D & capture distinct Workspace Presets screenshots + Play Mode
        OpenProjectByInput("AetherRealms3D");
        if (!m_Scene.GetEntities().empty())
            m_Ctx.SelectSingle(m_Scene.GetEntities()[0].id);

        m_Workspace.RequestPreset(WorkspacePreset::SceneEditing);
        RenderAndCaptureEditorScreenshot("autotest_editor_scene_editing.bmp", 0.5f);

        m_Workspace.RequestPreset(WorkspacePreset::MaterialsLighting);
        RenderAndCaptureEditorScreenshot("autotest_editor_lighting.bmp", 0.7f);

        // Open a project Lua script if present before switching to Scripting preset screenshot
        for (const auto& e : m_Scene.GetEntities())
        {
            if (e.HasLuaScript() && !e.luaScript.scriptPath.empty())
            {
                m_ScriptEditorPanel.OpenScript(m_Ctx, e.luaScript.scriptPath, 1);
                break;
            }
        }
        m_Workspace.RequestPreset(WorkspacePreset::Scripting);
        RenderAndCaptureEditorScreenshot("autotest_editor_scripting.bmp", 0.9f);

        // Return to Scene Editing & capture Play Mode
        m_Workspace.RequestPreset(WorkspacePreset::SceneEditing);
        StartPlayMode();
        for (int s = 0; s < 5; ++s)
        {
            GameSystem::StepGameplay(m_Scene, m_Physics, m_Scripts, m_Audio, m_Renderer3D, &m_Window, 0.016f);
            GameSystem::UpdateGameCamera(m_Scene, m_EditorCamera, 0.016f);
        }
        RenderAndCaptureEditorScreenshot("autotest_editor_play.bmp", 1.2f);
        StopPlayMode();

        Log::Info("=== Hargon Editor AutoTest Complete ===");
        m_ExitConfirmed = true;
    }

    void EditorApp::Shutdown()
    {
        StopPlayMode();
        m_GameModule.reset();
        m_Workspace.Shutdown(m_Ctx);
        m_ViewportPanel.Shutdown(m_Ctx);
        m_Scripts.Shutdown();
        m_ImGui.Shutdown();
        m_Renderer3D.Shutdown();
        m_Audio.Shutdown();
        m_RHI.Shutdown();
        m_Window.Shutdown();
    }
}

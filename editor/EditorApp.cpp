#include "EditorApp.h"
#include "../engine/runtime/GameSystem.h"
#include "../engine/project/SceneTemplates.h"
#include "../engine/core/Log.h"
#include <nvrhi/utils.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Hargon
{
    EditorApp::~EditorApp()
    {
        Shutdown();
    }

    bool EditorApp::Init(
        GraphicsBackend backend,
        bool enableValidation,
        bool autoTestMode,
        const std::string& initialProjectSlug,
        int autoCreateTemplate)
    {
        m_AutoTestMode = autoTestMode;
        Log::Init();
        Log::Editor("Starting Hargon 3D Game Engine & General-Purpose Visual Editor...");

        WindowConfig winCfg;
        winCfg.title = "Hargon 3D Game Engine - General-Purpose Visual Editor & Project Studio";
        winCfg.width = 1600;
        winCfg.height = 920;
        winCfg.resizable = true;

        if (!m_Window.Init(winCfg))
            return false;

        if (!m_RHI.Init(&m_Window, backend, enableValidation))
            return false;

        if (!m_Renderer3D.Init(&m_RHI))
            return false;

        if (!m_ImGui.Init(&m_RHI, &m_Window))
            return false;

        m_Audio.Init("assets/audio");
        m_Scripts.Init(&m_Scene, &m_Window, &m_Audio);
        m_Scripts.SetParticleCallback([this](const glm::vec3& pos, const glm::vec3& col, int count, float speed) {
            m_Renderer3D.SpawnParticleBurst(pos, col, count, speed);
        });

        m_CommandList = m_RHI.GetDevice()->createCommandList();
        m_ViewportTarget = m_RHI.CreateOffscreenTarget(1024, 640);

        // Optional CLI project creation for automated workflows (--create-project <slug> <template>)
        if (!initialProjectSlug.empty() && autoCreateTemplate >= 0)
        {
            GameProjectConfig createdCfg;
            std::string msg;
            std::string prettyTitle = initialProjectSlug;
            std::string prettyGenre = (autoCreateTemplate == 4)
                ? "3D Gravity Platformer & Time-Trial"
                : "Independent Hargon Game Project";
            if (autoCreateTemplate == 4 && initialProjectSlug == "SkywayTrial")
                prettyTitle = "Skyway Trial: Horizon Leap";
            ProjectManager::CreateNewProject(
                initialProjectSlug, prettyTitle, prettyGenre, autoCreateTemplate, (autoCreateTemplate == 3), createdCfg, &msg);
        }

        // Discover all game projects in projects/
        m_DiscoveredProjects = ProjectManager::DiscoverProjects("projects");

        // Open requested CLI project if provided, or first general-purpose SceneAndLua project by default
        GameProjectConfig initialProj;
        bool foundInitial = false;
        if (!initialProjectSlug.empty())
        {
            for (const auto& p : m_DiscoveredProjects)
            {
                if (p.projectName == initialProjectSlug)
                {
                    initialProj = p;
                    foundInitial = true;
                    break;
                }
            }
        }
        if (!foundInitial)
        {
            for (const auto& p : m_DiscoveredProjects)
            {
                if (p.gameplayModule == "SceneAndLua")
                {
                    initialProj = p;
                    foundInitial = true;
                    break;
                }
            }
        }
        if (!foundInitial && !m_DiscoveredProjects.empty())
            initialProj = m_DiscoveredProjects.front();

        OpenProject(initialProj);
        OpenLuaScriptInEditor(m_OpenScriptPath);

        Log::Editor("Hargon Visual Editor ready. Use the Project Manager or [Project] menu to create, open, switch, or configure projects.");
        return true;
    }

    GameModuleContext EditorApp::BuildModuleContext()
    {
        return GameModuleContext{ &m_Scene, &m_Physics, &m_Scripts, &m_Audio, &m_Renderer3D, &m_Window, &m_ProjectConfig };
    }

    bool EditorApp::OpenProject(const GameProjectConfig& config)
    {
        StopPlayMode();
        m_ProjectConfig = config;
        m_CurrentScenePath = config.startupScene;
        m_ExportDirectory = "Exports/" + (config.projectName.empty() ? std::string("GameProject") : config.projectName);
        m_ExportStatusMessage.clear();
        m_UndoRedo.Clear();

        m_GameModule = GameModuleRegistry::Get().CreateModule(config.gameplayModule);
        GameModuleContext ctx = BuildModuleContext();

        if (m_GameModule)
        {
            m_GameModule->OnProjectOpen(ctx, false, m_EditorCamera);
            m_DrawGrid = m_GameModule->ShouldDrawEditorGrid();

            float prefYaw = 0.0f, prefPitch = 20.0f;
            if (m_GameModule->HasPreferredEditorCameraPose(prefYaw, prefPitch))
            {
                m_EditorCamYaw = prefYaw;
                m_EditorCamPitch = prefPitch;
            }
            else
            {
                m_EditorCamYaw = 0.0f;
                m_EditorCamPitch = 20.0f;
            }
            UpdateEditorCamera(0.0f);
            m_SelectedEntity = m_GameModule->PickDefaultSelectedEntity(ctx, m_EditorCamera);
        }
        else
        {
            SceneTemplates::BuildEmptyTemplateScene(m_Scene, config.gameTitle);
            m_DrawGrid = true;
            m_SelectedEntity = kInvalidEntity;
        }

        if (Entity* sel = m_Scene.GetEntity(m_SelectedEntity))
        {
            EntityID eid = sel->id;
            glm::vec3 origPos = sel->transform.position;
            m_UndoRedo.CreateAction("Open Project '" + config.projectName + "' & Select " + sel->tag.name);
            m_UndoRedo.AddDoMethod([this, eid, origPos]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.position = origPos; });
            m_UndoRedo.AddUndoMethod([this, eid, origPos]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.position = origPos; });
            m_UndoRedo.CommitAction(false);
        }

        Log::Editor("Opened project '" + config.gameTitle + "' [" + config.genre + "] (Module: " + config.gameplayModule + ")");
        return true;
    }

    void EditorApp::Shutdown()
    {
        m_RHI.WaitForIdle();
        m_ViewportTarget = {};
        m_CommandList = nullptr;
        m_GameModule.reset();
        m_Scripts.Shutdown();
        m_Audio.Shutdown();
        m_ImGui.Shutdown();
        m_Renderer3D.Shutdown();
        m_RHI.Shutdown();
        m_Window.Shutdown();
    }

    void EditorApp::OpenLuaScriptInEditor(const std::string& path)
    {
        m_OpenScriptPath = path;
        std::ifstream in(path);
        if (in)
        {
            std::ostringstream ss;
            ss << in.rdbuf();
            m_ScriptEditorBuffer = ss.str();
        }
        else
        {
            m_ScriptEditorBuffer = "-- Script file: " + path + "\nfunction on_init(id)\nend\n\nfunction on_update(id, dt)\nend\n";
        }
    }

    void EditorApp::SaveAndReloadCurrentLuaScript()
    {
        std::ofstream out(m_OpenScriptPath, std::ios::out | std::ios::trunc);
        if (out)
        {
            out << m_ScriptEditorBuffer;
            out.close();
            m_Scripts.ReloadAllScripts(m_Scene);
            Log::Editor("Saved and hot-reloaded Lua script: " + m_OpenScriptPath);
        }
    }

    void EditorApp::StartPlayMode()
    {
        if (m_State == EditorState::Play)
            return;

        bool fromEdit = (m_State == EditorState::Edit);
        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            m_GameModule->OnEnterPlayMode(ctx, fromEdit, m_PlayCamera);
        }
        if (fromEdit)
            m_Audio.PlaySound2D("wave_start", 0.85f, 1.0f);

        m_State = EditorState::Play;
        Log::Editor("Entered Play Mode for project: " + m_ProjectConfig.gameTitle);
    }

    void EditorApp::StopPlayMode()
    {
        if (m_State == EditorState::Edit)
            return;

        m_State = EditorState::Edit;
        m_Window.ClearSimulatedKeys();
        m_Window.SetCursorCaptured(false);
        if (m_GameModule)
        {
            GameModuleContext ctx = BuildModuleContext();
            m_GameModule->OnStopPlayMode(ctx);
        }
        Log::Editor("Stopped Play Mode and restored Edit Scene state.");
    }

    void EditorApp::UpdateEditorCamera(float dt)
    {
        if (m_State != EditorState::Edit)
            return;

        if (m_ViewportHovered && m_Window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            glm::vec2 md = m_Window.GetMouseDelta();
            m_EditorCamYaw += md.x * 0.22f;
            m_EditorCamPitch = std::clamp(m_EditorCamPitch + md.y * 0.22f, -85.0f, 85.0f);
        }

        float yawRad = glm::radians(m_EditorCamYaw);
        float pitchRad = glm::radians(m_EditorCamPitch);

        glm::vec3 forward(
            std::sin(yawRad) * std::cos(pitchRad),
            -std::sin(pitchRad),
            std::cos(yawRad) * std::cos(pitchRad));
        m_EditorCamera.forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), m_EditorCamera.forward));
        glm::vec3 up = glm::normalize(glm::cross(m_EditorCamera.forward, right));

        if (m_ViewportHovered)
        {
            float scroll = m_Window.GetScrollDelta();
            if (std::abs(scroll) > 0.001f)
                m_EditorCamera.position += m_EditorCamera.forward * (scroll * 2.2f);
        }

        if (m_ViewportHovered && m_Window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) && dt > 0.0f)
        {
            float spd = m_EditorCamSpeed * (m_Window.IsKeyDown(GLFW_KEY_LEFT_SHIFT) ? 2.2f : 1.0f);
            if (m_Window.IsKeyDown(GLFW_KEY_W)) m_EditorCamera.position += m_EditorCamera.forward * spd * dt;
            if (m_Window.IsKeyDown(GLFW_KEY_S)) m_EditorCamera.position -= m_EditorCamera.forward * spd * dt;
            if (m_Window.IsKeyDown(GLFW_KEY_D)) m_EditorCamera.position += right * spd * dt;
            if (m_Window.IsKeyDown(GLFW_KEY_A)) m_EditorCamera.position -= right * spd * dt;
            if (m_Window.IsKeyDown(GLFW_KEY_E)) m_EditorCamera.position += up * spd * dt;
            if (m_Window.IsKeyDown(GLFW_KEY_Q)) m_EditorCamera.position -= up * spd * dt;
        }

        // Hotkeys when not typing in a text widget
        if (!ImGui::GetIO().WantTextInput && !m_Window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            bool ctrlDown = m_Window.IsKeyDown(GLFW_KEY_LEFT_CONTROL) || m_Window.IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
            if (ctrlDown && m_Window.IsKeyPressed(GLFW_KEY_Z)) m_UndoRedo.Undo();
            if (ctrlDown && m_Window.IsKeyPressed(GLFW_KEY_Y)) m_UndoRedo.Redo();
            if (!ctrlDown && m_Window.IsKeyPressed(GLFW_KEY_W)) m_GizmoMode = GizmoMode::Translate;
            if (!ctrlDown && m_Window.IsKeyPressed(GLFW_KEY_E)) m_GizmoMode = GizmoMode::Rotate;
            if (!ctrlDown && m_Window.IsKeyPressed(GLFW_KEY_R)) m_GizmoMode = GizmoMode::Scale;
            if (m_Window.IsKeyPressed(GLFW_KEY_F))
            {
                if (Entity* sel = m_Scene.GetEntity(m_SelectedEntity))
                {
                    m_EditorCamera.position = sel->transform.position - m_EditorCamera.forward * 10.0f;
                }
            }
            if (m_Window.IsKeyPressed(GLFW_KEY_DELETE) && m_SelectedEntity != kInvalidEntity)
            {
                m_Scene.DestroyEntity(m_SelectedEntity);
                m_Scene.FlushDestroyedEntities();
                m_SelectedEntity = kInvalidEntity;
            }
        }
    }

    glm::vec3 EditorApp::ComputeViewportRay(const ImVec2& mousePos) const
    {
        float nx = ((mousePos.x - m_ViewportPos.x) / std::max(1.0f, m_ViewportSize.x)) * 2.0f - 1.0f;
        float ny = 1.0f - ((mousePos.y - m_ViewportPos.y) / std::max(1.0f, m_ViewportSize.y)) * 2.0f;

        glm::mat4 invVP = glm::inverse(m_EditorCamera.GetViewProjectionMatrix());
        glm::vec4 nearPt = invVP * glm::vec4(nx, ny, 0.0f, 1.0f);
        glm::vec4 farPt  = invVP * glm::vec4(nx, ny, 1.0f, 1.0f);
        nearPt /= nearPt.w;
        farPt  /= farPt.w;

        return glm::normalize(glm::vec3(farPt - nearPt));
    }

    void EditorApp::HandleViewportGizmoAndPicking()
    {
        if (m_State != EditorState::Edit || !m_ViewportHovered)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                m_DraggedGizmoAxis = -1;
            return;
        }

        ImVec2 mousePos = ImGui::GetMousePos();
        glm::vec3 rayDir = ComputeViewportRay(mousePos);
        Entity* sel = m_Scene.GetEntity(m_SelectedEntity);

        if (m_DraggedGizmoAxis >= 0 && sel && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            glm::vec2 delta(mousePos.x - m_DragStartMouse.x, mousePos.y - m_DragStartMouse.y);
            float amount = (delta.x - delta.y) * 0.035f;

            if (m_GizmoMode == GizmoMode::Translate)
            {
                glm::vec3 oldPos = m_DragStartVec;
                glm::vec3 pos = m_DragStartVec;
                pos[m_DraggedGizmoAxis] += amount;
                EntityID eid = sel->id;
                m_UndoRedo.CreateAction("Gizmo Translate " + sel->tag.name, UndoRedo::MergeMode::MergeEnds);
                m_UndoRedo.AddDoMethod([this, eid, pos]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.position = pos; });
                m_UndoRedo.AddUndoMethod([this, eid, oldPos]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.position = oldPos; });
                m_UndoRedo.CommitAction(true);
            }
            else if (m_GizmoMode == GizmoMode::Rotate)
            {
                glm::vec3 oldRot = m_DragStartVec;
                glm::vec3 rot = m_DragStartVec;
                rot[m_DraggedGizmoAxis] += amount * 18.0f;
                EntityID eid = sel->id;
                m_UndoRedo.CreateAction("Gizmo Rotate " + sel->tag.name, UndoRedo::MergeMode::MergeEnds);
                m_UndoRedo.AddDoMethod([this, eid, rot]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.rotation = rot; });
                m_UndoRedo.AddUndoMethod([this, eid, oldRot]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.rotation = oldRot; });
                m_UndoRedo.CommitAction(true);
            }
            else if (m_GizmoMode == GizmoMode::Scale)
            {
                glm::vec3 oldScl = m_DragStartVec;
                glm::vec3 scl = m_DragStartVec;
                scl[m_DraggedGizmoAxis] = std::max(0.1f, scl[m_DraggedGizmoAxis] + amount * 0.5f);
                EntityID eid = sel->id;
                m_UndoRedo.CreateAction("Gizmo Scale " + sel->tag.name, UndoRedo::MergeMode::MergeEnds);
                m_UndoRedo.AddDoMethod([this, eid, scl]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.scale = scl; });
                m_UndoRedo.AddUndoMethod([this, eid, oldScl]() { if (Entity* ent = m_Scene.GetEntity(eid)) ent->transform.scale = oldScl; });
                m_UndoRedo.CommitAction(true);
            }
            return;
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_DraggedGizmoAxis = -1;
        }

        m_HoveredGizmoAxis = -1;
        if (sel)
        {
            m_HoveredGizmoAxis = m_Renderer3D.RaycastGizmo(
                sel->transform.position,
                m_EditorCamera,
                m_EditorCamera.position,
                rayDir);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (sel && m_HoveredGizmoAxis >= 0)
            {
                m_DraggedGizmoAxis = m_HoveredGizmoAxis;
                m_DragStartMouse = glm::vec2(mousePos.x, mousePos.y);
                if (m_GizmoMode == GizmoMode::Translate) m_DragStartVec = sel->transform.position;
                else if (m_GizmoMode == GizmoMode::Rotate) m_DragStartVec = sel->transform.rotation;
                else m_DragStartVec = sel->transform.scale;
            }
            else
            {
                EntityID picked = m_Scene.Raycast(m_EditorCamera.position, rayDir);
                if (picked != kInvalidEntity)
                    m_SelectedEntity = picked;
            }
        }
    }

    void EditorApp::Run()
    {
        auto lastTime = std::chrono::steady_clock::now();

        while (!m_Window.ShouldClose())
        {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;
            dt = std::clamp(dt, 0.001f, 0.05f);
            m_TotalTime += dt;
            m_SmoothedFPS = m_SmoothedFPS * 0.9f + (1.0f / dt) * 0.1f;

            m_Window.PollEvents();
            if (m_Window.WasResized())
            {
                m_RHI.Resize(m_Window.GetWidth(), m_Window.GetHeight());
                m_Window.ClearResizedFlag();
            }

            // Automated verification sequence when --autotest is active:
            // Verifies multi-project management cleanly without hardcoding project names:
            //   1) Active Project (Edit Mode + Standalone Export + Play Mode)
            //   2) Switch to another discovered project + Standalone Export
            if (m_AutoTestMode)
            {
                dt = 1.0f / 60.0f;
                ++m_FrameCounter;
                if (m_FrameCounter == 10)
                {
                    GameSystem::ExportStandaloneGame(m_Scene, m_ProjectConfig, "Exports/" + m_ProjectConfig.projectName, m_ExportStatusMessage);
                }
                else if (m_FrameCounter == 15)
                {
                    StartPlayMode();
                }
                else if (m_FrameCounter >= 16 && m_FrameCounter <= 52)
                {
                    m_Window.SetSimulatedKey(GLFW_KEY_W, true);
                    m_Window.SetSimulatedKey(GLFW_KEY_SPACE, (m_FrameCounter % 8) == 0);
                }
                else if (m_FrameCounter == 56)
                {
                    StopPlayMode();
                    std::string firstProj = m_ProjectConfig.projectName;
                    for (const auto& p : m_DiscoveredProjects)
                    {
                        if (p.projectName != firstProj)
                        {
                            OpenProject(p);
                            break;
                        }
                    }
                    GameSystem::ExportStandaloneGame(m_Scene, m_ProjectConfig, "Exports/" + m_ProjectConfig.projectName, m_ExportStatusMessage);
                }
                else if (m_FrameCounter >= 68)
                {
                    m_Window.RequestClose();
                }
            }

            if (m_State == EditorState::Play)
            {
                if (m_GameModule)
                {
                    GameModuleContext ctx = BuildModuleContext();
                    m_GameModule->OnStepPlay(ctx, dt, m_PlayCamera);
                }
            }
            else
            {
                UpdateEditorCamera(dt);
                TweenSystem::Get().Step(m_Scene, dt);
                if (m_LivePreviewAnimations && m_GameModule && m_GameModule->HandlesEditorAnimationPreview())
                {
                    GameModuleContext ctx = BuildModuleContext();
                    m_GameModule->OnStepEditorPreview(ctx, dt);
                }
                m_Renderer3D.UpdateParticles(dt);
            }

            if (!m_RHI.BeginFrame())
                continue;

            m_ImGui.BeginFrame();
            RenderUI(dt);

            m_CommandList->open();

            // 1. Render 3D Scene into the offscreen Viewport framebuffer
            CameraView activeCam = (m_State == EditorState::Edit) ? m_EditorCamera : m_PlayCamera;
            activeCam.aspectRatio = static_cast<float>(m_ViewportTarget.width) / std::max(1.0f, static_cast<float>(m_ViewportTarget.height));

            m_Renderer3D.RenderScene(
                m_CommandList,
                m_ViewportTarget.framebuffer,
                m_Scene,
                activeCam,
                m_TotalTime,
                (m_State == EditorState::Edit) && m_DrawGrid,
                (m_State == EditorState::Edit) ? m_SelectedEntity : kInvalidEntity,
                m_GizmoMode,
                (m_DraggedGizmoAxis >= 0) ? m_DraggedGizmoAxis : m_HoveredGizmoAxis,
                m_DrawColliders);

            // 2. Render Dear ImGui Editor Interface into the main window swapchain framebuffer
            nvrhi::IFramebuffer* mainFb = m_RHI.GetCurrentFramebuffer();
            nvrhi::utils::ClearColorAttachment(m_CommandList, mainFb, 0, nvrhi::Color(0.06f, 0.08f, 0.11f, 1.0f));
            m_ImGui.Render(m_CommandList, mainFb);

            m_CommandList->close();
            m_RHI.FinishFrameAndPresent(m_CommandList);

            // Capture screenshots during autotest for visual verification
            if (m_AutoTestMode)
            {
                if (m_FrameCounter == 12)
                {
                    m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), "screenshot_editor_aether_realms.bmp");
                }
                else if (m_FrameCounter == 48)
                {
                    m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), "screenshot_editor_play_mode.bmp");
                }
                else if (m_FrameCounter == 64)
                {
                    m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), "screenshot_editor_edit_mode.bmp");
                }
            }
        }
    }

    void EditorApp::RenderUI(float dt)
    {
        DrawMainMenuBar();
        DrawToolbar();
        DrawHierarchyPanel();
        DrawViewportPanel(dt);
        DrawInspectorPanel();
        DrawBottomPanel(dt);
        DrawExportModal();
        DrawProjectManagerModal();
    }

    void EditorApp::DrawMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Project"))
            {
                if (ImGui::MenuItem("Project Manager (New / Open / Switch Project)..."))
                {
                    m_DiscoveredProjects = ProjectManager::DiscoverProjects("projects");
                    m_ShowProjectManagerModal = true;
                }
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "Switch Active Project:");
                for (const auto& proj : m_DiscoveredProjects)
                {
                    bool isCur = (proj.projectName == m_ProjectConfig.projectName);
                    std::string itemLabel = proj.gameTitle + " [" + proj.genre + "]";
                    if (ImGui::MenuItem(itemLabel.c_str(), nullptr, isCur))
                    {
                        OpenProject(proj);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Project Configuration (project.hproject)") && !m_ProjectConfig.projectRootDir.empty())
                {
                    SceneSerializer::SaveProjectConfig(m_ProjectConfig, m_ProjectConfig.projectRootDir + "/project.hproject");
                }
                if (ImGui::MenuItem("Export Active Project as Standalone Game...", "Ctrl+E"))
                {
                    m_ShowExportModal = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Empty 3D Scene"))
                {
                    StopPlayMode();
                    SceneTemplates::BuildEmptyTemplateScene(m_Scene, "Untitled 3D Scene");
                }
                if (ImGui::MenuItem("Apply Template: Floating-Island 3D Platformer"))
                {
                    StopPlayMode();
                    SceneTemplates::BuildAetherRealmsScene(m_Scene);
                }
                if (ImGui::MenuItem("Apply Template: Skyway 3D Obstacle Course"))
                {
                    StopPlayMode();
                    SceneTemplates::BuildSkywayCourseScene(m_Scene, m_ProjectConfig.gameTitle);
                }
                if (ImGui::MenuItem("Apply Template: Top-Down Wave Defense Arena"))
                {
                    StopPlayMode();
                    SceneTemplates::BuildStarforgeOutpostScene(m_Scene);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                {
                    SceneSerializer::SaveToFile(m_Scene, m_CurrentScenePath);
                }
                if (ImGui::MenuItem("Load Scene"))
                {
                    StopPlayMode();
                    SceneSerializer::LoadFromFile(m_Scene, m_CurrentScenePath);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Editor Screenshot (BMP)"))
                {
                    m_RHI.SaveTextureToBMP(m_RHI.GetCurrentBackbuffer(), "editor_screenshot.bmp");
                }
                if (ImGui::MenuItem("Exit"))
                {
                    m_Window.RequestClose();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_UndoRedo.HasUndo()))
                {
                    m_UndoRedo.Undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_UndoRedo.HasRedo()))
                {
                    m_UndoRedo.Redo();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false, m_SelectedEntity != kInvalidEntity))
                {
                    m_SelectedEntity = m_Scene.DuplicateEntity(m_SelectedEntity);
                }
                if (ImGui::MenuItem("Delete Selected", "Del", false, m_SelectedEntity != kInvalidEntity))
                {
                    m_Scene.DestroyEntity(m_SelectedEntity);
                    m_Scene.FlushDestroyedEntities();
                    m_SelectedEntity = kInvalidEntity;
                }
                if (ImGui::MenuItem("Frame Selected", "F", false, m_SelectedEntity != kInvalidEntity))
                {
                    if (Entity* sel = m_Scene.GetEntity(m_SelectedEntity))
                        m_EditorCamera.position = sel->transform.position - m_EditorCamera.forward * 10.0f;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create 3D Object"))
            {
                auto spawnPrim = [this](const char* name, MeshPrimitive prim) {
                    Entity& e = m_Scene.CreateEntity(name, "Default");
                    e.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 8.0f;
                    e.transform.position.y = std::max(0.6f, e.transform.position.y);
                    e.mesh.primitive = prim;
                    m_SelectedEntity = e.id;
                };
                if (ImGui::MenuItem("Cube"))     spawnPrim("Cube",     MeshPrimitive::Cube);
                if (ImGui::MenuItem("Sphere"))   spawnPrim("Sphere",   MeshPrimitive::Sphere);
                if (ImGui::MenuItem("Cylinder")) spawnPrim("Cylinder", MeshPrimitive::Cylinder);
                if (ImGui::MenuItem("Cone"))     spawnPrim("Cone",     MeshPrimitive::Cone);
                if (ImGui::MenuItem("Torus"))    spawnPrim("Torus",    MeshPrimitive::Torus);
                ImGui::Separator();
                if (ImGui::MenuItem("CPUParticles3D Fountain Node"))
                {
                    Entity& p = m_Scene.CreateEntity("ParticleFountain", "Default");
                    p.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 7.0f;
                    p.mesh.primitive = MeshPrimitive::Sphere;
                    p.transform.scale = glm::vec3(0.45f);
                    p.particleEmitter.enabled = true;
                    p.particleEmitter.emitting = true;
                    p.particleEmitter.emissionShape = 3; // Ring
                    p.particleEmitter.emissionRate = 25.0f;
                    m_SelectedEntity = p.id;
                }
                if (ImGui::MenuItem("Camera3D + SpringArm Orbit Node"))
                {
                    Entity& c = m_Scene.CreateEntity("OrbitCamera3D", "Camera");
                    c.transform.position = m_EditorCamera.position;
                    c.mesh.enabled = false;
                    c.camera.enabled = true;
                    c.camera.mode = CameraMode::ThirdPersonOrbit;
                    m_SelectedEntity = c.id;
                }
                if (ImGui::MenuItem("Point Light"))
                {
                    Entity& l = m_Scene.CreateEntity("PointLight", "Light");
                    l.transform.position = m_EditorCamera.position + m_EditorCamera.forward * 6.0f;
                    l.mesh.primitive = MeshPrimitive::Sphere;
                    l.transform.scale = glm::vec3(0.35f);
                    l.light.enabled = true;
                    l.light.color = glm::vec3(0.3f, 0.85f, 1.0f);
                    l.light.intensity = 3.5f;
                    l.light.range = 14.0f;
                    m_SelectedEntity = l.id;
                }
                ImGui::EndMenu();
            }

            // Optional project-specific Editor Menu provided by the active IGameModule
            if (m_GameModule)
            {
                GameModuleContext ctx = BuildModuleContext();
                CameraView camBefore = m_EditorCamera;
                m_GameModule->DrawEditorMenu(ctx, m_EditorCamera, m_EditorCamYaw, m_EditorCamPitch);
                if (camBefore.position != m_EditorCamera.position || camBefore.forward != m_EditorCamera.forward)
                    StopPlayMode();
            }

            // Right-aligned Engine Status Info
            char statusBuf[256];
            std::snprintf(statusBuf, sizeof(statusBuf), "Project: %s | RHI: %s | %.0f FPS",
                m_ProjectConfig.projectName.c_str(), m_RHI.GetBackendName().c_str(), m_SmoothedFPS);
            float textW = ImGui::CalcTextSize(statusBuf).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - textW - 18.0f);
            ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "%s", statusBuf);

            ImGui::EndMainMenuBar();
        }
    }

    void EditorApp::DrawToolbar()
    {
        float menuH = ImGui::GetFrameHeight();
        float winW = static_cast<float>(m_Window.GetWidth());

        ImGui::SetNextWindowPos(ImVec2(0.0f, menuH));
        ImGui::SetNextWindowSize(ImVec2(winW, 42.0f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
        if (ImGui::Begin("##MainToolbar", nullptr, flags))
        {
            // Active Project Quick-Switcher
            ImGui::SetNextItemWidth(175.0f);
            if (ImGui::BeginCombo("##ActiveProjectCombo", ("Project: " + m_ProjectConfig.projectName).c_str()))
            {
                for (const auto& proj : m_DiscoveredProjects)
                {
                    bool isSel = (proj.projectName == m_ProjectConfig.projectName);
                    std::string lbl = proj.projectName + " (" + proj.genre + ")";
                    if (ImGui::Selectable(lbl.c_str(), isSel))
                        OpenProject(proj);
                }
                ImGui::Separator();
                if (ImGui::Selectable("+ New / Manage Projects..."))
                {
                    m_ShowProjectManagerModal = true;
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();

            // Gizmo Mode buttons
            auto modeBtn = [&](const char* label, GizmoMode mode) {
                bool active = (m_GizmoMode == mode);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
                if (ImGui::Button(label, ImVec2(90.0f, 28.0f)))
                    m_GizmoMode = mode;
                if (active) ImGui::PopStyleColor();
            };

            modeBtn("[W] Move", GizmoMode::Translate);
            ImGui::SameLine();
            modeBtn("[E] Rotate", GizmoMode::Rotate);
            ImGui::SameLine();
            modeBtn("[R] Scale", GizmoMode::Scale);

            ImGui::SameLine();
            ImGui::Checkbox("Grid", &m_DrawGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Anim", &m_LivePreviewAnimations);

            ImGui::SameLine();
            ImGui::BeginDisabled(!m_UndoRedo.HasUndo());
            if (ImGui::Button("Undo", ImVec2(52.0f, 28.0f)))
                m_UndoRedo.Undo();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!m_UndoRedo.HasRedo());
            if (ImGui::Button("Redo", ImVec2(52.0f, 28.0f)))
                m_UndoRedo.Redo();
            ImGui::EndDisabled();

            // Center Play / Pause / Stop Controls
            float centerPos = winW * 0.5f + 45.0f;
            if (centerPos > ImGui::GetCursorPosX())
                ImGui::SameLine(centerPos);
            else
                ImGui::SameLine();

            if (m_State == EditorState::Edit)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.62f, 0.32f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.78f, 0.40f, 1.0f));
                if (ImGui::Button("PLAY PROJECT", ImVec2(125.0f, 28.0f)))
                    StartPlayMode();
                ImGui::PopStyleColor(2);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.48f, 0.12f, 1.0f));
                if (ImGui::Button(m_State == EditorState::Paused ? "RESUME" : "PAUSE", ImVec2(85.0f, 28.0f)))
                {
                    m_State = (m_State == EditorState::Paused) ? EditorState::Play : EditorState::Paused;
                }
                ImGui::PopStyleColor();

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.20f, 0.20f, 1.0f));
                if (ImGui::Button("STOP", ImVec2(80.0f, 28.0f)))
                    StopPlayMode();
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();
            if (ImGui::Button("Projects...", ImVec2(90.0f, 28.0f)))
            {
                m_DiscoveredProjects = ProjectManager::DiscoverProjects("projects");
                m_ShowProjectManagerModal = true;
            }

            // Right-side Export Game button
            ImGui::SameLine(winW - 175.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.82f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.62f, 0.98f, 1.0f));
            if (ImGui::Button("EXPORT PROJECT...", ImVec2(160.0f, 28.0f)))
            {
                m_ShowExportModal = true;
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApp::DrawHierarchyPanel()
    {
        float topY = ImGui::GetFrameHeight() + 42.0f;
        float winH = static_cast<float>(m_Window.GetHeight());
        float bottomH = 225.0f;
        float panelH = std::max(200.0f, winH - topY - bottomH);

        ImGui::SetNextWindowPos(ImVec2(0.0f, topY));
        ImGui::SetNextWindowSize(ImVec2(280.0f, panelH));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

        if (ImGui::Begin("Scene Hierarchy", nullptr, flags))
        {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "%s (%zu nodes)",
                m_Scene.sceneName.c_str(), m_Scene.GetEntities().size());

            char filterBuf[128]{};
            std::strncpy(filterBuf, m_HierarchyFilter.c_str(), sizeof(filterBuf) - 1);
            if (ImGui::InputTextWithHint("##Search", "Filter nodes...", filterBuf, sizeof(filterBuf)))
                m_HierarchyFilter = filterBuf;

            ImGui::SameLine();
            if (ImGui::Button("+ Add"))
            {
                Entity& e = m_Scene.CreateEntity("NewNode3D", "Default");
                e.transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
                m_SelectedEntity = e.id;
            }

            ImGui::Separator();

            ImGui::BeginChild("EntityListScroll", ImVec2(0.0f, panelH - 210.0f), true);
            for (const auto& e : m_Scene.GetEntities())
            {
                if (!e.alive) continue;
                if (!m_HierarchyFilter.empty() && e.tag.name.find(m_HierarchyFilter) == std::string::npos)
                    continue;

                bool isSelected = (e.id == m_SelectedEntity);
                std::string indent = (e.parentId != kInvalidEntity) ? "  |- " : "";
                std::string label = indent + e.tag.name + "  [" + e.tag.tag + "]###ent_" + std::to_string(e.id);

                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    m_SelectedEntity = e.id;
                    if (!e.luaScript.scriptPath.empty())
                        OpenLuaScriptInEditor(e.luaScript.scriptPath);
                }
            }
            ImGui::EndChild();

            if (ImGui::CollapsingHeader("Godot WorldEnvironment & Tonemap", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit3("Sky Zenith", &m_Scene.skyZenithColor.x);
                ImGui::ColorEdit3("Sky Horizon", &m_Scene.skyHorizonColor.x);
                ImGui::SliderFloat("Ambient", &m_Scene.ambientIntensity, 0.1f, 1.5f);
                ImGui::SliderFloat("Fog", &m_Scene.fogDensity, 0.0f, 0.02f, "%.4f");
                const char* tmModes[] = { "Linear", "Reinhard", "Filmic", "ACES", "AgX (Godot)" };
                int curTm = static_cast<int>(m_Scene.worldEnv.tonemapMode);
                if (ImGui::Combo("ToneMapper", &curTm, tmModes, 5))
                    m_Scene.worldEnv.tonemapMode = static_cast<ToneMapperMode>(curTm);
                ImGui::SliderFloat("Exposure", &m_Scene.worldEnv.tonemapExposure, 0.4f, 2.2f);
                ImGui::SliderFloat("Glow/Bloom", &m_Scene.worldEnv.glowIntensity, 0.0f, 1.5f);
                ImGui::Checkbox("Frustum Cull", &m_Scene.worldEnv.frustumCullingEnabled);
                ImGui::Checkbox("PSSM + Omni Shadows", &m_Scene.worldEnv.shadowsEnabled);
                ImGui::Checkbox("Screen-Space AO (SSAO)", &m_Scene.worldEnv.ssaoEnabled);
                if (m_Scene.worldEnv.ssaoEnabled)
                {
                    ImGui::SliderFloat("SSAO Radius", &m_Scene.worldEnv.ssaoRadius, 0.25f, 3.0f);
                    ImGui::SliderFloat("SSAO Intensity", &m_Scene.worldEnv.ssaoIntensity, 0.2f, 3.0f);
                }
            }
        }
        ImGui::End();
    }

    void EditorApp::DrawViewportPanel(float dt)
    {
        float topY = ImGui::GetFrameHeight() + 42.0f;
        float winW = static_cast<float>(m_Window.GetWidth());
        float winH = static_cast<float>(m_Window.GetHeight());
        float leftW = 280.0f;
        float rightW = 350.0f;
        float bottomH = 225.0f;

        float vpWindowW = std::max(200.0f, winW - leftW - rightW);
        float vpWindowH = std::max(200.0f, winH - topY - bottomH);

        ImGui::SetNextWindowPos(ImVec2(leftW, topY));
        ImGui::SetNextWindowSize(ImVec2(vpWindowW, vpWindowH));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
        if (ImGui::Begin("3D Viewport (NVRHI)", nullptr, flags))
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            uint32_t targetW = static_cast<uint32_t>(std::max(64.0f, avail.x));
            uint32_t targetH = static_cast<uint32_t>(std::max(64.0f, avail.y));

            if (targetW != m_ViewportTarget.width || targetH != m_ViewportTarget.height)
            {
                m_RHI.WaitForIdle();
                m_ImGui.InvalidateTextureCache();
                m_ViewportTarget = m_RHI.CreateOffscreenTarget(targetW, targetH);
            }

            m_ViewportPos = ImGui::GetCursorScreenPos();
            m_ViewportSize = avail;

            ImTextureID texId = static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_ViewportTarget.colorTexture.Get()));
            ImGui::Image(texId, avail);

            m_ViewportHovered = ImGui::IsItemHovered();
            HandleViewportGizmoAndPicking();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (m_State != EditorState::Edit)
            {
                if (m_GameModule)
                {
                    GameModuleContext ctx = BuildModuleContext();
                    m_GameModule->OnRenderHUD(ctx, m_ViewportPos, m_ViewportSize, dl);
                }
            }
            else
            {
                // Editor Viewport Overlay Info Pill
                ImVec2 p0(m_ViewportPos.x + 12.0f, m_ViewportPos.y + 12.0f);
                dl->AddRectFilled(p0, ImVec2(p0.x + 515.0f, p0.y + 46.0f), IM_COL32(8, 14, 22, 185), 5.0f);
                dl->AddRect(p0, ImVec2(p0.x + 515.0f, p0.y + 46.0f), IM_COL32(60, 140, 210, 150), 5.0f);

                char info1[220];
                std::snprintf(info1, sizeof(info1), "PROJECT: %s | DrawCalls: %u | Tris: %u | Culled: %u",
                    m_ProjectConfig.projectName.c_str(),
                    m_Renderer3D.GetStats().drawCalls,
                    m_Renderer3D.GetStats().triangleCount,
                    m_Renderer3D.GetStats().culledEntities);
                dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 6.0f), IM_COL32(130, 225, 255, 255), info1);
                dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 25.0f), IM_COL32(200, 215, 230, 230),
                    "[LMB] Pick/Gizmo   [RMB+WASD] Fly Cam   [Project Combo] Switch Game Project");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        (void)dt;
    }

    void EditorApp::DrawInspectorPanel()
    {
        float topY = ImGui::GetFrameHeight() + 42.0f;
        float winW = static_cast<float>(m_Window.GetWidth());
        float winH = static_cast<float>(m_Window.GetHeight());
        float rightW = 350.0f;
        float bottomH = 225.0f;
        float panelH = std::max(200.0f, winH - topY - bottomH);

        ImGui::SetNextWindowPos(ImVec2(winW - rightW, topY));
        ImGui::SetNextWindowSize(ImVec2(rightW, panelH));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

        if (ImGui::Begin("Entity Inspector", nullptr, flags))
        {
            Entity* e = m_Scene.GetEntity(m_SelectedEntity);
            if (!e)
            {
                ImGui::TextDisabled("Select an entity in the Scene Hierarchy or 3D Viewport.");
            }
            else
            {
                char nameBuf[128]{};
                std::strncpy(nameBuf, e->tag.name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                    e->tag.name = nameBuf;

                const char* tags[] = { "Default", "Player", "Enemy", "Crystal", "Turret", "Core", "Obstacle", "Environment", "Light", "Camera" };
                int curTag = 0;
                for (int i = 0; i < 10; ++i)
                    if (e->tag.tag == tags[i]) curTag = i;
                if (ImGui::Combo("Gameplay Tag", &curTag, tags, 10))
                    e->tag.tag = tags[curTag];

                const char* procModes[] = { "Inherit", "Pausable", "When Paused", "Always", "Disabled" };
                int curProc = static_cast<int>(e->processMode);
                if (ImGui::Combo("Process Mode", &curProc, procModes, 5))
                    e->processMode = static_cast<ProcessMode>(curProc);

                int parentIdInput = static_cast<int>(e->parentId);
                if (ImGui::InputInt("Parent Node ID", &parentIdInput))
                {
                    m_Scene.SetParent(e->id, parentIdInput > 0 ? static_cast<EntityID>(parentIdInput) : kInvalidEntity);
                }

                if (ImGui::Button("Save Node as PackedScene Prefab (.hprefab)", ImVec2(-1.0f, 0.0f)))
                {
                    std::string prefabPath = "assets/prefabs/" + e->tag.name + ".hprefab";
                    SceneSerializer::SaveEntityPrefab(*e, prefabPath);
                }

                ImGui::Separator();

                // 1. Transform Component
                if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3("Position", &e->transform.position.x, 0.1f);
                    ImGui::DragFloat3("Rotation", &e->transform.rotation.x, 1.0f);
                    ImGui::DragFloat3("Scale",    &e->transform.scale.x,    0.05f, 0.05f, 200.0f);
                }

                // 2. Godot AnimationPlayer Component
                if (ImGui::CollapsingHeader("AnimationPlayer Track (Godot)", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("Animation Enabled", &e->animation.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Playing", &e->animation.playing);
                    const char* trackTypes[] = { "IdleBob + Spin", "SpinContinuous", "PatrolPingPong", "PulseScale", "OrbitCircle", "PendulumSwing" };
                    if (ImGui::Combo("Track Type", &e->animation.trackType, trackTypes, 6))
                        e->animation.originCaptured = false;
                    ImGui::SliderFloat("Speed", &e->animation.speed, 0.1f, 120.0f);
                    ImGui::SliderFloat("Amplitude", &e->animation.amplitude, 0.05f, 5.0f);
                }

                // 3. Godot CPUParticles3D Emitter Component
                if (ImGui::CollapsingHeader("CPUParticles3D Emitter (Godot)", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("Emitter Enabled", &e->particleEmitter.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Emitting", &e->particleEmitter.emitting);
                    const char* shapes[] = { "Point", "Sphere", "Box", "Ring" };
                    ImGui::Combo("Emission Shape", &e->particleEmitter.emissionShape, shapes, 4);
                    ImGui::SliderFloat("Rate (/sec)", &e->particleEmitter.emissionRate, 1.0f, 80.0f);
                    ImGui::SliderFloat("Init Speed", &e->particleEmitter.initialSpeed, 0.5f, 20.0f);
                    ImGui::ColorEdit3("Start Color", &e->particleEmitter.startColor.x);
                    ImGui::ColorEdit3("End Color", &e->particleEmitter.endColor.x);
                }

                // 4. Godot Camera3D & SpringArm3D Component
                if (ImGui::CollapsingHeader("Camera3D & SpringArm (Godot)"))
                {
                    ImGui::Checkbox("Camera Enabled", &e->camera.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Current/Primary", &e->camera.isPrimary);
                    const char* camModes[] = { "Follow Offset", "Third-Person Orbit (SpringArm)", "First-Person", "Fixed Static" };
                    int cm = static_cast<int>(e->camera.mode);
                    if (ImGui::Combo("Camera Mode", &cm, camModes, 4))
                        e->camera.mode = static_cast<CameraMode>(cm);
                    ImGui::SliderFloat("FOV", &e->camera.fov, 35.0f, 110.0f);
                    ImGui::SliderFloat("SpringArm Length", &e->camera.springArmLength, 2.0f, 35.0f);
                    ImGui::SliderFloat("Orbit Pitch", &e->camera.orbitPitch, -45.0f, 75.0f);
                    ImGui::SliderFloat("Orbit Yaw", &e->camera.orbitYaw, -180.0f, 180.0f);
                }

                // 5. PBR Material Component
                if (ImGui::CollapsingHeader("PBR Material & Mesh Shadows", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("Cast Shadows", &e->mesh.castShadows);
                    ImGui::ColorEdit4("Albedo", &e->material.albedoColor.x);
                    ImGui::SliderFloat("Metallic", &e->material.metallic, 0.0f, 1.0f);
                    ImGui::SliderFloat("Roughness", &e->material.roughness, 0.05f, 1.0f);
                    ImGui::ColorEdit3("Emissive", &e->material.emissiveColor.x);
                    ImGui::SliderFloat("Glow Power", &e->material.emissiveStrength, 0.0f, 8.0f);
                }

                // 6. Lua Script Component
                if (ImGui::CollapsingHeader("Lua Script Component"))
                {
                    ImGui::Checkbox("Script Enabled", &e->luaScript.enabled);
                    char pathBuf[256]{};
                    std::strncpy(pathBuf, e->luaScript.scriptPath.c_str(), sizeof(pathBuf) - 1);
                    if (ImGui::InputText("Lua File", pathBuf, sizeof(pathBuf)))
                    {
                        e->luaScript.scriptPath = pathBuf;
                        e->luaScript.initialized = false;
                    }
                }

                // 7. AudioStreamPlayer3D Component (miniaudio)
                if (ImGui::CollapsingHeader("AudioStreamPlayer3D (miniaudio)"))
                {
                    ImGui::Checkbox("Audio Enabled", &e->audioSource.enabled);
                    ImGui::SliderFloat("Volume", &e->audioSource.volume, 0.0f, 1.0f);
                    ImGui::SliderFloat("Pitch Scale", &e->audioSource.pitch, 0.5f, 2.0f);
                    ImGui::SliderFloat("Pitch Randomness", &e->audioSource.pitchRandomness, 0.0f, 0.35f);
                    const char* attenModes[] = { "Inverse Distance", "Inverse Square", "Logarithmic", "Disabled" };
                    ImGui::Combo("Attenuation", &e->audioSource.attenuationModel, attenModes, 4);
                    ImGui::SliderFloat("Max Distance", &e->audioSource.maxDistance, 5.0f, 150.0f);
                    ImGui::Checkbox("Play On Awake", &e->audioSource.playOnAwake);
                }

                // 8. RigidBody3D Physics Component
                if (ImGui::CollapsingHeader("RigidBody3D Physics"))
                {
                    ImGui::Checkbox("RigidBody Enabled", &e->rigidBody.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Kinematic", &e->rigidBody.isKinematic);
                    ImGui::Checkbox("Use Gravity", &e->rigidBody.useGravity);
                    ImGui::SameLine();
                    ImGui::Checkbox("Freeze Y", &e->rigidBody.freezeY);
                    ImGui::SliderFloat("Mass", &e->rigidBody.mass, 0.1f, 50.0f);
                    ImGui::SliderFloat("Drag", &e->rigidBody.drag, 0.0f, 20.0f);
                    ImGui::SliderFloat("Bounciness", &e->rigidBody.bounciness, 0.0f, 1.0f);
                    ImGui::Text("Is On Floor: %s | Vel: (%.1f, %.1f, %.1f)",
                        e->rigidBody.isOnFloor ? "YES" : "NO",
                        e->rigidBody.velocity.x, e->rigidBody.velocity.y, e->rigidBody.velocity.z);
                }

                // 9. CollisionShape3D & Layer/Mask Filter
                if (ImGui::CollapsingHeader("Collider3D & Layer Mask"))
                {
                    ImGui::Checkbox("Collider Enabled", &e->collider.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Is Sphere", &e->collider.isSphere);
                    ImGui::SameLine();
                    ImGui::Checkbox("Is Trigger", &e->collider.isTrigger);
                    if (e->collider.isSphere)
                        ImGui::SliderFloat("Sphere Radius", &e->collider.sphereRadius, 0.1f, 15.0f);
                    else
                        ImGui::DragFloat3("Box Half Extents", &e->collider.boxHalfExtents.x, 0.1f, 0.05f, 100.0f);
                    int layer = static_cast<int>(e->collider.collisionLayer);
                    if (ImGui::InputInt("Collision Layer", &layer))
                        e->collider.collisionLayer = static_cast<uint32_t>(std::max(0, layer));
                    int mask = static_cast<int>(e->collider.collisionMask & 0x7FFFFFFFu);
                    if (ImGui::InputInt("Collision Mask", &mask))
                        e->collider.collisionMask = static_cast<uint32_t>(mask);
                    if (e->collider.isTrigger)
                    {
                        const char* areaModes[] = { "None (Trigger Only)", "Replace Gravity", "Wind / Force Field" };
                        ImGui::Combo("Area3D Override", &e->collider.areaSpaceOverride, areaModes, 3);
                        if (e->collider.areaSpaceOverride == 1)
                            ImGui::DragFloat3("Area Gravity", &e->collider.areaGravityVec.x, 0.25f);
                        else if (e->collider.areaSpaceOverride == 2)
                            ImGui::DragFloat3("Area Force", &e->collider.areaForceVec.x, 0.5f);
                        ImGui::SliderFloat("Area Linear Damp", &e->collider.areaLinearDamp, 0.0f, 10.0f);
                    }
                }

                // 9b. Godot RayCast3D Sensor Node
                if (ImGui::CollapsingHeader("RayCast3D Sensor (Godot)"))
                {
                    ImGui::Checkbox("RayCast Enabled", &e->raycast.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Debug Ray", &e->raycast.showDebugRay);
                    ImGui::DragFloat3("Target Position", &e->raycast.targetPosition.x, 0.25f);
                    ImGui::Checkbox("Collide With Triggers", &e->raycast.collideWithTriggers);
                    ImGui::Text("Hit: %s | Dist: %.2f | Entity: %llu",
                        e->raycast.isColliding ? "YES" : "NO",
                        e->raycast.hitDistance,
                        static_cast<unsigned long long>(e->raycast.hitEntity));
                }

                // 10. Light3D Component
                if (ImGui::CollapsingHeader("Light3D (Directional / Omni)"))
                {
                    ImGui::Checkbox("Light Enabled", &e->light.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Directional", &e->light.isDirectional);
                    ImGui::Checkbox("Cast Shadows", &e->light.castShadows);
                    ImGui::ColorEdit3("Light Color", &e->light.color.x);
                    ImGui::SliderFloat("Intensity", &e->light.intensity, 0.0f, 12.0f);
                    ImGui::SliderFloat("Range", &e->light.range, 1.0f, 100.0f);
                    ImGui::SliderFloat("Shadow Opacity", &e->light.shadowOpacity, 0.0f, 1.0f);
                    ImGui::SliderFloat("Shadow Bias", &e->light.shadowBias, 0.0002f, 0.02f, "%.4f");
                    ImGui::SliderFloat("Normal Bias", &e->light.shadowNormalBias, 0.0f, 0.2f, "%.3f");
                    ImGui::SliderFloat("Shadow Blur", &e->light.shadowBlur, 0.5f, 3.0f);
                    if (e->light.isDirectional)
                        ImGui::SliderFloat("Max Distance", &e->light.shadowMaxDistance, 25.0f, 150.0f);
                }
            }
        }
        ImGui::End();
    }

    void EditorApp::DrawBottomPanel(float /*dt*/)
    {
        float winW = static_cast<float>(m_Window.GetWidth());
        float winH = static_cast<float>(m_Window.GetHeight());
        float bottomH = 225.0f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, winH - bottomH));
        ImGui::SetNextWindowSize(ImVec2(winW, bottomH));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

        if (ImGui::Begin("Engine Workspace: Project Browser | Lua Script Editor | Console | Godot Systems", nullptr, flags))
        {
            if (ImGui::BeginTabBar("BottomTabs"))
            {
                // Tab 1: Active Projects & Prefab Browser
                if (ImGui::BeginTabItem("Projects & PackedScene Prefabs"))
                {
                    ImGui::Columns(3, "ProjCols", true);
                    ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "Independent Game Projects (projects/)");
                    for (const auto& proj : m_DiscoveredProjects)
                    {
                        bool isCur = (proj.projectName == m_ProjectConfig.projectName);
                        std::string lbl = (isCur ? ">> [ACTIVE] " : "   ") + proj.gameTitle + " (" + proj.genre + ")";
                        if (ImGui::Selectable(lbl.c_str(), isCur))
                        {
                            OpenProject(proj);
                        }
                    }
                    if (ImGui::Button("+ Create / Manage Game Projects..."))
                        m_ShowProjectManagerModal = true;
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "Active Project Configuration");
                    ImGui::Text("Project Slug : %s", m_ProjectConfig.projectName.c_str());
                    ImGui::Text("Game Title   : %s", m_ProjectConfig.gameTitle.c_str());
                    ImGui::Text("Genre        : %s", m_ProjectConfig.genre.c_str());
                    ImGui::Text("Module       : %s", m_ProjectConfig.gameplayModule.c_str());
                    ImGui::Text("Startup Scene: %s", m_ProjectConfig.startupScene.c_str());
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.65f, 1.0f), "PackedScene Prefabs (.hprefab)");
                    std::filesystem::create_directories("assets/prefabs");
                    for (const auto& entry : std::filesystem::directory_iterator("assets/prefabs"))
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".hprefab")
                        {
                            std::string pName = entry.path().filename().string();
                            if (ImGui::Button(("Instantiate " + pName).c_str()))
                            {
                                glm::vec3 spawnPos = m_EditorCamera.position + m_EditorCamera.forward * 8.0f;
                                m_SelectedEntity = SceneSerializer::InstantiatePrefab(m_Scene, entry.path().string(), spawnPos);
                            }
                        }
                    }
                    ImGui::Columns(1);
                    ImGui::EndTabItem();
                }

                // Tab 2: Built-in Lua Script Editor
                if (ImGui::BeginTabItem("Lua Script Editor (Hot-Reload)"))
                {
                    const char* scriptFiles[] = {
                        "assets/scripts/player_rover.lua",
                        "assets/scripts/enemy_drone.lua",
                        "assets/scripts/crystal_pickup.lua",
                        "assets/scripts/defense_turret.lua",
                        "assets/scripts/core_reactor.lua"
                    };
                    for (int i = 0; i < 5; ++i)
                    {
                        if (i > 0) ImGui::SameLine();
                        bool active = (m_OpenScriptPath == scriptFiles[i]);
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.55f, 0.85f, 1.0f));
                        std::string fname = std::filesystem::path(scriptFiles[i]).filename().string();
                        if (ImGui::Button(fname.c_str()))
                            OpenLuaScriptInEditor(scriptFiles[i]);
                        if (active) ImGui::PopStyleColor();
                    }

                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.62f, 0.35f, 1.0f));
                    if (ImGui::Button("Save & Hot-Reload Lua Script"))
                        SaveAndReloadCurrentLuaScript();
                    ImGui::PopStyleColor();

                    std::vector<char> editBuf(m_ScriptEditorBuffer.begin(), m_ScriptEditorBuffer.end());
                    editBuf.resize(std::max<size_t>(editBuf.size() + 1024, 8192), '\0');
                    if (ImGui::InputTextMultiline("##LuaSourceEditor", editBuf.data(), editBuf.size(), ImVec2(-1.0f, -1.0f)))
                    {
                        m_ScriptEditorBuffer = editBuf.data();
                    }
                    ImGui::EndTabItem();
                }

                // Tab 3: Godot Engine Evolution Systems (InputMap | UndoRedo | Tweens | Signals)
                if (ImGui::BeginTabItem("Godot Systems: InputMap | UndoRedo | Tweens | Signals"))
                {
                    ImGui::Columns(3, "GodotSysCols", true);

                    ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "UndoRedo History (%d actions)", m_UndoRedo.GetHistoryCount());
                    if (ImGui::Button("Undo (Ctrl+Z)")) m_UndoRedo.Undo();
                    ImGui::SameLine();
                    if (ImGui::Button("Redo (Ctrl+Y)")) m_UndoRedo.Redo();
                    for (int i = 0; i < m_UndoRedo.GetHistoryCount(); ++i)
                    {
                        bool isCur = (i == m_UndoRedo.GetCurrentActionIndex());
                        ImGui::TextColored(isCur ? ImVec4(0.35f, 1.0f, 0.55f, 1.0f) : ImVec4(0.75f, 0.78f, 0.82f, 1.0f),
                            "%s[%d] %s", isCur ? ">> " : "   ", i, m_UndoRedo.GetActionName(i).c_str());
                    }
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.35f, 1.0f), "Tween Engine (%zu active)", TweenSystem::Get().GetActiveTweenCount());
                    if (Entity* sel = m_Scene.GetEntity(m_SelectedEntity))
                    {
                        if (ImGui::Button("Tween Bounce Scale on Selected"))
                        {
                            glm::vec3 baseScl = sel->transform.scale;
                            sel->transform.scale = baseScl * 1.45f;
                            TweenSystem::Get().TweenEntityVec3(m_Scene, sel->id, TweenProperty::Scale, baseScl, 0.65f, TweenTransition::Bounce, TweenEase::Out);
                        }
                        if (ImGui::Button("Tween Hop Position on Selected"))
                        {
                            glm::vec3 basePos = sel->transform.position;
                            sel->transform.position = basePos + glm::vec3(0.0f, 1.2f, 0.0f);
                            TweenSystem::Get().TweenEntityVec3(m_Scene, sel->id, TweenProperty::Position, basePos, 0.60f, TweenTransition::Elastic, TweenEase::Out);
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.65f, 1.0f), "InputMap Actions (%zu mapped)", InputMap::Get().GetActionMap().size());
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.95f, 0.55f, 1.0f, 1.0f), "Live Node Signals Emitted");
                    const auto& sigs = m_Scene.GetRecentSignals();
                    if (sigs.empty())
                        ImGui::TextDisabled("No signals emitted yet.");
                    else
                    {
                        for (int s = static_cast<int>(sigs.size()) - 1; s >= std::max(0, static_cast<int>(sigs.size()) - 5); --s)
                            ImGui::TextUnformatted(sigs[s].c_str());
                    }
                    ImGui::Columns(1);
                    ImGui::EndTabItem();
                }

                // Tab 4: Console & Live Lua REPL
                if (ImGui::BeginTabItem("Engine Console & Lua REPL"))
                {
                    if (ImGui::Button("Clear Console"))
                        Log::Clear();
                    ImGui::SameLine();

                    char replBuf[256]{};
                    std::strncpy(replBuf, m_LuaConsoleInput.c_str(), sizeof(replBuf) - 1);
                    ImGui::SetNextItemWidth(450.0f);
                    if (ImGui::InputText("##LuaREPL", replBuf, sizeof(replBuf), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        m_LuaConsoleInput = replBuf;
                        m_Scripts.ExecuteSnippet(m_LuaConsoleInput);
                    }
                    else
                    {
                        m_LuaConsoleInput = replBuf;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Execute Lua"))
                        m_Scripts.ExecuteSnippet(m_LuaConsoleInput);

                    ImGui::BeginChild("ConsoleScroll", ImVec2(0.0f, 0.0f), true);
                    auto entries = Log::GetEntries();
                    for (const auto& entry : entries)
                    {
                        ImVec4 col(0.85f, 0.88f, 0.92f, 1.0f);
                        if (entry.category == LogCategory::Error) col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                        else if (entry.category == LogCategory::Lua) col = ImVec4(0.45f, 0.95f, 0.65f, 1.0f);
                        else if (entry.category == LogCategory::RHI) col = ImVec4(0.45f, 0.85f, 1.0f, 1.0f);
                        else if (entry.category == LogCategory::Audio) col = ImVec4(1.0f, 0.85f, 0.35f, 1.0f);
                        else if (entry.category == LogCategory::Gameplay) col = ImVec4(0.95f, 0.55f, 1.0f, 1.0f);
                        ImGui::TextColored(col, "[%.2fs] %s", entry.timestamp, entry.message.c_str());
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void EditorApp::DrawExportModal()
    {
        if (!m_ShowExportModal)
            return;

        ImGui::SetNextWindowSize(ImVec2(560.0f, 340.0f), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(m_Window.GetWidth() * 0.5f - 280.0f, m_Window.GetHeight() * 0.5f - 170.0f),
            ImGuiCond_Appearing);

        if (ImGui::Begin("Export Independent Game Project", &m_ShowExportModal, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextColored(ImVec4(0.35f, 0.88f, 1.0f, 1.0f),
                "Package '%s' (%s) as a Standalone Game",
                m_ProjectConfig.gameTitle.c_str(), m_ProjectConfig.genre.c_str());
            ImGui::Separator();

            char titleBuf[128]{};
            std::strncpy(titleBuf, m_ProjectConfig.gameTitle.c_str(), sizeof(titleBuf) - 1);
            if (ImGui::InputText("Game Window Title", titleBuf, sizeof(titleBuf)))
                m_ProjectConfig.gameTitle = titleBuf;

            char exeBuf[128]{};
            std::strncpy(exeBuf, m_ProjectConfig.executableName.c_str(), sizeof(exeBuf) - 1);
            if (ImGui::InputText("Executable Name", exeBuf, sizeof(exeBuf)))
                m_ProjectConfig.executableName = exeBuf;

            char dirBuf[256]{};
            std::strncpy(dirBuf, m_ExportDirectory.c_str(), sizeof(dirBuf) - 1);
            if (ImGui::InputText("Export Folder", dirBuf, sizeof(dirBuf)))
                m_ExportDirectory = dirBuf;

            int res[2] = { static_cast<int>(m_ProjectConfig.windowWidth), static_cast<int>(m_ProjectConfig.windowHeight) };
            if (ImGui::InputInt2("Resolution", res))
            {
                m_ProjectConfig.windowWidth = static_cast<uint32_t>(std::max(640, res[0]));
                m_ProjectConfig.windowHeight = static_cast<uint32_t>(std::max(480, res[1]));
            }

            auto moduleIds = GameModuleRegistry::Get().GetRegisteredModuleIds();
            if (!moduleIds.empty() && ImGui::BeginCombo("Gameplay Module", m_ProjectConfig.gameplayModule.c_str()))
            {
                for (const auto& id : moduleIds)
                {
                    bool selected = (id == m_ProjectConfig.gameplayModule);
                    std::string label = id + " (" + GameModuleRegistry::Get().GetDisplayName(id) + ")";
                    if (ImGui::Selectable(label.c_str(), selected))
                        m_ProjectConfig.gameplayModule = id;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Include BadLevel Procedural Maps", &m_ProjectConfig.enableBadLevel);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.35f, 1.0f));
            if (ImGui::Button("Build & Export Standalone Project Now", ImVec2(-1.0f, 36.0f)))
            {
                if (!m_ProjectConfig.projectRootDir.empty())
                    SceneSerializer::SaveProjectConfig(m_ProjectConfig, m_ProjectConfig.projectRootDir + "/project.hproject");
                GameSystem::ExportStandaloneGame(m_Scene, m_ProjectConfig, m_ExportDirectory, m_ExportStatusMessage);
            }
            ImGui::PopStyleColor();

            if (!m_ExportStatusMessage.empty())
            {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", m_ExportStatusMessage.c_str());
            }
        }
        ImGui::End();
    }

    void EditorApp::DrawProjectManagerModal()
    {
        if (!m_ShowProjectManagerModal)
            return;

        ImGui::SetNextWindowSize(ImVec2(680.0f, 475.0f), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(m_Window.GetWidth() * 0.5f - 340.0f, m_Window.GetHeight() * 0.5f - 237.0f),
            ImGuiCond_Appearing);

        if (ImGui::Begin("Hargon Engine — Project Manager (Create / Open / Configure Projects)", &m_ShowProjectManagerModal, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextColored(ImVec4(0.40f, 0.88f, 1.0f, 1.0f), "1. Existing Game Projects in Workspace (projects/)");
            ImGui::BeginChild("ExistingProjectsList", ImVec2(0.0f, 165.0f), true);
            for (const auto& proj : m_DiscoveredProjects)
            {
                ImGui::PushID(proj.projectName.c_str());
                bool isCur = (proj.projectName == m_ProjectConfig.projectName);
                if (ImGui::Button(isCur ? "Active" : "Open Project", ImVec2(105.0f, 0.0f)))
                {
                    OpenProject(proj);
                    m_ShowProjectManagerModal = false;
                }
                ImGui::SameLine();
                ImGui::TextColored(isCur ? ImVec4(0.35f, 1.0f, 0.55f, 1.0f) : ImVec4(0.92f, 0.94f, 0.98f, 1.0f),
                    "%s  —  [%s]  (Module: %s)",
                    proj.gameTitle.c_str(), proj.genre.c_str(), proj.gameplayModule.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "2. Create a Brand-New Game Project");

            char slugBuf[128]{};
            std::strncpy(slugBuf, m_NewProjectSlug.c_str(), sizeof(slugBuf) - 1);
            if (ImGui::InputText("Project Folder Name", slugBuf, sizeof(slugBuf)))
                m_NewProjectSlug = slugBuf;

            char titleBuf[128]{};
            std::strncpy(titleBuf, m_NewProjectTitle.c_str(), sizeof(titleBuf) - 1);
            if (ImGui::InputText("Game Title", titleBuf, sizeof(titleBuf)))
                m_NewProjectTitle = titleBuf;

            char genreBuf[128]{};
            std::strncpy(genreBuf, m_NewProjectGenre.c_str(), sizeof(genreBuf) - 1);
            if (ImGui::InputText("Genre Description", genreBuf, sizeof(genreBuf)))
                m_NewProjectGenre = genreBuf;

            const char* templates[] = {
                "Empty 3D Scene (Sun + Orbit Camera + Ground + Player)",
                "3D Floating-Island Action-Platformer (Aether Realms Template)",
                "Sci-Fi Top-Down Rover & Turret Defense (Starforge Template)",
                "Single-Player Retro FPS with BadLevel Generator",
                "Skyway 3D Obstacle Course & Time-Trial (Non-Combat Template)"
            };
            ImGui::Combo("Starter Template", &m_NewProjectTemplate, templates, 5);
            ImGui::Checkbox("Enable Optional BadLevel Level Generator Integration", &m_NewProjectBadLevel);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.35f, 1.0f));
            if (ImGui::Button("Create & Open New Project", ImVec2(-1.0f, 34.0f)))
            {
                GameProjectConfig newCfg;
                if (ProjectManager::CreateNewProject(
                    m_NewProjectSlug, m_NewProjectTitle, m_NewProjectGenre,
                    m_NewProjectTemplate, m_NewProjectBadLevel, newCfg, &m_ProjectModalStatus))
                {
                    m_DiscoveredProjects = ProjectManager::DiscoverProjects("projects");
                    OpenProject(newCfg);
                }
            }
            ImGui::PopStyleColor();

            if (!m_ProjectModalStatus.empty())
            {
                ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.65f, 1.0f), "%s", m_ProjectModalStatus.c_str());
            }
        }
        ImGui::End();
    }
}

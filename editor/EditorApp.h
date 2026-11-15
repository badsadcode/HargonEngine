#pragma once
#include "../engine/core/Window.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/project/ProjectManager.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/core/UndoRedo.h"
#include "../engine/core/InputMap.h"
#include "../engine/animation/TweenSystem.h"
#include "../engine/runtime/GameModule.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace Hargon
{
    enum class EditorState
    {
        Edit = 0,
        Play = 1,
        Paused = 2
    };

    class EditorApp
    {
    public:
        EditorApp() = default;
        ~EditorApp();

        bool Init(
            GraphicsBackend backend = GraphicsBackend::Vulkan,
            bool enableValidation = true,
            bool autoTestMode = false,
            const std::string& initialProjectSlug = "",
            int autoCreateTemplate = -1);
        void Run();
        void Shutdown();

        bool OpenProject(GameProjectConfig config);
        bool IsActiveProject(const GameProjectConfig& candidate) const;
        bool OpenProjectByInput(const std::string& input, std::string* outError = nullptr);
        void RefreshDiscoveredProjects();

    private:
        void UpdateEditorCamera(float dt);
        void HandleViewportGizmoAndPicking();

        void StartPlayMode();
        void StopPlayMode();

        void RenderUI(float dt);
        void DrawMainMenuBar();
        void DrawToolbar();
        void DrawHierarchyPanel();
        void DrawInspectorPanel();
        void DrawViewportPanel(float dt);
        void DrawBottomPanel(float dt);
        void DrawExportModal();
        void DrawProjectManagerModal();

        void OpenLuaScriptInEditor(const std::string& path);
        void SaveAndReloadCurrentLuaScript();
        GameModuleContext BuildModuleContext();

        glm::vec3 ComputeViewportRay(const ImVec2& mousePos) const;

    private:
        Window m_Window;
        RHIDevice m_RHI;
        Renderer3D m_Renderer3D;
        ImGuiNVRHI m_ImGui;
        AudioEngine m_Audio;
        PhysicsSystem m_Physics;
        ScriptEngine m_Scripts;

        Scene m_Scene;
        std::unique_ptr<IGameModule> m_GameModule;
        UndoRedo m_UndoRedo;

        std::vector<GameProjectConfig> m_DiscoveredProjects;
        GameProjectConfig m_ProjectConfig;
        std::string m_CurrentScenePath;

        OffscreenTarget m_ViewportTarget;
        nvrhi::CommandListHandle m_CommandList;

        EditorState m_State = EditorState::Edit;
        EntityID m_SelectedEntity = kInvalidEntity;
        GizmoMode m_GizmoMode = GizmoMode::Translate;
        int m_HoveredGizmoAxis = -1;
        int m_DraggedGizmoAxis = -1;
        glm::vec2 m_DragStartMouse{ 0.0f };
        glm::vec3 m_DragStartVec{ 0.0f };

        CameraView m_EditorCamera;
        CameraView m_PlayCamera;
        float m_EditorCamYaw = 0.0f;
        float m_EditorCamPitch = 25.0f;
        float m_EditorCamSpeed = 18.0f;

        bool m_DrawGrid = true;
        bool m_DrawColliders = false;
        bool m_LivePreviewAnimations = true;
        bool m_ViewportHovered = false;
        ImVec2 m_ViewportPos{ 0.0f, 0.0f };
        ImVec2 m_ViewportSize{ 1024.0f, 640.0f };

        // UI state
        std::string m_HierarchyFilter;
        std::string m_OpenScriptPath = "assets/scripts/player_rover.lua";
        std::string m_ScriptEditorBuffer;
        std::string m_LuaConsoleInput = "Engine.log('Lua REPL Ready!')";

        bool m_ShowExportModal = false;
        std::string m_ExportDirectory = "Exports/GameProject";
        std::string m_ExportStatusMessage;

        bool m_ShowProjectManagerModal = false;
        std::string m_OpenProjectPathInput;
        std::string m_NewProjectSlug = "ChronoCitadel";
        std::string m_NewProjectTitle = "Chrono Citadel: Time Weaver";
        std::string m_NewProjectGenre = "3D Action-Puzzle Adventure";
        int m_NewProjectTemplate = 1;
        bool m_NewProjectBadLevel = false;
        bool m_ProjectModalError = false;
        std::string m_ProjectModalStatus;

        float m_TotalTime = 0.0f;
        float m_SmoothedFPS = 60.0f;

        // Automated verification state
        bool m_AutoTestMode = false;
        int m_FrameCounter = 0;
    };
}

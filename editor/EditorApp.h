#pragma once
#include "../engine/core/Window.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/runtime/FPSGameManager.h"
#include "../engine/core/UndoRedo.h"
#include "../engine/core/InputMap.h"
#include "../engine/animation/TweenSystem.h"
#include <imgui.h>
#include <string>

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

        bool Init(GraphicsBackend backend = GraphicsBackend::Vulkan, bool enableValidation = true, bool autoTestMode = false);
        void Run();
        void Shutdown();

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

        void OpenLuaScriptInEditor(const std::string& path);
        void SaveAndReloadCurrentLuaScript();

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
        FPSGameManager m_FPSGame;
        UndoRedo m_UndoRedo;
        GameProjectConfig m_ProjectConfig;
        std::string m_CurrentScenePath = "assets/scenes/starforge_outpost.hscene";
        std::string m_SavedEditSceneSnapshot;

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
        float m_EditorCamPitch = 32.0f;
        float m_EditorCamSpeed = 18.0f;

        bool m_DrawGrid = true;
        bool m_DrawColliders = false;
        bool m_ViewportHovered = false;
        ImVec2 m_ViewportPos{ 0.0f, 0.0f };
        ImVec2 m_ViewportSize{ 1024.0f, 640.0f };

        // UI state
        std::string m_HierarchyFilter;
        std::string m_OpenScriptPath = "assets/scripts/player_rover.lua";
        std::string m_ScriptEditorBuffer;
        std::string m_LuaConsoleInput = "Engine.log('Lua REPL Ready!')";

        bool m_ShowExportModal = false;
        std::string m_ExportDirectory = "ExportedGame";
        std::string m_ExportStatusMessage;

        float m_TotalTime = 0.0f;
        float m_SmoothedFPS = 60.0f;

        // Automated verification state
        bool m_AutoTestMode = false;
        int m_FrameCounter = 0;
    };
}

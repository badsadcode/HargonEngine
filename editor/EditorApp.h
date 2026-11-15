#pragma once
#include "EditorContext.h"
#include "EditorWorkspace.h"
#include "commands/EditorCommandSystem.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"
#include "panels/AssetBrowserPanel.h"
#include "panels/ScriptEditorPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/ProjectManagerWindow.h"

#include "../engine/core/Window.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/runtime/GameModule.h"
#include <memory>
#include <string>

namespace Hargon
{
    class EditorApp
    {
    public:
        bool Init(
            GraphicsBackend backend = GraphicsBackend::Vulkan,
            bool enableValidation = true,
            bool autoTest = false,
            const std::string& initialProject = "",
            int autoCreateTemplate = -1);

        void Run();
        void Shutdown();

        bool OpenProject(const GameProjectConfig& config);
        bool OpenProjectByInput(const std::string& inputPath);
        GameModuleContext BuildModuleContext();

    private:
        void SetupContextCallbacks();
        void UpdateCursorOwnership();
        void HandleGlobalShortcuts();
        void UpdateWindowTitle();

        void DrawMainMenuBar();
        void DrawMainToolbar(float toolbarHeight);

        bool HasAnyUnsavedWork() const;
        void RequestGuardedAction(const PendingUnsavedAction& action);
        void ExecutePendingAction(const PendingUnsavedAction& action);

        bool SaveCurrentScene();
        bool SaveCurrentSceneAs();
        bool LoadSceneFile(const std::string& scenePath);
        bool InstantiateAsset(const std::string& relOrAbsPath, const glm::vec3* worldPos);

        void StartPlayMode();
        void PausePlayMode();
        void StopPlayMode();
        void StepPlayMode();
        void FrameSelectedEntity();

        void RenderAndCaptureEditorScreenshot(const std::string& outPath, float simTime = 0.5f);
        void RunAutoTests();

    private:
        Window m_Window;
        RHIDevice m_RHI;
        AudioEngine m_Audio;
        Renderer3D m_Renderer3D;
        ImGuiNVRHI m_ImGui;
        PhysicsSystem m_Physics;
        ScriptEngine m_Scripts;

        GameProjectConfig m_ProjectConfig;
        std::unique_ptr<IGameModule> m_GameModule;
        std::string m_CurrentScenePath;
        Scene m_Scene;
        Scene m_SavedPlaySceneSnapshot;
        bool m_HasPlaySnapshot = false;

        PlayState m_PlayState = PlayState::Edit;
        CameraView m_EditorCamera;
        float m_EditorCamYaw = -90.0f;
        float m_EditorCamPitch = -22.0f;
        float m_SmoothedFPS = 60.0f;
        float m_TotalElapsedTime = 0.0f;
        bool m_AutoTestMode = false;
        bool m_ExitConfirmed = false;
        std::string m_LastTitleSignature;

        // Modular UI & Commands
        EditorContext m_Ctx;
        EditorCommandSystem m_Commands;
        EditorWorkspace m_Workspace;

        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ViewportPanel m_ViewportPanel;
        AssetBrowserPanel m_AssetBrowserPanel;
        ScriptEditorPanel m_ScriptEditorPanel;
        ConsolePanel m_ConsolePanel;
        ProjectManagerWindow m_ProjectManagerWindow;
    };
}

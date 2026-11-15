#pragma once
#include "../engine/scene/Scene.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/rhi/RHIDevice.h"
#include "../engine/renderer/Renderer3D.h"
#include "../engine/renderer/ImGuiNVRHI.h"
#include "../engine/core/Window.h"
#include "../engine/audio/AudioEngine.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/scripting/ScriptEngine.h"
#include "../engine/runtime/GameModule.h"
#include <imgui.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace Hargon
{
    class EditorCommandSystem;

    // Dockable ImGui window names (kept stable across layouts & presets)
    inline constexpr const char* kWinHierarchy    = "Scene Hierarchy";
    inline constexpr const char* kWinInspector    = "Inspector";
    inline constexpr const char* kWinViewport     = "3D Viewport";
    inline constexpr const char* kWinAssetBrowser = "Asset Browser";
    inline constexpr const char* kWinScriptEditor = "Script Editor";
    inline constexpr const char* kWinConsole      = "Console & Diagnostics";
    inline constexpr const char* kWinWorldEnv     = "World & Environment";
    inline constexpr const char* kWinSystems      = "Engine Systems";

    enum class WorkspacePreset
    {
        SceneEditing = 0,
        Scripting = 1,
        MaterialsLighting = 2
    };

    enum class PlayState
    {
        Edit = 0,
        Playing = 1,
        Paused = 2
    };

    enum class PendingActionType
    {
        None = 0,
        OpenProject,
        NewEmptyScene,
        ApplyTemplate,
        LoadScene,
        ReloadScene,
        ExitApp,
        SwitchCreateProject
    };

    struct PendingUnsavedAction
    {
        PendingActionType type = PendingActionType::None;
        std::string stringArg;
        int intArg = 0;
        GameProjectConfig projectConfigArg{};
    };

    struct ViewportToolSettings
    {
        GizmoMode gizmoMode = GizmoMode::Translate;
        bool localSpace = false;
        bool snapEnabled = false;
        float moveSnap = 0.5f;
        float rotateSnapDeg = 15.0f;
        float scaleSnap = 0.25f;
        bool drawGrid = true;
        bool drawColliders = false;
        bool showOverlay = true;
        bool showStats = true;
    };

    struct EditorSettings
    {
        float uiScale = 1.0f;
        bool autosaveEnabled = true;
        float autosaveIntervalSec = 120.0f;
        int maxBackups = 10;
        std::string defaultBackend = "Vulkan";
        bool openLastProjectOnStartup = true;
    };

    struct ComponentClipboard
    {
        enum class Type
        {
            None = 0,
            Transform,
            Mesh,
            Material,
            Light,
            Camera,
            RigidBody,
            Collider,
            RayCast,
            AudioSource,
            LuaScript,
            Particles,
            Animation,
            Gameplay
        };

        Type type = Type::None;
        TransformComponent transform;
        MeshComponent mesh;
        MaterialComponent material;
        LightComponent light;
        CameraComponent camera;
        RigidBodyComponent rigidBody;
        ColliderComponent collider;
        RayCastComponent raycast;
        AudioSourceComponent audioSource;
        LuaScriptComponent luaScript;
        ParticleEmitterComponent particleEmitter;
        AnimationComponent animation;
        GameplayComponent gameplay;
    };

    struct SceneEnvSnapshot
    {
        std::string sceneName;
        glm::vec3 skyZenithColor{ 0.03f, 0.06f, 0.16f };
        glm::vec3 skyHorizonColor{ 0.14f, 0.34f, 0.56f };
        glm::vec3 groundAmbientColor{ 0.08f, 0.10f, 0.15f };
        float ambientIntensity = 0.55f;
        float fogDensity = 0.0045f;
        float arenaHalfSize = 36.0f;
        WorldEnvironmentSettings worldEnv;
        std::string introBannerText;
        std::string hudHealthLabel;
        std::string hudShieldLabel;
        std::string hudObjectiveLabel;
        std::string hudCollectibleLabel;
        std::string hudControlsText;
        std::string victoryText;
        std::string defeatText;

        static SceneEnvSnapshot Capture(const Scene& s)
        {
            SceneEnvSnapshot snap;
            snap.sceneName = s.sceneName;
            snap.skyZenithColor = s.skyZenithColor;
            snap.skyHorizonColor = s.skyHorizonColor;
            snap.groundAmbientColor = s.groundAmbientColor;
            snap.ambientIntensity = s.ambientIntensity;
            snap.fogDensity = s.fogDensity;
            snap.arenaHalfSize = s.arenaHalfSize;
            snap.worldEnv = s.worldEnv;
            snap.introBannerText = s.introBannerText;
            snap.hudHealthLabel = s.hudHealthLabel;
            snap.hudShieldLabel = s.hudShieldLabel;
            snap.hudObjectiveLabel = s.hudObjectiveLabel;
            snap.hudCollectibleLabel = s.hudCollectibleLabel;
            snap.hudControlsText = s.hudControlsText;
            snap.victoryText = s.victoryText;
            snap.defeatText = s.defeatText;
            return snap;
        }

        void Apply(Scene& s) const
        {
            s.sceneName = sceneName;
            s.skyZenithColor = skyZenithColor;
            s.skyHorizonColor = skyHorizonColor;
            s.groundAmbientColor = groundAmbientColor;
            s.ambientIntensity = ambientIntensity;
            s.fogDensity = fogDensity;
            s.arenaHalfSize = arenaHalfSize;
            s.worldEnv = worldEnv;
            s.introBannerText = introBannerText;
            s.hudHealthLabel = hudHealthLabel;
            s.hudShieldLabel = hudShieldLabel;
            s.hudObjectiveLabel = hudObjectiveLabel;
            s.hudCollectibleLabel = hudCollectibleLabel;
            s.hudControlsText = hudControlsText;
            s.victoryText = victoryText;
            s.defeatText = defeatText;
        }
    };

    struct EditorContext
    {
        // Engine systems (non-owning pointers owned by EditorApp)
        Window* window = nullptr;
        RHIDevice* rhi = nullptr;
        AudioEngine* audio = nullptr;
        Renderer3D* renderer3D = nullptr;
        PhysicsSystem* physics = nullptr;
        ScriptEngine* scripts = nullptr;
        ImGuiNVRHI* imguiLayer = nullptr;
        EditorCommandSystem* commands = nullptr;

        // Active project & scene state
        GameProjectConfig* projectConfig = nullptr;
        Scene* scene = nullptr;
        std::unique_ptr<IGameModule>* gameModule = nullptr;
        std::string* currentScenePath = nullptr;
        PlayState* playState = nullptr;

        // Camera & viewport state
        CameraView* editorCamera = nullptr;
        float* editorCamYaw = nullptr;
        float* editorCamPitch = nullptr;
        float* smoothedFPS = nullptr;
        ViewportToolSettings viewportTools;
        EditorSettings settings;

        bool viewportFocused = false;
        bool viewportHovered = false;
        bool gameplayMouseLockActive = false;

        // Selection & multi-selection
        EntityID selectedEntity = kInvalidEntity;
        std::vector<EntityID> selectedEntities;
        EntityID renameRequestId = kInvalidEntity;

        // Panel visibility
        bool showHierarchy = true;
        bool showInspector = true;
        bool showViewport = true;
        bool showAssetBrowser = true;
        bool showScriptEditor = false;
        bool showConsole = true;
        bool showWorldEnv = true;
        bool showSystems = false;

        // Modal dialogs
        bool showProjectManagerModal = false;
        bool showExportModal = false;
        bool showSettingsModal = false;
        bool showRecoveryModal = false;
        std::string recoveryFilePath;
        PendingUnsavedAction pendingAction;
        bool showUnsavedPromptModal = false;

        // Clipboard & toast status
        ComponentClipboard clipboard;
        std::string statusToast = "Ready";
        float statusToastTimer = 0.0f;

        // Cross-panel actions wired by EditorApp
        std::function<bool()> saveScene;
        std::function<bool()> saveSceneAs;
        std::function<bool(const std::string&)> openProjectByPath;
        std::function<bool(const GameProjectConfig&)> openProjectConfig;
        std::function<void()> startPlayMode;
        std::function<void()> pausePlayMode;
        std::function<void()> stopPlayMode;
        std::function<void()> stepPlayMode;
        std::function<void(const std::string&, int)> openScriptTab;
        std::function<bool(const std::string&, const glm::vec3*)> instantiateAssetInScene;
        std::function<void(WorkspacePreset)> applyWorkspacePreset;
        std::function<void(const std::string&)> saveNamedLayout;
        std::function<bool(const std::string&)> loadNamedLayout;
        std::function<void()> resetDefaultLayout;
        std::function<void()> rebuildUIStyleAndSaveSettings;
        std::function<void(const PendingUnsavedAction&)> requestGuardedAction;
        std::function<void()> focusSelectedInViewport;
        std::function<bool()> hasAnyDirtyScriptTabs;
        std::function<bool()> saveAllDirtyScriptTabs;
        std::function<void()> discardDirtyScriptTabs;
        std::function<std::string()> getActiveSceneBackupDir;
        std::function<bool()> isSceneDirty;
        std::function<GameModuleContext()> buildModuleContext;

        void SetToast(const std::string& msg, float durationSec = 4.0f)
        {
            statusToast = msg;
            statusToastTimer = durationSec;
        }

        void SelectSingle(EntityID id)
        {
            selectedEntities.clear();
            if (id != kInvalidEntity && scene && scene->GetEntity(id))
            {
                selectedEntity = id;
                selectedEntities.push_back(id);
            }
            else
            {
                selectedEntity = kInvalidEntity;
            }
        }

        void ToggleSelect(EntityID id)
        {
            if (id == kInvalidEntity || !scene || !scene->GetEntity(id))
                return;
            auto it = std::find(selectedEntities.begin(), selectedEntities.end(), id);
            if (it != selectedEntities.end())
            {
                selectedEntities.erase(it);
                if (selectedEntity == id)
                    selectedEntity = selectedEntities.empty() ? kInvalidEntity : selectedEntities.back();
            }
            else
            {
                selectedEntities.push_back(id);
                selectedEntity = id;
            }
        }

        void AddToSelection(EntityID id)
        {
            if (id == kInvalidEntity || !scene || !scene->GetEntity(id))
                return;
            if (std::find(selectedEntities.begin(), selectedEntities.end(), id) == selectedEntities.end())
                selectedEntities.push_back(id);
            selectedEntity = id;
        }

        void ClearSelection()
        {
            selectedEntity = kInvalidEntity;
            selectedEntities.clear();
        }

        bool IsSelected(EntityID id) const
        {
            if (id == kInvalidEntity) return false;
            return selectedEntity == id ||
                   std::find(selectedEntities.begin(), selectedEntities.end(), id) != selectedEntities.end();
        }

        void PruneInvalidSelection()
        {
            if (!scene)
            {
                ClearSelection();
                return;
            }
            selectedEntities.erase(
                std::remove_if(selectedEntities.begin(), selectedEntities.end(), [&](EntityID id) {
                    const Entity* e = scene->GetEntity(id);
                    return !e || !e->alive;
                }),
                selectedEntities.end());
            if (selectedEntity != kInvalidEntity)
            {
                const Entity* e = scene->GetEntity(selectedEntity);
                if (!e || !e->alive)
                    selectedEntity = selectedEntities.empty() ? kInvalidEntity : selectedEntities.back();
                else if (std::find(selectedEntities.begin(), selectedEntities.end(), selectedEntity) == selectedEntities.end())
                    selectedEntities.insert(selectedEntities.begin(), selectedEntity);
            }
            else if (!selectedEntities.empty())
            {
                selectedEntity = selectedEntities.back();
            }
        }

        Scene::BoundsLookupFn MakeBoundsLookup() const
        {
            return [this](const MeshComponent& mc, glm::vec3& outMin, glm::vec3& outMax) {
                return renderer3D ? renderer3D->TryGetMeshLocalBounds(mc, outMin, outMax) : false;
            };
        }
    };
}

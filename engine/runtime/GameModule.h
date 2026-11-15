#pragma once
#include "../scene/Scene.h"
#include "../scene/SceneSerializer.h"
#include "../physics/PhysicsSystem.h"
#include "../scripting/ScriptEngine.h"
#include "../audio/AudioEngine.h"
#include "../renderer/Renderer3D.h"
#include "../rhi/RHIDevice.h"
#include <imgui.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hargon
{
    class Window;

    struct GameModuleContext
    {
        Scene* scene = nullptr;
        PhysicsSystem* physics = nullptr;
        ScriptEngine* scripts = nullptr;
        AudioEngine* audio = nullptr;
        Renderer3D* renderer = nullptr;
        Window* window = nullptr;
        GameProjectConfig* projectConfig = nullptr;
    };

    class IGameModule
    {
    public:
        virtual ~IGameModule() = default;

        virtual std::string GetModuleId() const = 0;
        virtual std::string GetDisplayName() const = 0;

        virtual bool OnProjectOpen(const GameModuleContext& ctx, bool isPlayMode, CameraView& ioCamera) = 0;
        virtual void OnEnterPlayMode(const GameModuleContext& ctx, bool fromEditMode, CameraView& ioCamera) = 0;
        virtual void OnStopPlayMode(const GameModuleContext& ctx) = 0;

        virtual void OnStepPlay(const GameModuleContext& ctx, float dt, CameraView& ioCamera) = 0;
        virtual void OnStepEditorPreview(const GameModuleContext& ctx, float dt) = 0;
        virtual void OnRenderHUD(const GameModuleContext& ctx, const ImVec2& vpMin, const ImVec2& vpSize, ImDrawList* dl) = 0;

        virtual void DrawEditorMenu(const GameModuleContext& ctx, CameraView& ioEditorCamera, float& ioYaw, float& ioPitch) {}
        virtual void RunAutoTestStep(const GameModuleContext& ctx, RHIDevice& rhi, int frameCount, bool& outSaveAndReloadCheckDone) {}

        virtual bool HandlesEditorAnimationPreview() const { return true; }
        virtual bool ShouldDrawEditorGrid() const { return true; }
        virtual bool HasPreferredEditorCameraPose(float& outYaw, float& outPitch) const { return false; }
        virtual EntityID PickDefaultSelectedEntity(const GameModuleContext& ctx, const CameraView& cam) const { return kInvalidEntity; }
    };

    class GameModuleRegistry
    {
    public:
        using FactoryFn = std::function<std::unique_ptr<IGameModule>()>;

        static GameModuleRegistry& Get();

        void RegisterModule(const std::string& moduleId, const std::string& displayName, FactoryFn factory);
        std::unique_ptr<IGameModule> CreateModule(const std::string& moduleId) const;
        std::vector<std::string> GetRegisteredModuleIds() const;
        std::string GetDisplayName(const std::string& moduleId) const;

    private:
        GameModuleRegistry();

        struct Entry
        {
            std::string displayName;
            FactoryFn factory;
        };
        std::unordered_map<std::string, Entry> m_Factories;
    };

    template <typename TModule>
    struct AutoGameModuleRegistrar
    {
        AutoGameModuleRegistrar(const std::string& id, const std::string& displayName)
        {
            GameModuleRegistry::Get().RegisterModule(id, displayName, []() {
                return std::make_unique<TModule>();
            });
        }
    };

#define HARGON_REGISTER_GAME_MODULE(ModuleId, DisplayName, ClassName) \
    static ::Hargon::AutoGameModuleRegistrar<ClassName> s_HargonReg_##ClassName(ModuleId, DisplayName)
}

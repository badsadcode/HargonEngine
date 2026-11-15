#include "GameModule.h"
#include "GameSystem.h"
#include "../project/ProjectManager.h"
#include "../project/SceneTemplates.h"
#include "../world/BadLevelImporter.h"
#include "../animation/TweenSystem.h"
#include "../core/Window.h"
#include "../core/Log.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <filesystem>

namespace Hargon
{
    class SceneAndLuaGameModule final : public IGameModule
    {
    public:
        std::string GetModuleId() const override { return "SceneAndLua"; }
        std::string GetDisplayName() const override { return "Scene + Lua Scripting + Physics (General Purpose)"; }

        bool OnProjectOpen(const GameModuleContext& ctx, bool isPlayMode, CameraView& ioCamera) override
        {
            if (!ctx.scene || !ctx.projectConfig)
                return false;

            if (ctx.renderer)
                ctx.renderer->ClearCustomMeshes();

            if (ctx.scripts)
                ctx.scripts->SetProjectRootDir(ctx.projectConfig->projectRootDir);

            const std::string& startup = ctx.projectConfig->startupScene;
            std::string resolvedStartup = ProjectManager::ResolveProjectAssetPath(*ctx.projectConfig, startup);

            bool loaded = false;
            std::error_code ec;
            if (!resolvedStartup.empty() && std::filesystem::exists(resolvedStartup, ec))
            {
                loaded = SceneSerializer::LoadFromFile(*ctx.scene, resolvedStartup);
                if (!loaded)
                {
                    Log::Error("Project startup scene exists but failed to load: " + resolvedStartup);
                    return false;
                }
            }
            else if (startup.empty())
            {
                SceneTemplates::BuildEmptyTemplateScene(*ctx.scene, ctx.projectConfig->gameTitle);
                loaded = true;
            }
            else
            {
                SceneTemplates::BuildEmptyTemplateScene(*ctx.scene, ctx.projectConfig->gameTitle);
                std::filesystem::path saveTarget = std::filesystem::path(startup);
                if (!ctx.projectConfig->projectRootDir.empty() && saveTarget.is_relative()
                    && startup.rfind("projects/", 0) != 0 && startup.rfind("projects\\", 0) != 0)
                {
                    saveTarget = std::filesystem::path(ctx.projectConfig->projectRootDir) / saveTarget;
                }
                SceneSerializer::SaveToFile(*ctx.scene, saveTarget.string());
                loaded = true;
            }

            if (ctx.scripts)
                ctx.scripts->ResetForNewProject(*ctx.scene);

            m_HasSnapshot = false;
            m_SavedEditSnapshot.clear();

            if (isPlayMode)
            {
                GameSystem::UpdateGameCamera(*ctx.scene, ioCamera, 0.0f);
            }
            else
            {
                ioCamera.position = glm::vec3(0.0f, 12.5f, -26.5f);
            }
            return loaded;
        }

        void OnEnterPlayMode(const GameModuleContext& ctx, bool fromEditMode, CameraView& ioCamera) override
        {
            if (!ctx.scene)
                return;
            if (fromEditMode)
            {
                m_SavedEditSnapshot = SceneSerializer::SerializeToString(*ctx.scene);
                m_HasSnapshot = true;
                if (ctx.scripts)
                    ctx.scripts->ReloadAllScripts(*ctx.scene);
                GameSystem::UpdateGameCamera(*ctx.scene, ioCamera, 0.0f);
            }
        }

        void OnStopPlayMode(const GameModuleContext& ctx) override
        {
            if (ctx.scene && m_HasSnapshot && !m_SavedEditSnapshot.empty())
            {
                SceneSerializer::DeserializeFromString(*ctx.scene, m_SavedEditSnapshot);
            }
            m_HasSnapshot = false;
            m_SavedEditSnapshot.clear();
        }

        void OnStepPlay(const GameModuleContext& ctx, float dt, CameraView& ioCamera) override
        {
            if (!ctx.scene || !ctx.physics || !ctx.scripts || !ctx.audio || !ctx.renderer)
                return;

            GameSystem::StepGameplay(*ctx.scene, *ctx.physics, *ctx.scripts, *ctx.audio, *ctx.renderer, ctx.window, dt);
            if (ctx.scene->restartRequested)
            {
                ctx.scene->restartRequested = false;
                std::string resolvedStartup = ctx.projectConfig
                    ? ProjectManager::ResolveProjectAssetPath(*ctx.projectConfig, ctx.projectConfig->startupScene)
                    : "";
                if (!resolvedStartup.empty() && std::filesystem::exists(resolvedStartup))
                {
                    SceneSerializer::LoadFromFile(*ctx.scene, resolvedStartup);
                }
                else
                {
                    SceneTemplates::BuildEmptyTemplateScene(*ctx.scene, ctx.projectConfig ? ctx.projectConfig->gameTitle : "Scene");
                }
                ctx.scripts->ResetForNewProject(*ctx.scene);
            }

            GameSystem::UpdateGameCamera(*ctx.scene, ioCamera, dt);
            TweenSystem::Get().Step(*ctx.scene, dt);
        }

        void OnStepEditorPreview(const GameModuleContext& ctx, float dt) override
        {
            if (ctx.scene && ctx.renderer)
                GameSystem::StepAnimationsAndParticles(*ctx.scene, *ctx.renderer, dt);
        }

        void OnRenderHUD(const GameModuleContext& ctx, const ImVec2& vpMin, const ImVec2& vpSize, ImDrawList* dl) override
        {
            if (ctx.scene)
                GameSystem::RenderHUD(*ctx.scene, vpMin, vpSize, dl);
        }

        void DrawEditorMenu(const GameModuleContext& ctx, CameraView&, float&, float&) override
        {
            if (!ctx.projectConfig || !ctx.projectConfig->enableBadLevel || !ctx.scene)
                return;

            if (ImGui::BeginMenu("BadLevel World Tools"))
            {
                if (ImGui::MenuItem("Import Procedural BadLevel Dungeon (c1m1.json)"))
                {
                    std::string status;
                    std::string lvlPath = ProjectManager::ResolveProjectAssetPath(*ctx.projectConfig, "assets/levels/c1m1.json");
                    if (!BadLevelImporter::ImportJsonToScene(lvlPath, *ctx.scene, 0.045f, 400, &status))
                        Log::Error(status);
                }
                if (ImGui::MenuItem("Generate Fresh BadLevel Map via Python (Seed 777)"))
                {
                    std::string outPath = ctx.projectConfig->projectRootDir.empty()
                        ? "assets/levels/custom_seed.json"
                        : (ctx.projectConfig->projectRootDir + "/levels/custom_seed.json");
                    std::string status;
                    if (BadLevelImporter::GenerateMapJson(outPath, 777, "classic", "gothic", &status))
                        BadLevelImporter::ImportJsonToScene(outPath, *ctx.scene, 0.045f, 400, &status);
                }
                ImGui::EndMenu();
            }
        }

        void RunAutoTestStep(const GameModuleContext& ctx, RHIDevice& rhi, int frameCount, bool&) override
        {
            if (!ctx.window || !ctx.projectConfig)
                return;
            if (frameCount >= 5 && frameCount <= 55)
            {
                ctx.window->SetSimulatedKey(GLFW_KEY_W, true);
                ctx.window->SetSimulatedKey(GLFW_KEY_SPACE, (frameCount % 8) == 0);
            }
            if (frameCount == 45)
            {
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_runtime_project_" + ctx.projectConfig->projectName + ".bmp");
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_exported_game.bmp");
            }
            if (frameCount >= 65)
            {
                ctx.window->RequestClose();
            }
        }

        EntityID PickDefaultSelectedEntity(const GameModuleContext& ctx, const CameraView&) const override
        {
            if (!ctx.scene)
                return kInvalidEntity;
            if (auto cores = ctx.scene->FindEntitiesByTag("Core"); !cores.empty())
                return cores[0];
            if (auto players = ctx.scene->FindEntitiesByTag("Player"); !players.empty())
                return players[0];
            return ctx.scene->GetEntities().empty() ? kInvalidEntity : ctx.scene->GetEntities().front().id;
        }

    private:
        std::string m_SavedEditSnapshot;
        bool m_HasSnapshot = false;
    };

    GameModuleRegistry& GameModuleRegistry::Get()
    {
        static GameModuleRegistry s_Instance;
        return s_Instance;
    }

    GameModuleRegistry::GameModuleRegistry()
    {
        RegisterModule("SceneAndLua", "Scene + Lua Scripting + Physics", []() {
            return std::make_unique<SceneAndLuaGameModule>();
        });
    }

    void GameModuleRegistry::RegisterModule(const std::string& moduleId, const std::string& displayName, FactoryFn factory)
    {
        m_Factories[moduleId] = Entry{ displayName, std::move(factory) };
    }

    std::unique_ptr<IGameModule> GameModuleRegistry::CreateModule(const std::string& moduleId) const
    {
        auto it = m_Factories.find(moduleId);
        if (it != m_Factories.end() && it->second.factory)
            return it->second.factory();

        auto fallback = m_Factories.find("SceneAndLua");
        if (fallback != m_Factories.end() && fallback->second.factory)
        {
            if (!moduleId.empty() && moduleId != "SceneAndLua")
                Log::Info("Module '" + moduleId + "' not registered in this binary; falling back to 'SceneAndLua'.");
            return fallback->second.factory();
        }
        return nullptr;
    }

    std::vector<std::string> GameModuleRegistry::GetRegisteredModuleIds() const
    {
        std::vector<std::string> ids;
        ids.reserve(m_Factories.size());
        for (const auto& [id, _] : m_Factories)
            ids.push_back(id);
        return ids;
    }

    std::string GameModuleRegistry::GetDisplayName(const std::string& moduleId) const
    {
        auto it = m_Factories.find(moduleId);
        return (it != m_Factories.end()) ? it->second.displayName : moduleId;
    }
}

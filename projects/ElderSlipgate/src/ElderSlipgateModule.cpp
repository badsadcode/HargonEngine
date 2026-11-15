#include "engine/runtime/GameModule.h"
#include "engine/animation/TweenSystem.h"
#include "engine/core/Window.h"
#include "FPSGameManager.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace Hargon
{
    class ElderSlipgateGameModule final : public IGameModule
    {
    public:
        std::string GetModuleId() const override { return "ElderSlipgateFPS"; }
        std::string GetDisplayName() const override { return "Elder Slipgate Single-Player Retro FPS (BadLevel)"; }

        bool OnProjectOpen(const GameModuleContext& ctx, bool isPlayMode, CameraView& ioCamera) override
        {
            if (!ctx.scene || !ctx.renderer || !ctx.audio || !ctx.window)
                return false;

            if (ctx.projectConfig)
                m_FPSGame.SetProjectRootDir(ctx.projectConfig->projectRootDir);
            else
                m_FPSGame.SetProjectRootDir("");

            m_FPSGame.SetHostManagesCursor(true);
            if (!m_FPSGame.Init(ctx.scene, ctx.renderer, ctx.audio, ctx.window, isPlayMode))
                return false;

            if (!isPlayMode)
            {
                m_FPSGame.SetMenuState(FPSMenuState::None);
                m_FPSGame.AimAtNearestVisibleEnemy();
            }
            m_FPSGame.UpdateFirstPersonCamera(ioCamera, 0.0f);
            return true;
        }

        void OnEnterPlayMode(const GameModuleContext& ctx, bool fromEditMode, CameraView& ioCamera) override
        {
            (void)ctx;
            (void)fromEditMode;
            m_FPSGame.SetMenuState(FPSMenuState::None);
            m_FPSGame.UpdateFirstPersonCamera(ioCamera, 0.0f);
        }

        void OnStopPlayMode(const GameModuleContext& ctx) override
        {
            (void)ctx;
            m_FPSGame.LoadCampaignLevel(m_FPSGame.GetCurrentCampaign(), m_FPSGame.GetCurrentLevelIndex(), true);
            m_FPSGame.SetMenuState(FPSMenuState::None);
        }

        void OnStepPlay(const GameModuleContext& ctx, float dt, CameraView& ioCamera) override
        {
            (void)ctx;
            m_FPSGame.Step(dt);
            m_FPSGame.UpdateFirstPersonCamera(ioCamera, dt);
        }

        void OnStepEditorPreview(const GameModuleContext& ctx, float dt) override
        {
            (void)ctx;
            (void)dt;
        }

        void OnRenderHUD(const GameModuleContext& ctx, const ImVec2& vpMin, const ImVec2& vpSize, ImDrawList* dl) override
        {
            (void)ctx;
            m_FPSGame.RenderUI(vpMin, vpSize, dl);
        }

        bool HandlesEditorAnimationPreview() const override { return false; }
        bool ShouldDrawEditorGrid() const override { return false; }
        bool WantsCursorCapture() const override { return m_FPSGame.WantsCursorCapture(); }

        bool HasPreferredEditorCameraPose(float& outYaw, float& outPitch) const override
        {
            outYaw = m_FPSGame.GetPlayerYaw();
            outPitch = m_FPSGame.GetPlayerPitch();
            return true;
        }

        EntityID PickDefaultSelectedEntity(const GameModuleContext& ctx, const CameraView& cam) const override
        {
            if (!ctx.scene)
                return kInvalidEntity;
            EntityID selected = kInvalidEntity;
            float bestDist = 999999.0f;
            for (const auto& e : ctx.scene->GetEntities())
            {
                if (e.alive && e.tag.tag == "Enemy")
                {
                    glm::vec3 toEnt = e.transform.position - cam.position;
                    float d = glm::length(toEnt);
                    if (d > 1.0f && glm::dot(glm::normalize(toEnt), cam.forward) > 0.7f && d < bestDist)
                    {
                        bestDist = d;
                        selected = e.id;
                    }
                }
            }
            return selected;
        }

        void DrawEditorMenu(const GameModuleContext& ctx, CameraView& ioEditorCamera, float& ioYaw, float& ioPitch) override
        {
            (void)ctx;
            if (ImGui::BeginMenu("BadLevel FPS Campaigns (10 Maps)"))
            {
                ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.0f), "Campaign 1: The Elder Ward");
                for (int m = 1; m <= 5; ++m)
                {
                    std::string lbl = "Load C1M" + std::to_string(m);
                    if (ImGui::MenuItem(lbl.c_str()))
                    {
                        m_FPSGame.LoadCampaignLevel(1, m, true);
                        m_FPSGame.SetMenuState(FPSMenuState::None);
                        m_FPSGame.UpdateFirstPersonCamera(ioEditorCamera, 0.0f);
                        ioYaw = m_FPSGame.GetPlayerYaw();
                        ioPitch = m_FPSGame.GetPlayerPitch();
                    }
                }
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Campaign 2: The Iron Citadel");
                for (int m = 1; m <= 5; ++m)
                {
                    std::string lbl = "Load C2M" + std::to_string(m);
                    if (ImGui::MenuItem(lbl.c_str()))
                    {
                        m_FPSGame.LoadCampaignLevel(2, m, true);
                        m_FPSGame.SetMenuState(FPSMenuState::None);
                        m_FPSGame.UpdateFirstPersonCamera(ioEditorCamera, 0.0f);
                        ioYaw = m_FPSGame.GetPlayerYaw();
                        ioPitch = m_FPSGame.GetPlayerPitch();
                    }
                }
                ImGui::EndMenu();
            }
        }

        void RunAutoTestStep(const GameModuleContext& ctx, RHIDevice& rhi, int frameCount, bool&) override
        {
            if (!ctx.window)
                return;

            if (frameCount == 5)
            {
                m_FPSGame.SetMenuState(FPSMenuState::MainMenu);
            }
            else if (frameCount == 20)
            {
                m_FPSGame.SetMenuState(FPSMenuState::None);
                m_FPSGame.UnlockAllWeaponsForShowcase();
                m_FPSGame.SelectWeapon(1);
            }
            else if (frameCount >= 22 && frameCount <= 80)
            {
                m_FPSGame.AimAtNearestVisibleEnemy();
                if (frameCount == 42)
                    m_FPSGame.SelectWeapon(3);
                ctx.window->SetSimulatedKey(GLFW_KEY_F, (frameCount % 10) == 2);
                ctx.window->SetSimulatedKey(GLFW_KEY_W, (frameCount % 20) < 6);
            }
            else if (frameCount == 82)
            {
                std::string saveMsg, loadMsg;
                m_FPSGame.SaveGame(1, &saveMsg);
                m_FPSGame.LoadGame(1, &loadMsg);
            }
            else if (frameCount == 88)
            {
                m_FPSGame.LoadCampaignLevel(2, 2, false);
                m_FPSGame.SetMenuState(FPSMenuState::None);
                m_FPSGame.UnlockAllWeaponsForShowcase();
                m_FPSGame.SelectWeapon(2);
            }
            else if (frameCount >= 90 && frameCount <= 145)
            {
                m_FPSGame.AimAtNearestVisibleEnemy();
                if (frameCount == 104)
                    m_FPSGame.SelectWeapon(4);
                ctx.window->SetSimulatedKey(GLFW_KEY_F, true);
                ctx.window->SetSimulatedKey(GLFW_KEY_W, false);
            }
            else if (frameCount >= 152)
            {
                ctx.window->RequestClose();
            }

            if (frameCount == 12)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_menu_settings.bmp");
            else if (frameCount == 46)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign1_combat.bmp");
            else if (frameCount == 124)
                rhi.SaveTextureToBMP(rhi.GetCurrentBackbuffer(), "screenshot_fps_campaign2_combat.bmp");
        }

    private:
        FPSGameManager m_FPSGame;
    };

    HARGON_REGISTER_GAME_MODULE("ElderSlipgateFPS", "Elder Slipgate Single-Player Retro FPS (BadLevel)", ElderSlipgateGameModule);
}

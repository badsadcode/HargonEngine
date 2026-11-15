#include "ProjectManagerWindow.h"
#include "../../engine/project/ProjectManager.h"
#include "../../engine/runtime/GameSystem.h"
#include "../../engine/scene/SceneSerializer.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace Hargon
{
    void ProjectManagerWindow::RefreshProjectList()
    {
        m_DiscoveredProjects = ProjectManager::DiscoverProjects("projects");
        m_ListLoaded = true;
    }

    void ProjectManagerWindow::DrawModals(EditorContext& ctx, const std::function<void(const PendingUnsavedAction&)>& executeAction)
    {
        if (!m_ListLoaded)
            RefreshProjectList();

        DrawProjectManagerModal(ctx);
        DrawExportModal(ctx);
        DrawUnsavedConfirmModal(ctx, executeAction);
        DrawRecoveryModal(ctx);
    }

    void ProjectManagerWindow::DrawProjectManagerModal(EditorContext& ctx)
    {
        if (ctx.showProjectManagerModal)
        {
            RefreshProjectList();
            m_ProjectStatusMsg.clear();
            m_ProjectStatusError = false;
            ImGui::OpenPopup("Project Manager");
            ctx.showProjectManagerModal = false;
        }

        if (ImGui::BeginPopupModal("Project Manager", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Hargon Project Browser & Generator");
            ImGui::Separator();

            if (ImGui::BeginTabBar("ProjectMgrTabs"))
            {
                if (ImGui::BeginTabItem("Open Existing Project"))
                {
                    ImGui::BeginChild("ExistingProjList", ImVec2(560.0f, 240.0f), true);
                    for (const auto& cfg : m_DiscoveredProjects)
                    {
                        bool isCurrent = (ctx.projectConfig && ctx.projectConfig->projectRootDir == cfg.projectRootDir);
                        ImGui::PushID(cfg.projectRootDir.c_str());
                        ImGui::TextColored(isCurrent ? ImVec4(0.45f, 0.95f, 0.62f, 1.0f) : ImVec4(0.92f, 0.95f, 1.0f, 1.0f),
                            "%s  (%s)%s", cfg.gameTitle.c_str(), cfg.genre.c_str(), isCurrent ? "  [Active]" : "");
                        ImGui::TextDisabled("%s | Startup: %s", cfg.projectRootDir.c_str(), cfg.startupScene.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("Open"))
                        {
                            if (ctx.requestGuardedAction)
                            {
                                PendingUnsavedAction act;
                                act.type = PendingActionType::OpenProject;
                                act.projectConfigArg = cfg;
                                act.stringArg = (fs::path(cfg.projectRootDir) / "project.hproject").generic_string();
                                ctx.requestGuardedAction(act);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    ImGui::EndChild();

                    if (ImGui::Button("Browse .hproject File..."))
                    {
                        std::string picked;
                        if (ProjectManager::BrowseForProjectFile(picked))
                        {
                            if (ctx.requestGuardedAction)
                            {
                                PendingUnsavedAction act;
                                act.type = PendingActionType::OpenProject;
                                act.stringArg = picked;
                                ctx.requestGuardedAction(act);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Create New Project"))
                {
                    ImGui::InputText("Folder / Project ID", m_SlugBuf, sizeof(m_SlugBuf));
                    ImGui::InputText("Game Title", m_TitleBuf, sizeof(m_TitleBuf));
                    ImGui::InputText("Genre", m_GenreBuf, sizeof(m_GenreBuf));
                    const char* templates[] = {
                        "0: Empty 3D Scene",
                        "1: 3D Floating-Island Action-Platformer",
                        "2: Top-Down Wave Defense Arena",
                        "3: Procedural Dungeon + BadLevel Starter"
                    };
                    ImGui::Combo("Template", &m_TemplateIdx, templates, IM_ARRAYSIZE(templates));
                    ImGui::Checkbox("Enable BadLevel Procedural Generator", &m_EnableBadLevel);

                    if (ImGui::Button("Create & Open Project", ImVec2(200, 0)))
                    {
                        GameProjectConfig newCfg;
                        std::string status;
                        if (ProjectManager::CreateNewProject(m_SlugBuf, m_TitleBuf, m_GenreBuf, m_TemplateIdx, m_EnableBadLevel, newCfg, &status))
                        {
                            m_ProjectStatusMsg = status;
                            m_ProjectStatusError = false;
                            RefreshProjectList();
                            if (ctx.requestGuardedAction)
                            {
                                PendingUnsavedAction act;
                                act.type = PendingActionType::OpenProject;
                                act.projectConfigArg = newCfg;
                                act.stringArg = (fs::path(newCfg.projectRootDir) / "project.hproject").generic_string();
                                ctx.requestGuardedAction(act);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        else
                        {
                            m_ProjectStatusMsg = status;
                            m_ProjectStatusError = true;
                        }
                    }

                    if (!m_ProjectStatusMsg.empty())
                    {
                        ImGui::TextColored(m_ProjectStatusError ? ImVec4(1.0f, 0.42f, 0.40f, 1.0f) : ImVec4(0.42f, 0.95f, 0.55f, 1.0f),
                            "%s", m_ProjectStatusMsg.c_str());
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(110, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ProjectManagerWindow::DrawExportModal(EditorContext& ctx)
    {
        if (ctx.showExportModal)
        {
            if (ctx.projectConfig)
            {
                std::string slug = fs::path(ctx.projectConfig->projectRootDir).filename().string();
                if (slug.empty()) slug = "ExportedGame";
                if (m_LastExportProjectSlug != slug || m_ExportDirBuf[0] == '\0')
                {
                    m_LastExportProjectSlug = slug;
                    std::snprintf(m_ExportDirBuf, sizeof(m_ExportDirBuf), "Exports/%s", slug.c_str());
                }
            }
            m_ExportStatusMsg.clear();
            ImGui::OpenPopup("Export Standalone Game");
            ctx.showExportModal = false;
        }

        if (ImGui::BeginPopupModal("Export Standalone Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Package Standalone Release Build");
            if (ctx.projectConfig)
                ImGui::Text("Project: %s (%s)", ctx.projectConfig->gameTitle.c_str(), ctx.projectConfig->projectRootDir.c_str());
            ImGui::Separator();

            ImGui::SetNextItemWidth(380.0f);
            ImGui::InputText("Export Folder", m_ExportDirBuf, sizeof(m_ExportDirBuf));
            ImGui::Checkbox("Copy Runtime Executable & Shaders", &m_PackageRuntime);

            if (ImGui::Button("Build Export Package", ImVec2(190, 0)))
            {
                if (ctx.scene && ctx.projectConfig)
                {
                    GameProjectConfig exportCfg = *ctx.projectConfig;
                    std::string statusOut;
                    m_ExportSuccess = GameSystem::ExportStandaloneGame(*ctx.scene, exportCfg, m_ExportDirBuf, statusOut);
                    m_ExportStatusMsg = statusOut;
                    if (m_ExportSuccess)
                        ctx.SetToast("Export succeeded: " + std::string(m_ExportDirBuf));
                    else
                        ctx.SetToast("Export failed");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();

            if (!m_ExportStatusMsg.empty())
            {
                ImGui::Separator();
                ImGui::TextColored(m_ExportSuccess ? ImVec4(0.42f, 0.95f, 0.55f, 1.0f) : ImVec4(1.0f, 0.42f, 0.40f, 1.0f),
                    "%s", m_ExportStatusMsg.c_str());
            }

            ImGui::EndPopup();
        }
    }

    void ProjectManagerWindow::DrawUnsavedConfirmModal(EditorContext& ctx, const std::function<void(const PendingUnsavedAction&)>& executeAction)
    {
        if (ctx.showUnsavedPromptModal)
        {
            ImGui::OpenPopup("Unsaved Changes");
            ctx.showUnsavedPromptModal = false;
        }

        if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            bool sceneDirty = ctx.isSceneDirty && ctx.isSceneDirty();
            bool scriptDirty = ctx.hasAnyDirtyScriptTabs && ctx.hasAnyDirtyScriptTabs();

            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.28f, 1.0f), "You have unsaved changes:");
            if (sceneDirty)
                ImGui::BulletText("Current scene has unsaved modifications");
            if (scriptDirty)
                ImGui::BulletText("One or more Lua script tabs have unsaved edits");
            ImGui::Spacing();
            ImGui::TextUnformatted("Save before continuing?");
            ImGui::Separator();

            if (ImGui::Button("Save & Continue", ImVec2(140, 0)))
            {
                bool ok = true;
                if (sceneDirty && ctx.saveScene)
                    ok &= ctx.saveScene();
                if (scriptDirty && ctx.saveAllDirtyScriptTabs)
                    ok &= ctx.saveAllDirtyScriptTabs();
                if (ok)
                {
                    PendingUnsavedAction act = ctx.pendingAction;
                    ctx.pendingAction = PendingUnsavedAction{};
                    ImGui::CloseCurrentPopup();
                    if (executeAction) executeAction(act);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard & Continue", ImVec2(145, 0)))
            {
                if (ctx.discardDirtyScriptTabs)
                    ctx.discardDirtyScriptTabs();
                PendingUnsavedAction act = ctx.pendingAction;
                ctx.pendingAction = PendingUnsavedAction{};
                ImGui::CloseCurrentPopup();
                if (executeAction) executeAction(act);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(95, 0)))
            {
                ctx.pendingAction = PendingUnsavedAction{};
                if (ctx.window)
                    ctx.window->ClearCloseRequest();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void ProjectManagerWindow::DrawRecoveryModal(EditorContext& ctx)
    {
        if (ctx.showRecoveryModal)
        {
            ImGui::OpenPopup("Recover Unsaved Session?");
            ctx.showRecoveryModal = false;
        }

        if (ImGui::BeginPopupModal("Recover Unsaved Session?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.30f, 1.0f), "Hargon detected an autosaved backup from an unclean shutdown.");
            ImGui::TextDisabled("Backup: %s", ctx.recoveryFilePath.c_str());
            ImGui::Separator();

            if (ImGui::Button("Recover Autosave", ImVec2(160, 0)))
            {
                if (ctx.scene && !ctx.recoveryFilePath.empty() && fs::exists(ctx.recoveryFilePath))
                {
                    if (SceneSerializer::LoadFromFile(*ctx.scene, ctx.recoveryFilePath))
                    {
                        if (ctx.scripts)
                            ctx.scripts->ResetForNewProject(*ctx.scene);
                        ctx.ClearSelection();
                        ctx.SetToast("Recovered autosaved scene");
                    }
                }
                std::error_code ec;
                fs::remove(ctx.recoveryFilePath, ec);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard Recovery", ImVec2(150, 0)))
            {
                std::error_code ec;
                fs::remove(ctx.recoveryFilePath, ec);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

#include "EditorWorkspace.h"
#include "commands/EditorCommandSystem.h"
#include "../engine/scene/SceneSerializer.h"
#include "../engine/core/Log.h"
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace Hargon
{
    namespace
    {
        void ApplyHargonPalette(ImGuiStyle& style)
        {
            style.WindowRounding = 5.0f;
            style.ChildRounding = 4.0f;
            style.FrameRounding = 4.0f;
            style.PopupRounding = 5.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 5.0f;

            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;
            style.TabBorderSize = 0.0f;

            style.WindowPadding = ImVec2(9.0f, 8.0f);
            style.FramePadding = ImVec2(7.0f, 4.0f);
            style.ItemSpacing = ImVec2(7.0f, 5.0f);
            style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
            style.IndentSpacing = 16.0f;
            style.ScrollbarSize = 13.0f;
            style.GrabMinSize = 10.0f;

            ImVec4* c = style.Colors;
            c[ImGuiCol_Text]                 = ImVec4(0.91f, 0.94f, 0.98f, 1.00f);
            c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.53f, 0.60f, 1.00f);
            c[ImGuiCol_WindowBg]             = ImVec4(0.085f, 0.098f, 0.125f, 1.00f);
            c[ImGuiCol_ChildBg]              = ImVec4(0.070f, 0.082f, 0.105f, 1.00f);
            c[ImGuiCol_PopupBg]              = ImVec4(0.095f, 0.110f, 0.145f, 0.98f);
            c[ImGuiCol_Border]               = ImVec4(0.20f, 0.24f, 0.31f, 0.85f);
            c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

            c[ImGuiCol_FrameBg]              = ImVec4(0.135f, 0.155f, 0.200f, 1.00f);
            c[ImGuiCol_FrameBgHovered]       = ImVec4(0.185f, 0.215f, 0.280f, 1.00f);
            c[ImGuiCol_FrameBgActive]        = ImVec4(0.220f, 0.270f, 0.360f, 1.00f);

            c[ImGuiCol_TitleBg]              = ImVec4(0.065f, 0.075f, 0.098f, 1.00f);
            c[ImGuiCol_TitleBgActive]        = ImVec4(0.095f, 0.115f, 0.155f, 1.00f);
            c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.055f, 0.065f, 0.085f, 1.00f);

            c[ImGuiCol_MenuBarBg]            = ImVec4(0.072f, 0.084f, 0.110f, 1.00f);
            c[ImGuiCol_ScrollbarBg]          = ImVec4(0.060f, 0.070f, 0.090f, 1.00f);
            c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.22f, 0.27f, 0.34f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.29f, 0.36f, 0.45f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.22f, 0.68f, 0.90f, 1.00f);

            c[ImGuiCol_CheckMark]            = ImVec4(0.24f, 0.78f, 0.98f, 1.00f);
            c[ImGuiCol_SliderGrab]           = ImVec4(0.22f, 0.68f, 0.92f, 1.00f);
            c[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.84f, 1.00f, 1.00f);

            c[ImGuiCol_Button]               = ImVec4(0.155f, 0.185f, 0.245f, 1.00f);
            c[ImGuiCol_ButtonHovered]        = ImVec4(0.21f, 0.26f, 0.35f, 1.00f);
            c[ImGuiCol_ButtonActive]         = ImVec4(0.18f, 0.56f, 0.82f, 1.00f);

            c[ImGuiCol_Header]               = ImVec4(0.16f, 0.28f, 0.42f, 0.85f);
            c[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.40f, 0.62f, 0.92f);
            c[ImGuiCol_HeaderActive]         = ImVec4(0.20f, 0.54f, 0.82f, 1.00f);

            c[ImGuiCol_Separator]            = ImVec4(0.21f, 0.25f, 0.32f, 0.85f);
            c[ImGuiCol_SeparatorHovered]     = ImVec4(0.26f, 0.68f, 0.94f, 0.90f);
            c[ImGuiCol_SeparatorActive]      = ImVec4(0.32f, 0.78f, 1.00f, 1.00f);

            c[ImGuiCol_Tab]                  = ImVec4(0.095f, 0.112f, 0.145f, 1.00f);
            c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.36f, 0.56f, 1.00f);
            c[ImGuiCol_TabSelected]          = ImVec4(0.15f, 0.29f, 0.45f, 1.00f);
            c[ImGuiCol_TabDimmed]            = ImVec4(0.080f, 0.092f, 0.120f, 1.00f);
            c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.115f, 0.160f, 0.230f, 1.00f);

            c[ImGuiCol_DockingPreview]       = ImVec4(0.22f, 0.74f, 0.98f, 0.70f);
            c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.055f, 0.062f, 0.080f, 1.00f);
        }

        std::string MakeTimestampTag()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tmBuf{};
#ifdef _WIN32
            localtime_s(&tmBuf, &tt);
#else
            localtime_r(&tt, &tmBuf);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tmBuf, "%Y%m%d_%H%M%S");
            return oss.str();
        }
    }

    std::string EditorWorkspace::GetWorkspaceConfigDir() const
    {
        return ".hargon";
    }

    std::string EditorWorkspace::SanitizeLayoutName(const std::string& raw)
    {
        std::string out;
        for (char c : raw)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
                out += c;
            else if (c == ' ')
                out += '_';
        }
        return out.empty() ? "CustomLayout" : out;
    }

    void EditorWorkspace::Init(EditorContext& ctx)
    {
        std::error_code ec;
        fs::create_directories(fs::path(GetWorkspaceConfigDir()) / "layouts", ec);

        if (!m_BaseStyleCaptured)
        {
            ApplyHargonPalette(m_BaseStyle);
            m_BaseStyleCaptured = true;
        }

        LoadSettings(ctx);
        ApplyStyle(ctx.settings.uiScale);

        // Crash recovery detection via session lock
        const fs::path lockPath = fs::path(GetWorkspaceConfigDir()) / "session.lock";
        const fs::path recoveryScene = fs::path(GetWorkspaceConfigDir()) / "autosave_recovery.hscene";
        if (fs::exists(lockPath, ec) && fs::exists(recoveryScene, ec))
        {
            ctx.showRecoveryModal = true;
            ctx.recoveryFilePath = recoveryScene.generic_string();
        }

        m_DockInitialized = false;
        m_HasPendingPreset = false;
    }

    void EditorWorkspace::Shutdown(EditorContext& ctx)
    {
        if (m_DockInitialized)
        {
            const fs::path lastIni = fs::path(GetWorkspaceConfigDir()) / "layouts" / "last_layout.ini";
            std::error_code ec;
            fs::create_directories(lastIni.parent_path(), ec);
            ImGui::SaveIniSettingsToDisk(lastIni.string().c_str());
        }
        SaveSettings(ctx);
        ClearRecoveryFiles();
    }

    void EditorWorkspace::ApplyStyle(float uiScale)
    {
        float clamped = std::clamp(uiScale, 0.75f, 2.0f);
        if (!m_BaseStyleCaptured)
        {
            ApplyHargonPalette(m_BaseStyle);
            m_BaseStyleCaptured = true;
        }

        ImGuiStyle scaled = m_BaseStyle;
        scaled.ScaleAllSizes(clamped);
        ImGui::GetStyle() = scaled;
        ImGui::GetIO().FontGlobalScale = clamped;
    }

    void EditorWorkspace::LoadSettings(EditorContext& ctx)
    {
        const fs::path cfgPath = fs::path(GetWorkspaceConfigDir()) / "editor_settings.json";
        std::ifstream ifs(cfgPath);
        if (!ifs.is_open())
            return;

        try
        {
            nlohmann::json j;
            ifs >> j;
            ctx.settings.uiScale = std::clamp(j.value("uiScale", 1.0f), 0.75f, 2.0f);
            ctx.settings.autosaveEnabled = j.value("autosaveEnabled", true);
            ctx.settings.autosaveIntervalSec = std::clamp(j.value("autosaveIntervalSec", 120.0f), 15.0f, 1800.0f);
            ctx.settings.maxBackups = std::clamp(j.value("maxBackups", 10), 1, 50);
            ctx.settings.defaultBackend = j.value("defaultBackend", std::string("Vulkan"));
            ctx.settings.openLastProjectOnStartup = j.value("openLastProjectOnStartup", true);
            m_ActivePreset = static_cast<WorkspacePreset>(std::clamp(j.value("activePreset", 0), 0, 2));
        }
        catch (...)
        {
        }
    }

    void EditorWorkspace::SaveSettings(const EditorContext& ctx)
    {
        std::error_code ec;
        fs::create_directories(GetWorkspaceConfigDir(), ec);
        const fs::path cfgPath = fs::path(GetWorkspaceConfigDir()) / "editor_settings.json";

        nlohmann::json j;
        j["uiScale"] = ctx.settings.uiScale;
        j["autosaveEnabled"] = ctx.settings.autosaveEnabled;
        j["autosaveIntervalSec"] = ctx.settings.autosaveIntervalSec;
        j["maxBackups"] = ctx.settings.maxBackups;
        j["defaultBackend"] = ctx.settings.defaultBackend;
        j["openLastProjectOnStartup"] = ctx.settings.openLastProjectOnStartup;
        j["activePreset"] = static_cast<int>(m_ActivePreset);

        std::ofstream ofs(cfgPath);
        if (ofs.is_open())
            ofs << j.dump(4);
    }

    void EditorWorkspace::RequestPreset(WorkspacePreset preset)
    {
        m_PendingPreset = preset;
        m_HasPendingPreset = true;
    }

    void EditorWorkspace::BuildPresetLayout(EditorContext& ctx, ImGuiID dockspaceId, ImVec2 dockSize, WorkspacePreset preset)
    {
        m_ActivePreset = preset;
        if (dockSize.x < 100.0f) dockSize.x = 1600.0f;
        if (dockSize.y < 100.0f) dockSize.y = 820.0f;

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, dockSize);

        if (preset == WorkspacePreset::SceneEditing)
        {
            ctx.showHierarchy = true;
            ctx.showInspector = true;
            ctx.showViewport = true;
            ctx.showAssetBrowser = true;
            ctx.showConsole = true;
            ctx.showWorldEnv = true;
            ctx.showScriptEditor = false;
            ctx.showSystems = false;

            ImGuiID mainId = dockspaceId;
            ImGuiID leftId = 0, rightId = 0, bottomId = 0;
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.21f, &leftId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.29f, &rightId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.28f, &bottomId, &mainId);

            ImGui::DockBuilderDockWindow(kWinHierarchy, leftId);
            ImGui::DockBuilderDockWindow(kWinViewport, mainId);
            ImGui::DockBuilderDockWindow(kWinAssetBrowser, bottomId);
            ImGui::DockBuilderDockWindow(kWinConsole, bottomId);
            ImGui::DockBuilderDockWindow(kWinInspector, rightId);
            ImGui::DockBuilderDockWindow(kWinWorldEnv, rightId);
        }
        else if (preset == WorkspacePreset::Scripting)
        {
            ctx.showHierarchy = true;
            ctx.showInspector = true;
            ctx.showViewport = true;
            ctx.showAssetBrowser = true;
            ctx.showScriptEditor = true;
            ctx.showConsole = true;
            ctx.showSystems = true;
            ctx.showWorldEnv = false;

            ImGuiID mainId = dockspaceId;
            ImGuiID leftId = 0, rightId = 0, bottomId = 0, rightBottomId = 0;
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.19f, &leftId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.30f, &rightId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.32f, &bottomId, &mainId);
            ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.56f, &rightBottomId, &rightId);

            ImGui::DockBuilderDockWindow(kWinHierarchy, leftId);
            ImGui::DockBuilderDockWindow(kWinAssetBrowser, leftId);
            ImGui::DockBuilderDockWindow(kWinScriptEditor, mainId);
            ImGui::DockBuilderDockWindow(kWinConsole, bottomId);
            ImGui::DockBuilderDockWindow(kWinViewport, rightId);
            ImGui::DockBuilderDockWindow(kWinInspector, rightBottomId);
            ImGui::DockBuilderDockWindow(kWinSystems, rightBottomId);
        }
        else // WorkspacePreset::MaterialsLighting
        {
            ctx.showHierarchy = true;
            ctx.showInspector = true;
            ctx.showViewport = true;
            ctx.showAssetBrowser = true;
            ctx.showConsole = true;
            ctx.showWorldEnv = true;
            ctx.showScriptEditor = false;
            ctx.showSystems = false;

            ImGuiID mainId = dockspaceId;
            ImGuiID leftId = 0, rightId = 0, bottomId = 0, rightBottomId = 0;
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.20f, &leftId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.28f, &rightId, &mainId);
            ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.24f, &bottomId, &mainId);
            ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.48f, &rightBottomId, &rightId);

            ImGui::DockBuilderDockWindow(kWinHierarchy, leftId);
            ImGui::DockBuilderDockWindow(kWinViewport, mainId);
            ImGui::DockBuilderDockWindow(kWinAssetBrowser, bottomId);
            ImGui::DockBuilderDockWindow(kWinConsole, bottomId);
            ImGui::DockBuilderDockWindow(kWinWorldEnv, rightId);
            ImGui::DockBuilderDockWindow(kWinInspector, rightBottomId);
        }

        ImGui::DockBuilderFinish(dockspaceId);

        if (preset == WorkspacePreset::SceneEditing)
            m_PendingFocusWindow = kWinInspector;
        else if (preset == WorkspacePreset::Scripting)
            m_PendingFocusWindow = kWinScriptEditor;
        else
            m_PendingFocusWindow = kWinWorldEnv;
    }

    std::string EditorWorkspace::ConsumePendingFocusWindow()
    {
        std::string out = std::move(m_PendingFocusWindow);
        m_PendingFocusWindow.clear();
        return out;
    }

    void EditorWorkspace::BeginMainDockspace(EditorContext& ctx, float toolbarHeight, float statusHeight)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 dockPos(vp->WorkPos.x, vp->WorkPos.y + toolbarHeight);
        ImVec2 dockSize(
            std::max(64.0f, vp->WorkSize.x),
            std::max(64.0f, vp->WorkSize.y - toolbarHeight - statusHeight));

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos(dockPos);
        ImGui::SetNextWindowSize(dockSize);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##HargonMainDockHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        m_DockspaceId = ImGui::GetID("HargonMainDockSpace");

        if (!m_DockInitialized)
        {
            m_DockInitialized = true;
            const fs::path lastIni = fs::path(GetWorkspaceConfigDir()) / "layouts" / "last_layout.ini";
            std::error_code ec;
            if (!m_HasPendingPreset && fs::exists(lastIni, ec))
            {
                ImGui::LoadIniSettingsFromDisk(lastIni.string().c_str());
            }
            else if (!m_HasPendingPreset)
            {
                m_PendingPreset = WorkspacePreset::SceneEditing;
                m_HasPendingPreset = true;
            }
        }

        if (m_HasPendingPreset)
        {
            m_HasPendingPreset = false;
            BuildPresetLayout(ctx, m_DockspaceId, dockSize, m_PendingPreset);
        }

        ImGui::DockSpace(m_DockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        m_LayoutAutoFlushSec += ImGui::GetIO().DeltaTime;
        if (m_LayoutAutoFlushSec >= 5.0f)
        {
            m_LayoutAutoFlushSec = 0.0f;
            const fs::path lastIni = fs::path(GetWorkspaceConfigDir()) / "layouts" / "last_layout.ini";
            std::error_code ec;
            fs::create_directories(lastIni.parent_path(), ec);
            ImGui::SaveIniSettingsToDisk(lastIni.string().c_str());
        }
    }

    void EditorWorkspace::DrawStatusBar(EditorContext& ctx, float statusHeight)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 pos(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - statusHeight);
        ImVec2 size(vp->WorkSize.x, statusHeight);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.065f, 0.078f, 0.102f, 1.0f));
        ImGui::Begin("##HargonStatusBar", nullptr, flags);

        const bool dirty = (ctx.isSceneDirty && ctx.isSceneDirty()) || (ctx.hasAnyDirtyScriptTabs && ctx.hasAnyDirtyScriptTabs());
        if (dirty)
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.22f, 1.0f), "* Unsaved");
        else
            ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.55f, 1.0f), "Saved");

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        const Entity* sel = (ctx.scene && ctx.selectedEntity != kInvalidEntity) ? ctx.scene->GetEntity(ctx.selectedEntity) : nullptr;
        if (sel)
        {
            if (ctx.selectedEntities.size() > 1)
                ImGui::Text("%zu selected (Primary: %s)", ctx.selectedEntities.size(), sel->tag.name.c_str());
            else
                ImGui::Text("Sel: %s (%.1f, %.1f, %.1f)", sel->tag.name.c_str(), sel->transform.position.x, sel->transform.position.y, sel->transform.position.z);
        }
        else
        {
            ImGui::TextDisabled("No selection (%zu entities)", ctx.scene ? ctx.scene->GetEntities().size() : 0);
        }

        if (!ctx.statusToast.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", ctx.statusToast.c_str());
        }

        // Right-aligned info
        const char* presetName =
            (m_ActivePreset == WorkspacePreset::SceneEditing) ? "Scene Editing" :
            (m_ActivePreset == WorkspacePreset::Scripting) ? "Scripting" : "Materials & Lighting";
        std::string rhiStr = ctx.rhi ? ctx.rhi->GetBackendName() : "N/A";
        float fps = ctx.smoothedFPS ? *ctx.smoothedFPS : 0.0f;
        char rightBuf[192];
        std::snprintf(rightBuf, sizeof(rightBuf), "Layout: %s  |  %s  |  %.0f FPS  |  Scale %.0f%%",
            presetName, rhiStr.c_str(), fps, ctx.settings.uiScale * 100.0f);
        float textW = ImGui::CalcTextSize(rightBuf).x;
        float rightX = std::max(ImGui::GetCursorPosX() + 16.0f, ImGui::GetWindowWidth() - textW - 14.0f);
        ImGui::SameLine(rightX);
        ImGui::TextDisabled("%s", rightBuf);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    std::vector<std::string> EditorWorkspace::ListSavedLayouts() const
    {
        std::vector<std::string> out;
        const fs::path dir = fs::path(GetWorkspaceConfigDir()) / "layouts";
        std::error_code ec;
        if (!fs::exists(dir, ec))
            return out;

        for (const auto& entry : fs::directory_iterator(dir, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".ini")
            {
                std::string stem = entry.path().stem().string();
                if (stem != "last_layout")
                    out.push_back(stem);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void EditorWorkspace::SaveNamedLayout(const std::string& name)
    {
        std::string safe = SanitizeLayoutName(name);
        const fs::path target = fs::path(GetWorkspaceConfigDir()) / "layouts" / (safe + ".ini");
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        ImGui::SaveIniSettingsToDisk(target.string().c_str());
    }

    bool EditorWorkspace::LoadNamedLayout(const std::string& name)
    {
        std::string safe = SanitizeLayoutName(name);
        const fs::path target = fs::path(GetWorkspaceConfigDir()) / "layouts" / (safe + ".ini");
        std::error_code ec;
        if (!fs::exists(target, ec))
            return false;
        ImGui::LoadIniSettingsFromDisk(target.string().c_str());
        return true;
    }

    void EditorWorkspace::DeleteNamedLayout(const std::string& name)
    {
        std::string safe = SanitizeLayoutName(name);
        const fs::path target = fs::path(GetWorkspaceConfigDir()) / "layouts" / (safe + ".ini");
        std::error_code ec;
        fs::remove(target, ec);
    }

    void EditorWorkspace::DrawSaveLayoutModal()
    {
        if (m_ShowSaveLayoutPopup)
        {
            ImGui::OpenPopup("Save Workspace Layout");
            m_ShowSaveLayoutPopup = false;
        }
        if (ImGui::BeginPopupModal("Save Workspace Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Save current docked panel arrangement under .hargon/layouts/:");
            ImGui::InputText("Layout Name", m_SaveLayoutNameBuf, sizeof(m_SaveLayoutNameBuf));
            if (ImGui::Button("Save Layout", ImVec2(120, 0)))
            {
                SaveNamedLayout(m_SaveLayoutNameBuf);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void EditorWorkspace::DrawSettingsModal(EditorContext& ctx)
    {
        if (ctx.showSettingsModal)
        {
            ImGui::OpenPopup("Editor Settings");
            ctx.showSettingsModal = false;
        }
        if (ImGui::BeginPopupModal("Editor Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Hargon Editor Preferences");
            ImGui::Separator();

            bool styleChanged = false;
            float scalePct = ctx.settings.uiScale * 100.0f;
            if (ImGui::SliderFloat("UI Scale (%)", &scalePct, 80.0f, 175.0f, "%.0f%%"))
            {
                ctx.settings.uiScale = scalePct / 100.0f;
                styleChanged = true;
            }
            if (ImGui::Button("100%")) { ctx.settings.uiScale = 1.0f; styleChanged = true; }
            ImGui::SameLine();
            if (ImGui::Button("115%")) { ctx.settings.uiScale = 1.15f; styleChanged = true; }
            ImGui::SameLine();
            if (ImGui::Button("125%")) { ctx.settings.uiScale = 1.25f; styleChanged = true; }
            ImGui::SameLine();
            if (ImGui::Button("150%")) { ctx.settings.uiScale = 1.50f; styleChanged = true; }

            if (styleChanged)
                ApplyStyle(ctx.settings.uiScale);

            ImGui::Separator();
            ImGui::Checkbox("Enable Scene Autosave & Backups", &ctx.settings.autosaveEnabled);
            ImGui::SliderFloat("Autosave Interval (s)", &ctx.settings.autosaveIntervalSec, 30.0f, 600.0f, "%.0f s");
            ImGui::SliderInt("Max Rolling Backups", &ctx.settings.maxBackups, 2, 30);
            ImGui::Checkbox("Reopen Last Project on Startup", &ctx.settings.openLastProjectOnStartup);

            ImGui::Separator();
            if (ImGui::Button("Save Settings", ImVec2(140, 0)))
            {
                SaveSettings(ctx);
                ctx.SetToast("Editor settings saved");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void EditorWorkspace::TickAutosave(EditorContext& ctx, float dt)
    {
        if (!ctx.settings.autosaveEnabled || !ctx.scene || !ctx.commands)
            return;
        if (!ctx.commands->IsDirty())
        {
            m_AutosaveAccumSec = 0.0f;
            return;
        }
        if (ctx.playState && *ctx.playState != PlayState::Edit)
            return;

        m_AutosaveAccumSec += dt;
        if (m_AutosaveAccumSec < ctx.settings.autosaveIntervalSec)
            return;
        m_AutosaveAccumSec = 0.0f;

        std::error_code ec;
        const fs::path recoveryPath = fs::path(GetWorkspaceConfigDir()) / "autosave_recovery.hscene";
        fs::create_directories(recoveryPath.parent_path(), ec);
        SceneSerializer::SaveToFile(*ctx.scene, recoveryPath.string());

        if (ctx.getActiveSceneBackupDir)
        {
            std::string backupDir = ctx.getActiveSceneBackupDir();
            if (!backupDir.empty())
            {
                fs::create_directories(backupDir, ec);
                std::string stem = ctx.currentScenePath ? fs::path(*ctx.currentScenePath).stem().string() : "scene";
                if (stem.empty()) stem = "scene";
                fs::path backupFile = fs::path(backupDir) / (stem + "_" + MakeTimestampTag() + ".hscene");
                SceneSerializer::SaveToFile(*ctx.scene, backupFile.string());

                // Prune oldest backups beyond maxBackups
                std::vector<fs::directory_entry> backups;
                for (const auto& entry : fs::directory_iterator(backupDir, ec))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".hscene")
                        backups.push_back(entry);
                }
                if (static_cast<int>(backups.size()) > ctx.settings.maxBackups)
                {
                    std::sort(backups.begin(), backups.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                        std::error_code t1, t2;
                        return a.last_write_time(t1) < b.last_write_time(t2);
                    });
                    int toRemove = static_cast<int>(backups.size()) - ctx.settings.maxBackups;
                    for (int i = 0; i < toRemove; ++i)
                        fs::remove(backups[i].path(), ec);
                }
            }
        }
    }

    void EditorWorkspace::UpdateSessionLock(const EditorContext& ctx)
    {
        const fs::path lockPath = fs::path(GetWorkspaceConfigDir()) / "session.lock";
        std::error_code ec;
        fs::create_directories(lockPath.parent_path(), ec);
        std::ofstream ofs(lockPath);
        if (ofs.is_open())
        {
            nlohmann::json j;
            j["projectRootDir"] = ctx.projectConfig ? ctx.projectConfig->projectRootDir : "";
            j["projectName"] = ctx.projectConfig ? ctx.projectConfig->projectName : "";
            j["scenePath"] = ctx.currentScenePath ? *ctx.currentScenePath : "";
            ofs << j.dump(2);
        }
    }

    void EditorWorkspace::ClearRecoveryFiles()
    {
        std::error_code ec;
        fs::remove(fs::path(GetWorkspaceConfigDir()) / "session.lock", ec);
        fs::remove(fs::path(GetWorkspaceConfigDir()) / "autosave_recovery.hscene", ec);
    }
}

#pragma once
#include "EditorContext.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace Hargon
{
    class EditorWorkspace
    {
    public:
        void Init(EditorContext& ctx);
        void Shutdown(EditorContext& ctx);

        void ApplyStyle(float uiScale);
        void SaveSettings(const EditorContext& ctx);
        void LoadSettings(EditorContext& ctx);

        void BeginMainDockspace(EditorContext& ctx, float toolbarHeight, float statusHeight);
        void DrawStatusBar(EditorContext& ctx, float statusHeight);
        void DrawSettingsModal(EditorContext& ctx);
        void DrawSaveLayoutModal();

        void RequestPreset(WorkspacePreset preset);
        void RequestSaveLayoutPopup() { m_ShowSaveLayoutPopup = true; }
        std::string ConsumePendingFocusWindow();

        WorkspacePreset GetActivePreset() const { return m_ActivePreset; }
        std::vector<std::string> ListSavedLayouts() const;
        void SaveNamedLayout(const std::string& name);
        bool LoadNamedLayout(const std::string& name);
        void DeleteNamedLayout(const std::string& name);

        void TickAutosave(EditorContext& ctx, float dt);
        void UpdateSessionLock(const EditorContext& ctx);
        void ClearRecoveryFiles();
        std::string GetWorkspaceConfigDir() const;

    private:
        void BuildPresetLayout(EditorContext& ctx, ImGuiID dockspaceId, ImVec2 dockSize, WorkspacePreset preset);
        static std::string SanitizeLayoutName(const std::string& raw);

    private:
        ImGuiStyle m_BaseStyle{};
        bool m_BaseStyleCaptured = false;
        ImGuiID m_DockspaceId = 0;
        bool m_DockInitialized = false;
        bool m_HasPendingPreset = false;
        WorkspacePreset m_PendingPreset = WorkspacePreset::SceneEditing;
        WorkspacePreset m_ActivePreset = WorkspacePreset::SceneEditing;
        std::string m_PendingFocusWindow;

        bool m_ShowSaveLayoutPopup = false;
        char m_SaveLayoutNameBuf[64] = "CustomLayout";
        float m_AutosaveAccumSec = 0.0f;
        float m_LayoutAutoFlushSec = 0.0f;
    };
}

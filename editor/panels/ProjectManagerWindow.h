#pragma once
#include "../EditorContext.h"
#include <string>
#include <vector>

namespace Hargon
{
    class ProjectManagerWindow
    {
    public:
        void RefreshProjectList();
        void DrawModals(EditorContext& ctx, const std::function<void(const PendingUnsavedAction&)>& executeAction);

    private:
        void DrawProjectManagerModal(EditorContext& ctx);
        void DrawExportModal(EditorContext& ctx);
        void DrawUnsavedConfirmModal(EditorContext& ctx, const std::function<void(const PendingUnsavedAction&)>& executeAction);
        void DrawRecoveryModal(EditorContext& ctx);

    private:
        std::vector<GameProjectConfig> m_DiscoveredProjects;
        bool m_ListLoaded = false;

        // Create Project form
        char m_SlugBuf[64] = "NewProject3D";
        char m_TitleBuf[128] = "My New 3D Game";
        char m_GenreBuf[64] = "Action 3D";
        int m_TemplateIdx = 1;
        bool m_EnableBadLevel = false;
        std::string m_ProjectStatusMsg;
        bool m_ProjectStatusError = false;

        // Export Standalone form
        char m_ExportDirBuf[256] = "";
        bool m_PackageRuntime = true;
        std::string m_ExportStatusMsg;
        bool m_ExportSuccess = false;
        std::string m_LastExportProjectSlug;
    };
}

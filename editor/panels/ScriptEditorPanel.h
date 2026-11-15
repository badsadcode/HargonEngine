#pragma once
#include "../EditorContext.h"
#include <string>
#include <vector>

namespace Hargon
{
    class ScriptEditorPanel
    {
    public:
        void OpenScript(const EditorContext& ctx, const std::string& projectRelOrAbsPath, int jumpToLine = -1);
        void Draw(EditorContext& ctx);

        bool HasAnyDirtyTabs() const;
        bool SaveAllDirtyTabs(EditorContext& ctx);
        void DiscardAllDirtyTabs();
        void ClearAllTabs();

    private:
        struct ScriptTab
        {
            std::string displayName;
            std::string relPath;
            std::string diskPath;
            std::string code;
            std::string savedCode;
            int cursorPos = 0;
            int cursorLine = 1;
            int cursorCol = 1;
            int pendingJumpLine = -1;
            int errorLine = -1;
            std::string errorMessage;
            bool IsDirty() const { return code != savedCode; }
        };

        void DrawSyntaxHighlightedEditor(ScriptTab& tab, const ImVec2& availSize, EditorContext& ctx);
        bool SaveTab(EditorContext& ctx, ScriptTab& tab);
        void RefreshDiagnostics(ScriptTab& tab);
        static std::string ResolveDiskPath(const EditorContext& ctx, const std::string& path);

    private:
        std::vector<ScriptTab> m_Tabs;
        int m_ActiveTab = -1;
        int m_PendingCloseTab = -1;
        bool m_ShowFindBar = false;
        char m_FindBuf[128] = "";
        char m_ReplaceBuf[128] = "";
        bool m_OpenUnsavedCloseModal = false;
    };
}

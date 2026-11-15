#pragma once
#include "../EditorContext.h"
#include <string>
#include <vector>

namespace Hargon
{
    class AssetBrowserPanel
    {
    public:
        void Draw(EditorContext& ctx);

    private:
        struct AssetEntry
        {
            std::string name;
            std::string relPath;
            std::string fullPath;
            std::string extension;
            std::string category;
            uintmax_t fileSize = 0;
            bool isDirectory = false;
            bool isShared = false;
        };

        void RefreshEntries(const EditorContext& ctx);
        void LoadManifestUIDs(const EditorContext& ctx);

    private:
        char m_SearchBuf[128] = "";
        int m_SelectedCategory = 0; // 0=All, 1=scenes, 2=scripts, 3=prefabs, 4=models, 5=materials, 6=textures, 7=sounds, 8=animations
        bool m_ShowSharedAssets = false;
        bool m_NeedsRefresh = true;
        std::string m_LastProjectDir;
        std::vector<AssetEntry> m_Entries;
        std::vector<std::pair<std::string, std::string>> m_UidMap; // relPath -> uid
        int m_SelectedIdx = -1;
        char m_NewScriptNameBuf[64] = "new_script";
        bool m_OpenNewScriptPopup = false;
    };
}

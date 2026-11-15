#pragma once
#include "../EditorContext.h"
#include <string>
#include <vector>

namespace Hargon
{
    class HierarchyPanel
    {
    public:
        void Draw(EditorContext& ctx);

    private:
        void DrawNodeRecursive(
            EditorContext& ctx,
            EntityID entityId,
            std::vector<EntityID>& visibleOrder,
            int depth);
        void DrawEntityContextMenu(EditorContext& ctx, EntityID targetId);
        void DrawCreateMenuItems(EditorContext& ctx, EntityID parentId);
        void ExecuteRenameModal(EditorContext& ctx);

    private:
        char m_SearchBuf[128] = "";
        EntityID m_LastClickedId = kInvalidEntity;
        EntityID m_RenameEntityId = kInvalidEntity;
        char m_RenameBuf[128] = "";
        bool m_OpenRenamePopup = false;
    };
}

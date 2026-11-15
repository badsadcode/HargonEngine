#pragma once
#include "../EditorContext.h"
#include "../../engine/scene/Scene.h"
#include <functional>
#include <string>
#include <vector>

namespace Hargon
{
    class EditorCommandSystem
    {
    public:
        struct CommandItem
        {
            std::string description;
            std::string mergeKey;
            double timestampSec = 0.0;
            std::function<void()> undoFn;
            std::function<void()> redoFn;
        };

        EditorCommandSystem() = default;

        void SetMaxSteps(int maxSteps) { m_MaxSteps = std::max(8, maxSteps); }
        void SetOnMutatedCallback(std::function<void()> cb) { m_OnMutated = std::move(cb); }

        void Clear();
        void MarkSaved();
        void MarkDirtyExternally();

        bool IsDirty() const;
        bool HasUndo() const { return m_Cursor > 0; }
        bool HasRedo() const { return m_Cursor < static_cast<int>(m_History.size()); }

        bool Undo();
        bool Redo();

        std::string GetNextUndoLabel() const;
        std::string GetNextRedoLabel() const;

        const std::vector<CommandItem>& GetHistory() const { return m_History; }
        int GetCursor() const { return m_Cursor; }
        int GetSavedCursor() const { return m_SavedCursor; }
        bool IsSaveReachable() const { return m_SaveReachable; }

        void PushCommand(
            const std::string& description,
            std::function<void()> undoFn,
            std::function<void()> redoFn,
            const std::string& mergeKey = "");

        void RecordEntityEdit(
            Scene& scene,
            EntityID entityId,
            const Entity& beforeState,
            const Entity& afterState,
            const std::string& label,
            const std::string& mergeKey = "",
            std::function<void()> postApply = nullptr);

        void RecordMultiTransformEdit(
            Scene& scene,
            const std::vector<EntityID>& entityIds,
            const std::vector<TransformComponent>& beforeTransforms,
            const std::vector<TransformComponent>& afterTransforms,
            const std::string& label,
            const std::string& mergeKey = "");

        void RecordSceneSnapshot(
            Scene& scene,
            EditorContext& ctx,
            std::vector<Entity> beforeEntities,
            std::vector<EntityID> beforeSelection,
            EntityID beforePrimary,
            std::vector<Entity> afterEntities,
            std::vector<EntityID> afterSelection,
            EntityID afterPrimary,
            const std::string& label);

        void RecordEnvironmentEdit(
            Scene& scene,
            const SceneEnvSnapshot& beforeEnv,
            const SceneEnvSnapshot& afterEnv,
            const std::string& label,
            const std::string& mergeKey = "");

    private:
        double NowSeconds() const;

    private:
        std::vector<CommandItem> m_History;
        int m_Cursor = 0;
        int m_SavedCursor = 0;
        bool m_SaveReachable = true;
        int m_MaxSteps = 128;
        std::function<void()> m_OnMutated;
    };
}

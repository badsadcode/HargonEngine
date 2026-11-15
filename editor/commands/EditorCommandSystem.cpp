#include "EditorCommandSystem.h"
#include "../../engine/core/Log.h"
#include <chrono>
#include <utility>

namespace Hargon
{
    double EditorCommandSystem::NowSeconds() const
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }

    void EditorCommandSystem::Clear()
    {
        m_History.clear();
        m_Cursor = 0;
        m_SavedCursor = 0;
        m_SaveReachable = true;
    }

    void EditorCommandSystem::MarkSaved()
    {
        m_SavedCursor = m_Cursor;
        m_SaveReachable = true;
    }

    void EditorCommandSystem::MarkDirtyExternally()
    {
        m_SaveReachable = false;
    }

    bool EditorCommandSystem::IsDirty() const
    {
        return !(m_SaveReachable && m_Cursor == m_SavedCursor);
    }

    bool EditorCommandSystem::Undo()
    {
        if (!HasUndo())
            return false;

        --m_Cursor;
        if (m_History[m_Cursor].undoFn)
            m_History[m_Cursor].undoFn();
        if (m_OnMutated)
            m_OnMutated();
        return true;
    }

    bool EditorCommandSystem::Redo()
    {
        if (!HasRedo())
            return false;

        if (m_History[m_Cursor].redoFn)
            m_History[m_Cursor].redoFn();
        ++m_Cursor;
        if (m_OnMutated)
            m_OnMutated();
        return true;
    }

    std::string EditorCommandSystem::GetNextUndoLabel() const
    {
        if (!HasUndo())
            return {};
        return m_History[m_Cursor - 1].description;
    }

    std::string EditorCommandSystem::GetNextRedoLabel() const
    {
        if (!HasRedo())
            return {};
        return m_History[m_Cursor].description;
    }

    void EditorCommandSystem::PushCommand(
        const std::string& description,
        std::function<void()> undoFn,
        std::function<void()> redoFn,
        const std::string& mergeKey)
    {
        const double now = NowSeconds();

        // Discard truncated redo tail
        if (m_Cursor < static_cast<int>(m_History.size()))
        {
            if (m_SaveReachable && m_SavedCursor > m_Cursor)
                m_SaveReachable = false;
            m_History.erase(m_History.begin() + m_Cursor, m_History.end());
        }

        // Merge consecutive drag/slider edits ONLY if the previous entry is not the active save point
        const bool prevIsSavedPoint = (m_SaveReachable && m_Cursor == m_SavedCursor);
        if (!mergeKey.empty() && m_Cursor > 0 && !prevIsSavedPoint)
        {
            CommandItem& top = m_History[m_Cursor - 1];
            if (top.mergeKey == mergeKey && (now - top.timestampSec) < 1.25)
            {
                top.description = description;
                top.timestampSec = now;
                top.redoFn = std::move(redoFn);
                if (m_OnMutated)
                    m_OnMutated();
                return;
            }
        }

        CommandItem item;
        item.description = description;
        item.mergeKey = mergeKey;
        item.timestampSec = now;
        item.undoFn = std::move(undoFn);
        item.redoFn = std::move(redoFn);
        m_History.push_back(std::move(item));
        ++m_Cursor;

        while (static_cast<int>(m_History.size()) > m_MaxSteps)
        {
            m_History.erase(m_History.begin());
            --m_Cursor;
            if (m_SaveReachable)
            {
                --m_SavedCursor;
                if (m_SavedCursor < 0)
                    m_SaveReachable = false;
            }
        }

        if (m_OnMutated)
            m_OnMutated();
    }

    void EditorCommandSystem::RecordEntityEdit(
        Scene& scene,
        EntityID entityId,
        const Entity& beforeState,
        const Entity& afterState,
        const std::string& label,
        const std::string& mergeKey,
        std::function<void()> postApply)
    {
        auto applyState = [&scene, entityId, postApply](const Entity& state) {
            if (Entity* e = scene.GetEntity(entityId))
            {
                *e = state;
                if (postApply)
                    postApply();
            }
        };

        PushCommand(
            label,
            [applyState, beforeState]() { applyState(beforeState); },
            [applyState, afterState]()  { applyState(afterState); },
            mergeKey);
    }

    void EditorCommandSystem::RecordMultiTransformEdit(
        Scene& scene,
        const std::vector<EntityID>& entityIds,
        const std::vector<TransformComponent>& beforeTransforms,
        const std::vector<TransformComponent>& afterTransforms,
        const std::string& label,
        const std::string& mergeKey)
    {
        if (entityIds.empty() || entityIds.size() != beforeTransforms.size() || entityIds.size() != afterTransforms.size())
            return;

        auto applyTransforms = [&scene, entityIds](const std::vector<TransformComponent>& xforms) {
            for (size_t i = 0; i < entityIds.size(); ++i)
            {
                if (Entity* e = scene.GetEntity(entityIds[i]))
                    e->transform = xforms[i];
            }
        };

        PushCommand(
            label,
            [applyTransforms, beforeTransforms]() { applyTransforms(beforeTransforms); },
            [applyTransforms, afterTransforms]()  { applyTransforms(afterTransforms); },
            mergeKey);
    }

    void EditorCommandSystem::RecordSceneSnapshot(
        Scene& scene,
        EditorContext& ctx,
        std::vector<Entity> beforeEntities,
        std::vector<EntityID> beforeSelection,
        EntityID beforePrimary,
        std::vector<Entity> afterEntities,
        std::vector<EntityID> afterSelection,
        EntityID afterPrimary,
        const std::string& label)
    {
        auto restore = [&scene, &ctx](const std::vector<Entity>& ents, const std::vector<EntityID>& sel, EntityID primary) {
            scene.GetEntities() = ents;
            ctx.selectedEntities = sel;
            ctx.selectedEntity = primary;
            ctx.PruneInvalidSelection();
        };

        PushCommand(
            label,
            [restore, beforeEntities = std::move(beforeEntities), beforeSelection = std::move(beforeSelection), beforePrimary]() {
                restore(beforeEntities, beforeSelection, beforePrimary);
            },
            [restore, afterEntities = std::move(afterEntities), afterSelection = std::move(afterSelection), afterPrimary]() {
                restore(afterEntities, afterSelection, afterPrimary);
            },
            "");
    }

    void EditorCommandSystem::RecordEnvironmentEdit(
        Scene& scene,
        const SceneEnvSnapshot& beforeEnv,
        const SceneEnvSnapshot& afterEnv,
        const std::string& label,
        const std::string& mergeKey)
    {
        PushCommand(
            label,
            [&scene, beforeEnv]() { beforeEnv.Apply(scene); },
            [&scene, afterEnv]()  { afterEnv.Apply(scene); },
            mergeKey);
    }
}

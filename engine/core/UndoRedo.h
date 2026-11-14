#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Hargon
{
    // Inspired by Godot Engine's core/object/undo_redo.h
    class UndoRedo
    {
    public:
        enum class MergeMode
        {
            Disable = 0,
            MergeEnds = 1
        };

        struct Action
        {
            std::string name;
            std::vector<std::function<void()>> doOps;
            std::vector<std::function<void()>> undoOps;
            MergeMode mergeMode = MergeMode::Disable;
        };

    public:
        UndoRedo() = default;
        ~UndoRedo() = default;

        void CreateAction(const std::string& name, MergeMode mergeMode = MergeMode::Disable);
        void AddDoMethod(std::function<void()> fn);
        void AddUndoMethod(std::function<void()> fn);
        void CommitAction(bool executeDo = true);

        bool Undo();
        bool Redo();

        bool HasUndo() const { return m_CurrentAction >= 0; }
        bool HasRedo() const { return m_CurrentAction + 1 < static_cast<int>(m_Actions.size()); }

        void ClearHistory();

        int GetHistoryCount() const { return static_cast<int>(m_Actions.size()); }
        int GetCurrentActionIndex() const { return m_CurrentAction; }
        std::string GetActionName(int index) const;
        std::string GetCurrentActionName() const;
        uint64_t GetVersion() const { return m_Version; }

    private:
        std::vector<Action> m_Actions;
        int m_CurrentAction = -1;
        bool m_IsBuildingAction = false;
        bool m_MergedWithPrevious = false;
        uint64_t m_Version = 1;
        int m_MaxSteps = 64;
    };
}

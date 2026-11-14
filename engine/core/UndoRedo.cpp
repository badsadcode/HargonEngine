#include "UndoRedo.h"
#include "Log.h"

namespace Hargon
{
    void UndoRedo::CreateAction(const std::string& name, MergeMode mergeMode)
    {
        // Discard any redo tail beyond m_CurrentAction
        if (m_CurrentAction + 1 < static_cast<int>(m_Actions.size()))
        {
            m_Actions.erase(m_Actions.begin() + m_CurrentAction + 1, m_Actions.end());
        }

        // Godot MERGE_ENDS behavior: if the previous committed action has the same name and MergeEnds,
        // keep its initial undoOps and replace its doOps!
        if (mergeMode == MergeMode::MergeEnds &&
            m_CurrentAction >= 0 &&
            m_Actions[m_CurrentAction].mergeMode == MergeMode::MergeEnds &&
            m_Actions[m_CurrentAction].name == name)
        {
            m_Actions[m_CurrentAction].doOps.clear();
            m_IsBuildingAction = true;
            m_MergedWithPrevious = true;
            return;
        }

        if (static_cast<int>(m_Actions.size()) >= m_MaxSteps)
        {
            m_Actions.erase(m_Actions.begin());
            --m_CurrentAction;
        }

        Action act;
        act.name = name;
        act.mergeMode = mergeMode;
        m_Actions.push_back(std::move(act));
        m_CurrentAction = static_cast<int>(m_Actions.size()) - 1;
        m_IsBuildingAction = true;
        m_MergedWithPrevious = false;
    }

    void UndoRedo::AddDoMethod(std::function<void()> fn)
    {
        if (!m_IsBuildingAction || m_CurrentAction < 0)
            return;
        m_Actions[m_CurrentAction].doOps.push_back(std::move(fn));
    }

    void UndoRedo::AddUndoMethod(std::function<void()> fn)
    {
        if (!m_IsBuildingAction || m_CurrentAction < 0)
            return;
        // When merging ends (e.g. continuous gizmo drag), keep the original starting undo state
        if (m_MergedWithPrevious && !m_Actions[m_CurrentAction].undoOps.empty())
            return;
        m_Actions[m_CurrentAction].undoOps.push_back(std::move(fn));
    }

    void UndoRedo::CommitAction(bool executeDo)
    {
        if (!m_IsBuildingAction || m_CurrentAction < 0)
            return;
        m_IsBuildingAction = false;

        if (executeDo)
        {
            for (auto& op : m_Actions[m_CurrentAction].doOps)
            {
                if (op) op();
            }
        }
        ++m_Version;
    }

    bool UndoRedo::Undo()
    {
        if (!HasUndo())
            return false;

        Action& act = m_Actions[m_CurrentAction];
        for (int i = static_cast<int>(act.undoOps.size()) - 1; i >= 0; --i)
        {
            if (act.undoOps[i])
                act.undoOps[i]();
        }
        Log::Editor("Undo: " + act.name);
        --m_CurrentAction;
        ++m_Version;
        return true;
    }

    bool UndoRedo::Redo()
    {
        if (!HasRedo())
            return false;

        ++m_CurrentAction;
        Action& act = m_Actions[m_CurrentAction];
        for (auto& op : act.doOps)
        {
            if (op) op();
        }
        Log::Editor("Redo: " + act.name);
        ++m_Version;
        return true;
    }

    void UndoRedo::ClearHistory()
    {
        m_Actions.clear();
        m_CurrentAction = -1;
        m_IsBuildingAction = false;
        m_MergedWithPrevious = false;
        ++m_Version;
    }

    std::string UndoRedo::GetActionName(int index) const
    {
        if (index >= 0 && index < static_cast<int>(m_Actions.size()))
            return m_Actions[index].name;
        return "";
    }

    std::string UndoRedo::GetCurrentActionName() const
    {
        return GetActionName(m_CurrentAction);
    }
}

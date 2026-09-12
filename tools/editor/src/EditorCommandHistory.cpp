#include <Lorenzo2DEditor/EditorCommandHistory.hpp>

#include <utility>

namespace l2d_editor
{
    EditorCommandHistory::EditorCommandHistory()
    {
        m_undo.reserve(MaximumCommandCount);
        m_redo.reserve(MaximumCommandCount);
    }

    bool EditorCommandHistory::execute(EditorDocument& document, std::string label,
                                       const Mutation& mutation)
    {
        if (m_pending || label.empty() || label.size() > MaximumLabelBytes || !mutation) return false;

        EditorDocument::Snapshot before = document.snapshot();
        try
        {
            if (!mutation(document))
            {
                document.restore(before);
                return false;
            }

            EditorDocument::Snapshot after = document.snapshot();
            pushCompleted({std::move(label), std::move(before), std::move(after)});
            m_redo.clear();
            return true;
        }
        catch (...)
        {
            document.restore(before);
            throw;
        }
    }

    bool EditorCommandHistory::beginCoalescedCommand(EditorDocument& document, std::string label)
    {
        if (m_pending || label.empty() || label.size() > MaximumLabelBytes) return false;
        m_pending = PendingCommand{std::move(label), document.snapshot(), 0u};
        return true;
    }

    bool EditorCommandHistory::updateCoalescedCommand(EditorDocument& document,
                                                      const Mutation& mutation)
    {
        if (!m_pending || !mutation) return false;

        EditorDocument::Snapshot beforeUpdate = document.snapshot();
        try
        {
            if (!mutation(document))
            {
                document.restore(beforeUpdate);
                return false;
            }
            ++m_pending->successfulUpdates;
            return true;
        }
        catch (...)
        {
            document.restore(beforeUpdate);
            throw;
        }
    }

    bool EditorCommandHistory::commitCoalescedCommand(EditorDocument& document)
    {
        if (!m_pending) return false;
        if (m_pending->successfulUpdates == 0u)
        {
            m_pending.reset();
            return false;
        }

        EditorDocument::Snapshot after = document.snapshot();
        PendingCommand pending = std::move(*m_pending);
        m_pending.reset();
        pushCompleted({std::move(pending.label), std::move(pending.before), std::move(after)});
        m_redo.clear();
        return true;
    }

    bool EditorCommandHistory::cancelCoalescedCommand(EditorDocument& document)
    {
        if (!m_pending) return false;
        EditorDocument::Snapshot before = std::move(m_pending->before);
        m_pending.reset();
        document.restore(before);
        return true;
    }

    bool EditorCommandHistory::hasOpenCoalescedCommand() const noexcept
    {
        return m_pending.has_value();
    }

    bool EditorCommandHistory::undo(EditorDocument& document)
    {
        if (m_pending || m_undo.empty()) return false;

        Command& command = m_undo.back();
        document.restore(command.before);
        m_redo.push_back(std::move(command));
        m_undo.pop_back();
        return true;
    }

    bool EditorCommandHistory::redo(EditorDocument& document)
    {
        if (m_pending || m_redo.empty()) return false;

        Command& command = m_redo.back();
        document.restore(command.after);
        m_undo.push_back(std::move(command));
        m_redo.pop_back();
        return true;
    }

    void EditorCommandHistory::clear() noexcept
    {
        m_undo.clear();
        m_redo.clear();
    }

    bool EditorCommandHistory::canUndo() const noexcept
    {
        return !m_pending && !m_undo.empty();
    }

    bool EditorCommandHistory::canRedo() const noexcept
    {
        return !m_pending && !m_redo.empty();
    }

    std::size_t EditorCommandHistory::undoCount() const noexcept
    {
        return m_undo.size();
    }

    std::size_t EditorCommandHistory::redoCount() const noexcept
    {
        return m_redo.size();
    }

    std::string_view EditorCommandHistory::undoLabel() const noexcept
    {
        return m_pending || m_undo.empty() ? std::string_view{} : std::string_view(m_undo.back().label);
    }

    std::string_view EditorCommandHistory::redoLabel() const noexcept
    {
        return m_pending || m_redo.empty() ? std::string_view{} : std::string_view(m_redo.back().label);
    }

    void EditorCommandHistory::pushCompleted(Command command)
    {
        if (m_undo.size() >= MaximumCommandCount) m_undo.erase(m_undo.begin());
        m_undo.push_back(std::move(command));
    }
}

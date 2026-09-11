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
        if (label.empty() || label.size() > MaximumLabelBytes || !mutation) return false;

        EditorDocument::Snapshot before = document.snapshot();
        try
        {
            if (!mutation(document))
            {
                document.restore(before);
                return false;
            }

            EditorDocument::Snapshot after = document.snapshot();
            if (m_undo.size() >= MaximumCommandCount) m_undo.erase(m_undo.begin());
            m_undo.push_back({std::move(label), std::move(before), std::move(after)});
            m_redo.clear();
            return true;
        }
        catch (...)
        {
            document.restore(before);
            throw;
        }
    }

    bool EditorCommandHistory::undo(EditorDocument& document)
    {
        if (m_undo.empty()) return false;

        Command& command = m_undo.back();
        document.restore(command.before);
        m_redo.push_back(std::move(command));
        m_undo.pop_back();
        return true;
    }

    bool EditorCommandHistory::redo(EditorDocument& document)
    {
        if (m_redo.empty()) return false;

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
        return !m_undo.empty();
    }

    bool EditorCommandHistory::canRedo() const noexcept
    {
        return !m_redo.empty();
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
        return m_undo.empty() ? std::string_view{} : std::string_view(m_undo.back().label);
    }

    std::string_view EditorCommandHistory::redoLabel() const noexcept
    {
        return m_redo.empty() ? std::string_view{} : std::string_view(m_redo.back().label);
    }
}

#pragma once

#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace l2d_editor
{
    class EditorCommandHistory
    {
      public:
        static constexpr std::size_t MaximumCommandCount = 256u;
        static constexpr std::size_t MaximumLabelBytes = 128u;

        using Mutation = std::function<bool(EditorDocument&)>;

        EditorCommandHistory();

        // Executes one editor mutation transactionally. Returning false from
        // the mutation restores the exact pre-command document state. Thrown
        // exceptions also restore that state before propagating.
        [[nodiscard]] bool execute(EditorDocument& document, std::string label,
                                   const Mutation& mutation);

        [[nodiscard]] bool undo(EditorDocument& document);
        [[nodiscard]] bool redo(EditorDocument& document);
        void clear() noexcept;

        [[nodiscard]] bool canUndo() const noexcept;
        [[nodiscard]] bool canRedo() const noexcept;
        [[nodiscard]] std::size_t undoCount() const noexcept;
        [[nodiscard]] std::size_t redoCount() const noexcept;
        [[nodiscard]] std::string_view undoLabel() const noexcept;
        [[nodiscard]] std::string_view redoLabel() const noexcept;

      private:
        struct Command
        {
            std::string label;
            EditorDocument::Snapshot before;
            EditorDocument::Snapshot after;
        };

        std::vector<Command> m_undo;
        std::vector<Command> m_redo;
    };
}

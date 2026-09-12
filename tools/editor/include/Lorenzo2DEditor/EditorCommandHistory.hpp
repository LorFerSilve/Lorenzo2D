#pragma once

#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <cstddef>
#include <functional>
#include <optional>
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

        // A coalesced command is an explicit multi-update editor gesture. The
        // first snapshot is retained while updateCoalescedCommand() publishes
        // live document state. commitCoalescedCommand() records exactly one
        // undo entry; cancelCoalescedCommand() restores the gesture-start state.
        // Normal execute/undo/redo operations are rejected while a coalesced
        // command is open so history ordering cannot become ambiguous.
        [[nodiscard]] bool beginCoalescedCommand(EditorDocument& document, std::string label);
        [[nodiscard]] bool updateCoalescedCommand(EditorDocument& document,
                                                  const Mutation& mutation);
        [[nodiscard]] bool commitCoalescedCommand(EditorDocument& document);
        [[nodiscard]] bool cancelCoalescedCommand(EditorDocument& document);
        [[nodiscard]] bool hasOpenCoalescedCommand() const noexcept;

        [[nodiscard]] bool undo(EditorDocument& document);
        [[nodiscard]] bool redo(EditorDocument& document);

        // Clears completed undo/redo entries. An open coalesced command is
        // deliberately retained because clear() has no document parameter with
        // which it could safely restore the gesture-start snapshot.
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

        struct PendingCommand
        {
            std::string label;
            EditorDocument::Snapshot before;
            std::size_t successfulUpdates = 0u;
        };

        void pushCompleted(Command command);

        std::vector<Command> m_undo;
        std::vector<Command> m_redo;
        std::optional<PendingCommand> m_pending;
    };
}

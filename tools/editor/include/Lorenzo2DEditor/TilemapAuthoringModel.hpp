#pragma once

#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TiledJsonImporter.hpp>

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <vector>

namespace l2d_editor
{
    struct TilemapAuthoringLimits
    {
        std::size_t maxInputBytes = 16u * 1024u * 1024u;
        std::size_t maxCellCount = 256u * 1024u;
        std::size_t maxLayerCount = 64u;
        std::size_t maxDefinitionCount = 4096u;
        std::size_t maxObjectCount = 65536u;
        std::size_t maxTileSlotCount = 1024u * 1024u;
        std::size_t maxStrokeCellCount = 4096u;
        std::size_t maxHistoryCommandCount = 128u;
    };

    enum class TilemapAuthoringError
    {
        None,
        InvalidConfiguration,
        ImportFailed,
        InvalidMap,
        CellLimitExceeded,
        LayerLimitExceeded,
        DefinitionLimitExceeded,
        ObjectLimitExceeded,
        TileSlotLimitExceeded,
        InvalidSelection,
        OperationInProgress,
        NoOpenStroke,
        StrokeCellLimitExceeded,
        MutationRejected
    };

    class TilemapAuthoringModel
    {
      public:
        explicit TilemapAuthoringModel(TilemapAuthoringLimits limits = {});

        // Tiled JSON is imported through the installed public runtime importer.
        // Publication happens only after the complete candidate passes both the
        // runtime TileMapData invariants and the stricter editor authoring limits.
        [[nodiscard]] bool loadTiled(std::istream& input);
        [[nodiscard]] bool replace(l2d::TileMapData data);

        [[nodiscard]] bool hasMap() const noexcept;
        [[nodiscard]] const l2d::TileMapData& data() const noexcept;
        [[nodiscard]] const TilemapAuthoringLimits& limits() const noexcept;
        [[nodiscard]] TilemapAuthoringError lastError() const noexcept;

        // Layer order is the canonical TileMapData order. Tile IDs are exposed
        // in sorted order so an editor palette never depends on unordered_map
        // iteration order.
        [[nodiscard]] const std::vector<l2d::TileId>& tileIds() const noexcept;
        [[nodiscard]] std::optional<std::size_t> selectedLayer() const noexcept;
        [[nodiscard]] l2d::TileId selectedTile() const noexcept;
        [[nodiscard]] l2d::TileFlipFlags selectedFlipFlags() const noexcept;
        [[nodiscard]] bool selectLayer(std::size_t layerIndex);
        [[nodiscard]] bool selectTile(
            l2d::TileId tile, l2d::TileFlipFlags flags = l2d::TileFlipFlags::None);

        // paintCell() is one complete undoable mutation. Continuous painting
        // uses begin/paint/commit and records exactly one bounded delta command
        // regardless of how many pointer updates are produced by the shell.
        [[nodiscard]] bool paintCell(l2d::TileMapCell cell);
        [[nodiscard]] bool beginPaintStroke();
        [[nodiscard]] bool paintStrokeCell(l2d::TileMapCell cell);
        [[nodiscard]] bool commitPaintStroke();
        [[nodiscard]] bool cancelPaintStroke();
        [[nodiscard]] bool isPainting() const noexcept;

        [[nodiscard]] bool undo();
        [[nodiscard]] bool redo();
        [[nodiscard]] bool canUndo() const noexcept;
        [[nodiscard]] bool canRedo() const noexcept;
        [[nodiscard]] std::size_t undoCount() const noexcept;
        [[nodiscard]] std::size_t redoCount() const noexcept;

      private:
        struct CellState
        {
            l2d::TileId tile = l2d::EmptyTile;
            l2d::TileFlipFlags flags = l2d::TileFlipFlags::None;
        };

        struct CellDelta
        {
            std::size_t layerIndex = 0u;
            l2d::TileMapCell cell;
            CellState before;
            CellState after;
        };

        struct Command
        {
            std::vector<CellDelta> deltas;
        };

        struct PendingStroke
        {
            std::size_t layerIndex = 0u;
            CellState brush;
            std::vector<CellDelta> deltas;
        };

        [[nodiscard]] bool validLimits() const noexcept;
        [[nodiscard]] TilemapAuthoringError validateCandidate(const l2d::TileMapData& data) const;
        [[nodiscard]] std::optional<CellState> cellState(std::size_t layerIndex,
                                                         l2d::TileMapCell cell) const;
        [[nodiscard]] bool applyCellState(std::size_t layerIndex, l2d::TileMapCell cell,
                                          CellState state);
        [[nodiscard]] bool applyCommand(const Command& command, bool useAfterState);
        [[nodiscard]] bool validSelection() const;
        [[nodiscard]] static bool validFlipFlags(l2d::TileFlipFlags flags) noexcept;
        [[nodiscard]] static bool sameState(CellState left, CellState right) noexcept;
        void publish(l2d::TileMapData data);
        void rebuildTileIds();
        void pushCommand(Command command);
        void setError(TilemapAuthoringError error) noexcept;

        TilemapAuthoringLimits m_limits;
        TilemapAuthoringError m_lastError = TilemapAuthoringError::None;
        l2d::TileMapData m_data;
        bool m_hasMap = false;
        std::vector<l2d::TileId> m_tileIds;
        std::optional<std::size_t> m_selectedLayer;
        l2d::TileId m_selectedTile = l2d::EmptyTile;
        l2d::TileFlipFlags m_selectedFlags = l2d::TileFlipFlags::None;
        std::vector<Command> m_undo;
        std::vector<Command> m_redo;
        std::optional<PendingStroke> m_pendingStroke;
    };

    [[nodiscard]] std::string_view tilemapAuthoringErrorMessage(
        TilemapAuthoringError error) noexcept;
}

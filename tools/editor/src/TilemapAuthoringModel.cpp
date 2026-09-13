#include <Lorenzo2DEditor/TilemapAuthoringModel.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        constexpr std::uint8_t ValidFlipMask =
            static_cast<std::uint8_t>(l2d::TileFlipFlags::Horizontal) |
            static_cast<std::uint8_t>(l2d::TileFlipFlags::Vertical) |
            static_cast<std::uint8_t>(l2d::TileFlipFlags::Diagonal);
    }

    TilemapAuthoringModel::TilemapAuthoringModel(TilemapAuthoringLimits limits) : m_limits(limits)
    {
        if (!validLimits()) m_lastError = TilemapAuthoringError::InvalidConfiguration;
    }

    bool TilemapAuthoringModel::loadTiled(std::istream& input)
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (!validLimits())
        {
            setError(TilemapAuthoringError::InvalidConfiguration);
            return false;
        }

        l2d::TileMapData candidate;
        l2d::TiledJsonImportLimits importLimits;
        importLimits.maxInputBytes = m_limits.maxInputBytes;
        if (!l2d::TiledJsonImporter::load(input, candidate, importLimits))
        {
            setError(TilemapAuthoringError::ImportFailed);
            return false;
        }

        return replace(std::move(candidate));
    }

    bool TilemapAuthoringModel::replace(l2d::TileMapData data)
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (!validLimits())
        {
            setError(TilemapAuthoringError::InvalidConfiguration);
            return false;
        }

        const TilemapAuthoringError validation = validateCandidate(data);
        if (validation != TilemapAuthoringError::None)
        {
            setError(validation);
            return false;
        }

        publish(std::move(data));
        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::hasMap() const noexcept
    {
        return m_hasMap;
    }

    const l2d::TileMapData& TilemapAuthoringModel::data() const noexcept
    {
        return m_data;
    }

    const TilemapAuthoringLimits& TilemapAuthoringModel::limits() const noexcept
    {
        return m_limits;
    }

    TilemapAuthoringError TilemapAuthoringModel::lastError() const noexcept
    {
        return m_lastError;
    }

    const std::vector<l2d::TileId>& TilemapAuthoringModel::tileIds() const noexcept
    {
        return m_tileIds;
    }

    std::optional<std::size_t> TilemapAuthoringModel::selectedLayer() const noexcept
    {
        return m_selectedLayer;
    }

    l2d::TileId TilemapAuthoringModel::selectedTile() const noexcept
    {
        return m_selectedTile;
    }

    l2d::TileFlipFlags TilemapAuthoringModel::selectedFlipFlags() const noexcept
    {
        return m_selectedFlags;
    }

    bool TilemapAuthoringModel::selectLayer(std::size_t layerIndex)
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (!m_hasMap || layerIndex >= m_data.layers().size())
        {
            setError(TilemapAuthoringError::InvalidSelection);
            return false;
        }

        const bool changed = !m_selectedLayer || *m_selectedLayer != layerIndex;
        m_selectedLayer = layerIndex;
        setError(TilemapAuthoringError::None);
        return changed;
    }

    bool TilemapAuthoringModel::selectTile(l2d::TileId tile, l2d::TileFlipFlags flags)
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (!m_hasMap || !validFlipFlags(flags) ||
            (tile == l2d::EmptyTile && flags != l2d::TileFlipFlags::None) ||
            (tile != l2d::EmptyTile && m_data.definition(tile) == nullptr))
        {
            setError(TilemapAuthoringError::InvalidSelection);
            return false;
        }

        const bool changed = m_selectedTile != tile || m_selectedFlags != flags;
        m_selectedTile = tile;
        m_selectedFlags = flags;
        setError(TilemapAuthoringError::None);
        return changed;
    }

    bool TilemapAuthoringModel::paintCell(l2d::TileMapCell cell)
    {
        if (!beginPaintStroke()) return false;

        if (!paintStrokeCell(cell))
        {
            const TilemapAuthoringError error = m_lastError;
            (void)cancelPaintStroke();
            setError(error);
            return false;
        }

        return commitPaintStroke();
    }

    bool TilemapAuthoringModel::beginPaintStroke()
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (!validSelection())
        {
            setError(TilemapAuthoringError::InvalidSelection);
            return false;
        }

        PendingStroke pending;
        pending.layerIndex = *m_selectedLayer;
        pending.brush = {m_selectedTile, m_selectedFlags};
        pending.deltas.reserve(std::min<std::size_t>(m_limits.maxStrokeCellCount, 64u));
        m_pendingStroke = std::move(pending);
        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::paintStrokeCell(l2d::TileMapCell cell)
    {
        if (!m_pendingStroke)
        {
            setError(TilemapAuthoringError::NoOpenStroke);
            return false;
        }

        PendingStroke& pending = *m_pendingStroke;
        const std::optional<CellState> current = cellState(pending.layerIndex, cell);
        if (!current)
        {
            setError(TilemapAuthoringError::MutationRejected);
            return false;
        }
        if (sameState(*current, pending.brush))
        {
            setError(TilemapAuthoringError::None);
            return false;
        }

        auto existing = std::find_if(pending.deltas.begin(), pending.deltas.end(),
                                     [cell](const CellDelta& delta) { return delta.cell == cell; });
        if (existing == pending.deltas.end() &&
            pending.deltas.size() >= m_limits.maxStrokeCellCount)
        {
            setError(TilemapAuthoringError::StrokeCellLimitExceeded);
            return false;
        }

        if (!applyCellState(pending.layerIndex, cell, pending.brush))
        {
            setError(TilemapAuthoringError::MutationRejected);
            return false;
        }

        if (existing == pending.deltas.end())
        {
            pending.deltas.push_back({pending.layerIndex, cell, *current, pending.brush});
        }
        else
        {
            existing->after = pending.brush;
            if (sameState(existing->before, existing->after)) pending.deltas.erase(existing);
        }

        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::commitPaintStroke()
    {
        if (!m_pendingStroke)
        {
            setError(TilemapAuthoringError::NoOpenStroke);
            return false;
        }

        PendingStroke pending = std::move(*m_pendingStroke);
        m_pendingStroke.reset();
        if (pending.deltas.empty())
        {
            setError(TilemapAuthoringError::None);
            return false;
        }

        Command command;
        command.deltas = std::move(pending.deltas);
        pushCommand(std::move(command));
        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::cancelPaintStroke()
    {
        if (!m_pendingStroke)
        {
            setError(TilemapAuthoringError::NoOpenStroke);
            return false;
        }

        const PendingStroke pending = std::move(*m_pendingStroke);
        m_pendingStroke.reset();
        for (auto iterator = pending.deltas.rbegin(); iterator != pending.deltas.rend(); ++iterator)
        {
            if (applyCellState(iterator->layerIndex, iterator->cell, iterator->before)) continue;
            setError(TilemapAuthoringError::MutationRejected);
            return false;
        }

        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::isPainting() const noexcept
    {
        return m_pendingStroke.has_value();
    }

    bool TilemapAuthoringModel::undo()
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (m_undo.empty())
        {
            setError(TilemapAuthoringError::None);
            return false;
        }

        Command command = m_undo.back();
        if (!applyCommand(command, false))
        {
            setError(TilemapAuthoringError::MutationRejected);
            return false;
        }

        m_undo.pop_back();
        m_redo.push_back(std::move(command));
        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::redo()
    {
        if (m_pendingStroke)
        {
            setError(TilemapAuthoringError::OperationInProgress);
            return false;
        }
        if (m_redo.empty())
        {
            setError(TilemapAuthoringError::None);
            return false;
        }

        Command command = m_redo.back();
        if (!applyCommand(command, true))
        {
            setError(TilemapAuthoringError::MutationRejected);
            return false;
        }

        m_redo.pop_back();
        m_undo.push_back(std::move(command));
        setError(TilemapAuthoringError::None);
        return true;
    }

    bool TilemapAuthoringModel::canUndo() const noexcept
    {
        return !m_pendingStroke && !m_undo.empty();
    }

    bool TilemapAuthoringModel::canRedo() const noexcept
    {
        return !m_pendingStroke && !m_redo.empty();
    }

    std::size_t TilemapAuthoringModel::undoCount() const noexcept
    {
        return m_undo.size();
    }

    std::size_t TilemapAuthoringModel::redoCount() const noexcept
    {
        return m_redo.size();
    }

    bool TilemapAuthoringModel::validLimits() const noexcept
    {
        return m_limits.maxInputBytes > 0u && m_limits.maxCellCount > 0u &&
               m_limits.maxCellCount <= l2d::TileMapData::MaximumCellCount &&
               m_limits.maxLayerCount > 0u &&
               m_limits.maxLayerCount <= l2d::TileMapData::MaximumLayerCount &&
               m_limits.maxDefinitionCount > 0u && m_limits.maxObjectCount > 0u &&
               m_limits.maxObjectCount <= l2d::TileMapData::MaximumObjectCount &&
               m_limits.maxTileSlotCount > 0u && m_limits.maxStrokeCellCount > 0u &&
               m_limits.maxHistoryCommandCount > 0u;
    }

    TilemapAuthoringError TilemapAuthoringModel::validateCandidate(
        const l2d::TileMapData& data) const
    {
        if (!data.isValid()) return TilemapAuthoringError::InvalidMap;
        if (data.cellCount() > m_limits.maxCellCount)
            return TilemapAuthoringError::CellLimitExceeded;
        if (data.layers().size() > m_limits.maxLayerCount)
            return TilemapAuthoringError::LayerLimitExceeded;
        if (data.definitions().size() > m_limits.maxDefinitionCount)
            return TilemapAuthoringError::DefinitionLimitExceeded;
        if (data.objects().size() > m_limits.maxObjectCount)
            return TilemapAuthoringError::ObjectLimitExceeded;
        if (!data.layers().empty() &&
            data.cellCount() > m_limits.maxTileSlotCount / data.layers().size())
            return TilemapAuthoringError::TileSlotLimitExceeded;
        return TilemapAuthoringError::None;
    }

    std::optional<TilemapAuthoringModel::CellState> TilemapAuthoringModel::cellState(
        std::size_t layerIndex, l2d::TileMapCell cell) const
    {
        const std::optional<l2d::TileId> tile = m_data.tileAt(layerIndex, cell.row, cell.column);
        const l2d::TileMapLayer* layer = m_data.layer(layerIndex);
        if (!tile || layer == nullptr) return std::nullopt;

        l2d::TileFlipFlags flags = l2d::TileFlipFlags::None;
        if (!layer->flipFlags.empty())
        {
            const std::size_t index = cell.row * m_data.width() + cell.column;
            if (index >= layer->flipFlags.size()) return std::nullopt;
            flags = layer->flipFlags[index];
        }
        return CellState{*tile, flags};
    }

    bool TilemapAuthoringModel::applyCellState(std::size_t layerIndex, l2d::TileMapCell cell,
                                               CellState state)
    {
        return m_data.setTile(layerIndex, cell.row, cell.column, state.tile, state.flags);
    }

    bool TilemapAuthoringModel::applyCommand(const Command& command, bool useAfterState)
    {
        if (useAfterState)
        {
            std::size_t applied = 0u;
            for (const CellDelta& delta : command.deltas)
            {
                if (applyCellState(delta.layerIndex, delta.cell, delta.after))
                {
                    ++applied;
                    continue;
                }
                while (applied > 0u)
                {
                    --applied;
                    const CellDelta& rollback = command.deltas[applied];
                    (void)applyCellState(rollback.layerIndex, rollback.cell, rollback.before);
                }
                return false;
            }
            return true;
        }

        std::size_t applied = 0u;
        for (auto iterator = command.deltas.rbegin(); iterator != command.deltas.rend(); ++iterator)
        {
            if (applyCellState(iterator->layerIndex, iterator->cell, iterator->before))
            {
                ++applied;
                continue;
            }
            while (applied > 0u)
            {
                --applied;
                const CellDelta& rollback = command.deltas[command.deltas.size() - 1u - applied];
                (void)applyCellState(rollback.layerIndex, rollback.cell, rollback.after);
            }
            return false;
        }
        return true;
    }

    bool TilemapAuthoringModel::validSelection() const
    {
        if (!m_hasMap || !m_selectedLayer || *m_selectedLayer >= m_data.layers().size() ||
            !validFlipFlags(m_selectedFlags))
            return false;
        if (m_selectedTile == l2d::EmptyTile) return m_selectedFlags == l2d::TileFlipFlags::None;
        return m_data.definition(m_selectedTile) != nullptr;
    }

    bool TilemapAuthoringModel::validFlipFlags(l2d::TileFlipFlags flags) noexcept
    {
        return (static_cast<std::uint8_t>(flags) & ~ValidFlipMask) == 0u;
    }

    bool TilemapAuthoringModel::sameState(CellState left, CellState right) noexcept
    {
        return left.tile == right.tile && left.flags == right.flags;
    }

    void TilemapAuthoringModel::publish(l2d::TileMapData data)
    {
        m_data = std::move(data);
        m_hasMap = true;
        rebuildTileIds();
        m_selectedLayer = m_data.layers().empty() ? std::nullopt : std::optional<std::size_t>(0u);
        m_selectedTile = l2d::EmptyTile;
        m_selectedFlags = l2d::TileFlipFlags::None;
        m_undo.clear();
        m_redo.clear();
        m_pendingStroke.reset();
    }

    void TilemapAuthoringModel::rebuildTileIds()
    {
        m_tileIds.clear();
        m_tileIds.reserve(m_data.definitions().size());
        for (const auto& definition : m_data.definitions())
            m_tileIds.push_back(definition.first);
        std::sort(m_tileIds.begin(), m_tileIds.end());
    }

    void TilemapAuthoringModel::pushCommand(Command command)
    {
        if (m_undo.size() >= m_limits.maxHistoryCommandCount) m_undo.erase(m_undo.begin());
        m_undo.push_back(std::move(command));
        m_redo.clear();
    }

    void TilemapAuthoringModel::setError(TilemapAuthoringError error) noexcept
    {
        m_lastError = error;
    }

    std::string_view tilemapAuthoringErrorMessage(TilemapAuthoringError error) noexcept
    {
        switch (error)
        {
        case TilemapAuthoringError::None:
            return "no error";
        case TilemapAuthoringError::InvalidConfiguration:
            return "invalid tilemap authoring limits";
        case TilemapAuthoringError::ImportFailed:
            return "Tiled JSON import failed";
        case TilemapAuthoringError::InvalidMap:
            return "tilemap data is invalid";
        case TilemapAuthoringError::CellLimitExceeded:
            return "tilemap cell limit exceeded";
        case TilemapAuthoringError::LayerLimitExceeded:
            return "tilemap layer limit exceeded";
        case TilemapAuthoringError::DefinitionLimitExceeded:
            return "tile definition limit exceeded";
        case TilemapAuthoringError::ObjectLimitExceeded:
            return "tilemap object limit exceeded";
        case TilemapAuthoringError::TileSlotLimitExceeded:
            return "tilemap authored tile-slot limit exceeded";
        case TilemapAuthoringError::InvalidSelection:
            return "invalid tilemap layer or brush selection";
        case TilemapAuthoringError::OperationInProgress:
            return "a tilemap painting gesture is already in progress";
        case TilemapAuthoringError::NoOpenStroke:
            return "no tilemap painting gesture is open";
        case TilemapAuthoringError::StrokeCellLimitExceeded:
            return "tilemap painting gesture cell limit exceeded";
        case TilemapAuthoringError::MutationRejected:
            return "tilemap mutation was rejected";
        }
        return "unknown tilemap authoring error";
    }
}

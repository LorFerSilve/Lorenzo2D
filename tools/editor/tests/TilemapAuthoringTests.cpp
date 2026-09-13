#include <Lorenzo2DEditor/TilemapAuthoringModel.hpp>

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    l2d::TileMapData makeMap(std::size_t width, std::size_t height, std::size_t layerCount,
                             const std::vector<l2d::TileId>& definitionIds)
    {
        l2d::TileMapData data;
        require(data.setDimensions(width, height), "unable to configure tilemap dimensions");
        require(data.setTileSize({16.f, 16.f}), "unable to configure tilemap tile size");

        for (const l2d::TileId id : definitionIds)
        {
            l2d::TileDefinition definition;
            definition.id = id;
            require(data.setDefinition(std::move(definition)), "unable to add tile definition");
        }

        for (std::size_t index = 0u; index < layerCount; ++index)
        {
            l2d::TileMapLayer layer;
            layer.name = "Layer " + std::to_string(index);
            layer.tiles.resize(data.cellCount(), l2d::EmptyTile);
            require(data.addLayer(std::move(layer)), "unable to add tilemap layer");
        }

        require(data.isValid(), "test tilemap is not valid");
        return data;
    }

    l2d::TileFlipFlags flagsAt(const l2d::TileMapData& data, std::size_t layerIndex,
                               l2d::TileMapCell cell)
    {
        const l2d::TileMapLayer* layer = data.layer(layerIndex);
        require(layer != nullptr, "test requested missing tilemap layer");
        if (layer->flipFlags.empty()) return l2d::TileFlipFlags::None;
        return layer->flipFlags[cell.row * data.width() + cell.column];
    }

    void testTiledImportAndTransactionalPublication()
    {
        l2d_editor::TilemapAuthoringModel model;
        require(model.replace(makeMap(1u, 1u, 1u, {9u, 2u, 5u})),
                "unable to publish initial tilemap");
        require(model.tileIds() == std::vector<l2d::TileId>({2u, 5u, 9u}),
                "tile palette IDs are not sorted deterministically");
        require(model.selectedLayer() && *model.selectedLayer() == 0u,
                "first tilemap layer was not selected after publication");
        require(model.selectedTile() == l2d::EmptyTile,
                "tilemap publication did not reset the brush to erase");

        require(model.selectTile(2u), "unable to select initial tile brush");
        require(model.paintCell({0u, 0u}), "unable to create initial tile history entry");
        require(model.undoCount() == 1u, "initial tile history entry was not recorded");

        std::stringstream invalid("{not-json");
        require(!model.loadTiled(invalid), "malformed Tiled JSON unexpectedly loaded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::ImportFailed,
                "malformed Tiled JSON reported the wrong error");
        require(model.data().width() == 1u && model.data().height() == 1u &&
                    model.data().tileAt(0u, 0u, 0u) == 2u && model.undoCount() == 1u,
                "failed Tiled import replaced previously published authoring state");

        const std::string tiled =
            R"({"width":2,"height":2,"tilewidth":16,"tileheight":16,"orientation":"orthogonal","infinite":false,"tilesets":[{"firstgid":1,"tilecount":1,"columns":1,"tilewidth":16,"tileheight":16,"image":"tiles.png"}],"layers":[{"type":"tilelayer","name":"ground","width":2,"height":2,"data":[1,0,0,1]}]})";
        std::stringstream input(tiled);
        require(model.loadTiled(input), "valid Tiled JSON did not load through public importer");
        require(model.data().width() == 2u && model.data().height() == 2u,
                "Tiled import published wrong dimensions");
        require(model.data().layers().size() == 1u && model.data().definitions().size() == 1u,
                "Tiled import published wrong layer or definition count");
        require(model.data().tileAt(0u, 0u, 0u) == 1u &&
                    model.data().tileAt(0u, 1u, 1u) == 1u,
                "Tiled import published wrong cell values");
        require(model.undoCount() == 0u && model.redoCount() == 0u,
                "successful map replacement retained old tile history");

        l2d_editor::TilemapAuthoringLimits inputLimits;
        inputLimits.maxInputBytes = 8u;
        l2d_editor::TilemapAuthoringModel inputBounded(inputLimits);
        std::stringstream oversizedInput(tiled);
        require(!inputBounded.loadTiled(oversizedInput),
                "Tiled input above editor byte limit unexpectedly loaded");
        require(inputBounded.lastError() == l2d_editor::TilemapAuthoringError::ImportFailed,
                "Tiled input byte limit reported the wrong error");
        require(!inputBounded.hasMap(), "failed bounded Tiled import published a partial map");
    }

    void testAuthoringLimitsAreTransactional()
    {
        l2d_editor::TilemapAuthoringLimits limits;
        limits.maxCellCount = 4u;
        limits.maxLayerCount = 2u;
        limits.maxDefinitionCount = 2u;
        limits.maxObjectCount = 1u;
        limits.maxTileSlotCount = 6u;
        l2d_editor::TilemapAuthoringModel model(limits);
        require(model.replace(makeMap(2u, 2u, 1u, {1u})),
                "exact-limit baseline tilemap was rejected");

        require(!model.replace(makeMap(3u, 2u, 1u, {1u})),
                "over-limit cell count unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::CellLimitExceeded,
                "cell limit violation reported the wrong error");
        require(model.data().width() == 2u && model.data().height() == 2u,
                "cell-limit failure replaced previous map");

        require(!model.replace(makeMap(1u, 1u, 3u, {1u})),
                "over-limit layer count unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::LayerLimitExceeded,
                "layer limit violation reported the wrong error");

        require(!model.replace(makeMap(1u, 1u, 1u, {1u, 2u, 3u})),
                "over-limit tile definition count unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::DefinitionLimitExceeded,
                "definition limit violation reported the wrong error");

        l2d::TileMapData objectHeavy = makeMap(1u, 1u, 1u, {1u});
        l2d::TileMapObject firstObject;
        firstObject.id = 1u;
        l2d::TileMapObject secondObject;
        secondObject.id = 2u;
        require(objectHeavy.addObject(firstObject) && objectHeavy.addObject(secondObject),
                "unable to create object-limit fixture");
        require(!model.replace(std::move(objectHeavy)),
                "over-limit tilemap object count unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::ObjectLimitExceeded,
                "object limit violation reported the wrong error");

        require(!model.replace(makeMap(2u, 2u, 2u, {1u})),
                "over-limit authored tile-slot count unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::TileSlotLimitExceeded,
                "tile-slot limit violation reported the wrong error");

        require(model.data().width() == 2u && model.data().height() == 2u &&
                    model.data().layers().size() == 1u,
                "authoring-limit failures replaced the previously published map");

        l2d_editor::TilemapAuthoringLimits invalidLimits;
        invalidLimits.maxStrokeCellCount = 0u;
        l2d_editor::TilemapAuthoringModel invalid(invalidLimits);
        require(!invalid.replace(makeMap(1u, 1u, 1u, {1u})),
                "invalid authoring configuration unexpectedly accepted a map");
        require(invalid.lastError() == l2d_editor::TilemapAuthoringError::InvalidConfiguration,
                "invalid authoring configuration reported the wrong error");
    }

    void testSingleCellPaintingSelectionAndUndoRedo()
    {
        l2d_editor::TilemapAuthoringModel model;
        require(model.replace(makeMap(3u, 2u, 2u, {2u, 1u})),
                "unable to publish painting test map");
        require(model.selectLayer(1u), "unable to select second tile layer");
        require(model.selectTile(2u, l2d::TileFlipFlags::Horizontal),
                "unable to select textured tile brush");
        require(model.paintCell({2u, 1u}), "single-cell tile paint failed");
        require(model.data().tileAt(1u, 1u, 2u) == 2u,
                "single-cell paint wrote the wrong tile ID");
        require(flagsAt(model.data(), 1u, {2u, 1u}) == l2d::TileFlipFlags::Horizontal,
                "single-cell paint wrote the wrong flip flags");
        require(model.undoCount() == 1u && model.redoCount() == 0u,
                "single-cell paint did not create exactly one history command");

        require(model.undo(), "single-cell tile undo failed");
        require(model.data().tileAt(1u, 1u, 2u) == l2d::EmptyTile,
                "single-cell undo did not restore the previous tile");
        require(flagsAt(model.data(), 1u, {2u, 1u}) == l2d::TileFlipFlags::None,
                "single-cell undo did not restore previous flip flags");
        require(model.redo(), "single-cell tile redo failed");
        require(model.data().tileAt(1u, 1u, 2u) == 2u,
                "single-cell redo did not restore painted tile");

        const std::size_t historyBeforeFailure = model.undoCount();
        require(!model.paintCell({3u, 0u}), "out-of-bounds tile paint unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::MutationRejected,
                "out-of-bounds tile paint reported the wrong error");
        require(model.undoCount() == historyBeforeFailure,
                "rejected tile paint entered undo history");
        require(!model.selectLayer(9u), "invalid tile layer selection unexpectedly succeeded");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::InvalidSelection,
                "invalid tile layer selection reported the wrong error");
        require(!model.selectTile(999u), "unknown tile definition unexpectedly became brush");
        require(!model.selectTile(l2d::EmptyTile, l2d::TileFlipFlags::Vertical),
                "erase brush unexpectedly accepted flip flags");

        require(model.selectTile(l2d::EmptyTile), "unable to select erase brush");
        require(model.paintCell({2u, 1u}), "erase brush failed to clear painted tile");
        require(model.data().tileAt(1u, 1u, 2u) == l2d::EmptyTile,
                "erase brush did not clear tile");
        require(model.undo(), "erase undo failed");
        require(model.data().tileAt(1u, 1u, 2u) == 2u,
                "erase undo did not restore tile");
    }

    void testContinuousPaintingIsBoundedAndCoalesced()
    {
        l2d_editor::TilemapAuthoringLimits limits;
        limits.maxStrokeCellCount = 2u;
        limits.maxHistoryCommandCount = 4u;
        l2d_editor::TilemapAuthoringModel model(limits);
        require(model.replace(makeMap(2u, 2u, 1u, {1u, 2u})),
                "unable to publish stroke test map");
        require(model.selectTile(1u), "unable to select stroke brush");
        require(model.beginPaintStroke(), "unable to begin tile paint stroke");
        require(model.paintStrokeCell({0u, 0u}), "first stroke cell failed");
        require(model.paintStrokeCell({1u, 0u}), "second stroke cell failed");
        require(!model.paintStrokeCell({0u, 1u}),
                "stroke exceeded configured unique-cell limit");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::StrokeCellLimitExceeded,
                "stroke cell limit reported the wrong error");
        require(model.data().tileAt(0u, 1u, 0u) == l2d::EmptyTile,
                "over-limit stroke mutated rejected cell");
        require(!model.selectLayer(0u), "selection changed during open paint stroke");
        require(model.lastError() == l2d_editor::TilemapAuthoringError::OperationInProgress,
                "selection during stroke reported the wrong error");
        require(model.commitPaintStroke(), "bounded tile paint stroke did not commit");
        require(model.undoCount() == 1u,
                "multiple stroke updates did not coalesce into one history command");

        require(model.undo(), "coalesced tile stroke undo failed");
        require(model.data().tileAt(0u, 0u, 0u) == l2d::EmptyTile &&
                    model.data().tileAt(0u, 0u, 1u) == l2d::EmptyTile,
                "coalesced tile stroke undo did not restore all touched cells");
        require(model.redoCount() == 1u, "coalesced tile stroke undo did not create redo entry");

        require(model.beginPaintStroke(), "unable to begin cancellable stroke");
        require(model.paintStrokeCell({0u, 1u}), "cancellable stroke update failed");
        require(model.cancelPaintStroke(), "tile paint stroke cancellation failed");
        require(model.data().tileAt(0u, 1u, 0u) == l2d::EmptyTile,
                "cancelled tile stroke did not restore gesture-start cell state");
        require(model.redoCount() == 1u,
                "cancelled tile stroke invalidated pre-existing redo history");
        require(model.redo(), "redo after cancelled tile stroke failed");
        require(model.data().tileAt(0u, 0u, 0u) == 1u &&
                    model.data().tileAt(0u, 0u, 1u) == 1u,
                "redo after cancelled tile stroke restored wrong cells");

        require(model.beginPaintStroke(), "unable to begin no-op stroke");
        require(!model.paintStrokeCell({0u, 0u}),
                "painting identical tile state unexpectedly reported a mutation");
        const std::size_t historyBeforeNoOp = model.undoCount();
        require(!model.commitPaintStroke(), "no-op stroke unexpectedly created history");
        require(model.undoCount() == historyBeforeNoOp,
                "no-op stroke changed history depth");
    }

    void testHistoryDepthIsBounded()
    {
        l2d_editor::TilemapAuthoringLimits limits;
        limits.maxHistoryCommandCount = 2u;
        l2d_editor::TilemapAuthoringModel model(limits);
        require(model.replace(makeMap(3u, 1u, 1u, {1u})),
                "unable to publish bounded-history map");
        require(model.selectTile(1u), "unable to select bounded-history brush");
        require(model.paintCell({0u, 0u}), "first bounded-history paint failed");
        require(model.paintCell({1u, 0u}), "second bounded-history paint failed");
        require(model.paintCell({2u, 0u}), "third bounded-history paint failed");
        require(model.undoCount() == 2u, "tilemap history did not enforce configured command limit");

        require(model.undo(), "first bounded-history undo failed");
        require(model.undo(), "second bounded-history undo failed");
        require(!model.undo(), "evicted tilemap history entry was unexpectedly retained");
        require(model.data().tileAt(0u, 0u, 0u) == 1u &&
                    model.data().tileAt(0u, 0u, 1u) == l2d::EmptyTile &&
                    model.data().tileAt(0u, 0u, 2u) == l2d::EmptyTile,
                "bounded tilemap history evicted or restored the wrong command");
    }
}

int main()
{
    try
    {
        testTiledImportAndTransactionalPublication();
        testAuthoringLimitsAreTransactional();
        testSingleCellPaintingSelectionAndUndoRedo();
        testContinuousPaintingIsBoundedAndCoalesced();
        testHistoryDepthIsBounded();
        std::cout << "Lorenzo2D editor tilemap authoring tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Lorenzo2D editor tilemap authoring test failure: " << exception.what() << '\n';
        return 1;
    }
}

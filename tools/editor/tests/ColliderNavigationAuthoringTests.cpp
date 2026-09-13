#include <Lorenzo2DEditor/ColliderNavigationAuthoringModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    bool near(float lhs, float rhs)
    {
        return std::abs(lhs - rhs) < 0.0001f;
    }

    bool near(sf::Vector2f lhs, sf::Vector2f rhs)
    {
        return near(lhs.x, rhs.x) && near(lhs.y, rhs.y);
    }

    l2d::LevelDocument makeLevel()
    {
        l2d::Prefab prefab;
        prefab.name = "Collision subject";
        prefab.transform.position = {30.f, 40.f};
        prefab.boxCollider = l2d::BoxColliderPrefab{};
        prefab.boxCollider->size = {80.f, 32.f};
        prefab.boxCollider->properties.offset = {4.f, -3.f};
        prefab.circleCollider = l2d::CircleColliderPrefab{};
        prefab.circleCollider->radius = 12.f;

        l2d::LevelDocument level;
        level.name = "Collider navigation authoring";
        level.objects.push_back(prefab);
        return level;
    }

    l2d::NavigationGrid2D makeGrid(sf::Vector2u size = {4u, 3u})
    {
        l2d::NavigationGridConfig2D config;
        config.size = size;
        config.cellSize = {16.f, 20.f};
        config.origin = {-8.f, 10.f};
        return l2d::NavigationGrid2D(config);
    }

    void testColliderSnapshotAndDeterministicOrder()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "collider snapshot document setup failed");
        require(document.selectObject(1u), "collider snapshot selection failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ColliderNavigationAuthoringModel model(document, history);

        const auto snapshot = model.colliderSnapshot();
        require(snapshot.has_value(), "selected collider snapshot is missing");
        require(snapshot->objectId == 1u, "collider snapshot object identity is incorrect");
        require(near(snapshot->transform.position, {30.f, 40.f}),
                "collider snapshot transform is incorrect");
        require(snapshot->colliders.size() == 2u, "collider snapshot count is incorrect");
        require(snapshot->colliders[0].kind == l2d_editor::ColliderAuthoringKind::Box,
                "box collider did not appear first in deterministic order");
        require(snapshot->colliders[1].kind == l2d_editor::ColliderAuthoringKind::Circle,
                "circle collider did not appear second in deterministic order");
        require(near(snapshot->colliders[0].boxSize, {80.f, 32.f}),
                "box collider size snapshot is incorrect");
        require(near(snapshot->colliders[0].properties.offset, {4.f, -3.f}),
                "box collider offset snapshot is incorrect");
        require(near(snapshot->colliders[1].radius, 12.f),
                "circle collider radius snapshot is incorrect");
    }

    void testColliderEditsUseSharedDocumentHistory()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "collider edit document setup failed");
        require(document.selectObject(1u), "collider edit selection failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ColliderNavigationAuthoringModel model(document, history);

        require(model.setColliderOffset(l2d_editor::ColliderAuthoringKind::Box, {9.f, 7.f}),
                "valid collider offset edit failed");
        require(model.setBoxSize({120.f, 44.f}), "valid box collider resize failed");
        require(model.setCircleRadius(18.f), "valid circle collider resize failed");
        require(history.undoCount() == 3u, "collider edits did not enter shared history exactly once");

        const l2d::Prefab& edited = document.findObject(1u)->prefab;
        require(near(edited.boxCollider->properties.offset, {9.f, 7.f}),
                "box collider offset edit was not published");
        require(near(edited.boxCollider->size, {120.f, 44.f}),
                "box collider resize was not published");
        require(near(edited.circleCollider->radius, 18.f),
                "circle collider resize was not published");

        require(!model.setBoxSize({-1.f, 10.f}),
                "invalid box collider size was published");
        require(history.undoCount() == 3u,
                "rejected collider edit polluted document history");
        require(near(document.findObject(1u)->prefab.boxCollider->size, {120.f, 44.f}),
                "rejected collider edit changed published state");

        require(history.undo(document), "collider edit undo failed");
        require(near(document.findObject(1u)->prefab.circleCollider->radius, 12.f),
                "collider undo did not restore exact runtime prefab state");
        require(history.redo(document), "collider edit redo failed");
        require(near(document.findObject(1u)->prefab.circleCollider->radius, 18.f),
                "collider redo did not restore edited runtime prefab state");
    }

    void testNavigationOverlayAndCellDeltaHistory()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "navigation document setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ColliderNavigationAuthoringModel model(document, history);

        require(model.setNavigationGrid(makeGrid()), "valid navigation grid publication failed");
        const auto overlay = model.navigationOverlay();
        require(overlay.has_value(), "navigation overlay snapshot is missing");
        require(overlay->cells.size() == 12u, "navigation overlay cell count is incorrect");
        require(overlay->cells.front().position == sf::Vector2i(0, 0),
                "navigation overlay order does not begin at the first grid cell");
        require(near(overlay->cells.front().worldCenter, {0.f, 20.f}),
                "navigation overlay world center is incorrect");

        require(model.setNavigationWalkable({1, 1}, false),
                "navigation walkability edit failed");
        require(model.setNavigationTraversalCost({1, 1}, 2.5f),
                "navigation traversal-cost edit failed");
        require(model.navigationUndoCount() == 2u,
                "navigation edits did not create bounded cell-delta history");
        require(model.navigationRedoCount() == 0u, "unexpected navigation redo entries exist");

        const auto edited = model.navigationGrid()->cell({1, 1});
        require(edited && !edited->walkable && near(edited->traversalCost, 2.5f),
                "navigation edits were not published");
        require(!model.setNavigationTraversalCost({1, 1}, 2.5f),
                "navigation no-op unexpectedly entered history");
        require(!model.setNavigationTraversalCost({99, 99}, 3.f),
                "out-of-bounds navigation edit unexpectedly succeeded");
        require(model.navigationUndoCount() == 2u,
                "rejected navigation edits polluted history");

        require(model.undoNavigation(), "navigation traversal-cost undo failed");
        require(near(model.navigationGrid()->cell({1, 1})->traversalCost, 1.f),
                "navigation undo did not restore prior traversal cost");
        require(model.undoNavigation(), "navigation walkability undo failed");
        require(model.navigationGrid()->cell({1, 1})->walkable,
                "navigation undo did not restore prior walkability");
        require(model.navigationRedoCount() == 2u,
                "navigation undo did not populate redo history");
        require(model.redoNavigation(), "navigation walkability redo failed");
        require(!model.navigationGrid()->cell({1, 1})->walkable,
                "navigation redo did not restore edited walkability");
    }

    void testNavigationPublicationIsBoundedAndTransactional()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "navigation limit document setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ColliderNavigationAuthoringModel model(document, history);

        require(model.setNavigationGrid(makeGrid({2u, 2u})),
                "navigation limit baseline publication failed");
        require(model.setNavigationWalkable({0, 0}, false),
                "navigation limit baseline history setup failed");

        l2d::NavigationGrid2D oversized;
        l2d::NavigationGridConfig2D config;
        config.size = {513u, 512u};
        require(oversized.reset(config), "runtime rejected oversized editor-test grid");
        require(oversized.cellCount() >
                    l2d_editor::ColliderNavigationAuthoringModel::MaximumNavigationCells,
                "oversized test grid does not exceed editor limit");
        require(!model.setNavigationGrid(std::move(oversized)),
                "over-limit navigation grid was published");
        require(model.navigationGrid()->cellCount() == 4u,
                "failed navigation publication replaced prior grid");
        require(model.navigationUndoCount() == 1u,
                "failed navigation publication cleared prior edit history");

        model.clearNavigationGrid();
        require(!model.hasNavigationGrid(), "navigation clear did not remove the grid");
        require(model.navigationUndoCount() == 0u && model.navigationRedoCount() == 0u,
                "navigation clear did not clear cell-delta history");
    }
}

int main()
{
    try
    {
        testColliderSnapshotAndDeterministicOrder();
        testColliderEditsUseSharedDocumentHistory();
        testNavigationOverlayAndCellDeltaHistory();
        testNavigationPublicationIsBoundedAndTransactional();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Collider/navigation authoring regression failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "Collider/navigation authoring regressions passed.\n";
    return 0;
}

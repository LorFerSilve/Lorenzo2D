#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/ViewportTransformModel.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

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

    l2d::Prefab makePrefab(std::string name, sf::Vector2f position)
    {
        l2d::Prefab prefab;
        prefab.name = std::move(name);
        prefab.transform.position = position;
        return prefab;
    }

    l2d::LevelDocument makeLevel()
    {
        l2d::LevelDocument level;
        level.name = "Viewport Test";
        level.objects.push_back(makePrefab("First", {10.f, 20.f}));
        level.objects.push_back(makePrefab("Second", {-30.f, 5.f}));
        return level;
    }

    void configureViewport(l2d_editor::ViewportTransformModel& viewport)
    {
        require(viewport.setViewport({{0.f, 0.f}, {400.f, 300.f}}), "viewport bounds setup failed");
        require(viewport.setView({0.f, 0.f}, 1.f), "viewport view setup failed");
    }

    void testViewportMappingAndValidation()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "mapping document setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);

        require(viewport.setViewport({{100.f, 50.f}, {400.f, 300.f}}),
                "valid viewport bounds were rejected");
        require(viewport.setView({10.f, -20.f}, 2.f), "valid viewport view was rejected");

        require(near(viewport.worldToViewport({10.f, -20.f}), {300.f, 200.f}),
                "world center did not map to viewport center");
        require(near(viewport.worldToViewport({15.f, -15.f}), {310.f, 210.f}),
                "world-to-viewport mapping is incorrect");
        require(near(viewport.viewportToWorld({310.f, 210.f}), {15.f, -15.f}),
                "viewport-to-world mapping is incorrect");

        require(!viewport.setViewport({{0.f, 0.f}, {0.f, 100.f}}),
                "zero-width viewport was accepted");
        require(!viewport.setView({0.f, 0.f}, 0.f), "zero viewport zoom was accepted");
        require(
            !viewport.setView({0.f, 0.f}, l2d_editor::ViewportTransformModel::MaximumZoom * 2.f),
            "out-of-range viewport zoom was accepted");
        require(!viewport.setView({std::numeric_limits<float>::quiet_NaN(), 0.f}, 1.f),
                "non-finite viewport center was accepted");

        require(near(viewport.worldToViewport({10.f, -20.f}), {300.f, 200.f}),
                "rejected viewport configuration changed valid mapping state");
    }

    void testSelectionBoundHandleAndDragStart()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "selection document setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configureViewport(viewport);

        require(!viewport.snapshot(), "viewport exposed a gizmo without selection");
        require(!viewport.beginTranslationDrag({210.f, 170.f}),
                "viewport drag began without selection");

        require(document.selectObject(1u), "viewport selection setup failed");
        const auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "selected viewport snapshot is missing");
        require(snapshot->objectId == 1u, "viewport snapshot identity is incorrect");
        require(near(snapshot->gizmoPosition, {210.f, 170.f}),
                "selected gizmo screen position is incorrect");
        require(viewport.hitTestSelectedHandle({210.f, 170.f}), "gizmo center did not hit-test");
        require(!viewport.hitTestSelectedHandle({250.f, 170.f}),
                "distant point unexpectedly hit the gizmo");
        require(!viewport.beginTranslationDrag({250.f, 170.f}),
                "translation drag began outside the gizmo");

        require(viewport.beginTranslationDrag({210.f, 170.f}),
                "translation drag did not begin on the selected gizmo");
        require(viewport.isDragging(), "viewport did not report active drag state");
        require(history.hasOpenCoalescedCommand(), "drag did not open coalesced history state");
        require(!viewport.setView({5.f, 5.f}, 1.f), "view changed during active drag");
        require(!viewport.setViewport({{0.f, 0.f}, {800.f, 600.f}}),
                "viewport bounds changed during active drag");
        require(viewport.cancelTranslationDrag(), "translation drag cancellation failed");
        require(!viewport.isDragging(), "drag state remained active after cancellation");
        require(!history.hasOpenCoalescedCommand(),
                "history gesture remained open after cancellation");
    }

    void testContinuousDragCoalescesIntoOneUndoCommand()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "coalescing document setup failed");
        require(document.selectObject(1u), "coalescing selection setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configureViewport(viewport);

        l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        initial.rotation = 17.f;
        initial.scale = {2.f, 0.5f};
        require(document.setObjectTransform(1u, initial), "coalescing transform setup failed");

        const sf::Vector2f handle = viewport.worldToViewport(initial.position);
        require(viewport.beginTranslationDrag(handle), "coalesced drag begin failed");
        require(viewport.updateTranslationDrag({handle.x + 5.f, handle.y}),
                "first coalesced drag update failed");
        require(viewport.updateTranslationDrag({handle.x + 15.f, handle.y + 10.f}),
                "second coalesced drag update failed");
        require(viewport.updateTranslationDrag({handle.x + 25.f, handle.y - 4.f}),
                "third coalesced drag update failed");

        require(history.undoCount() == 0u,
                "in-progress drag emitted completed history entries per pointer update");
        require(history.hasOpenCoalescedCommand(), "coalesced drag lost pending history state");
        require(!history.execute(document, "Forbidden nested command",
                                 [](l2d_editor::EditorDocument& editor)
                                 { return editor.renameObject(2u, "Nested"); }),
                "regular command executed inside an open coalesced gesture");
        require(!history.undo(document), "undo executed inside an open coalesced gesture");
        require(!history.redo(document), "redo executed inside an open coalesced gesture");

        const l2d::TransformState live = document.findObject(1u)->prefab.transform;
        require(near(live.position, {35.f, 16.f}), "drag did not publish expected live position");
        require(near(live.rotation, initial.rotation) && near(live.scale, initial.scale),
                "translation drag modified rotation or scale");

        require(viewport.endTranslationDrag(), "coalesced drag commit failed");
        require(history.undoCount() == 1u, "drag did not produce exactly one undo command");
        require(history.undoLabel() == "Move object in viewport",
                "drag history label is incorrect");
        require(!history.hasOpenCoalescedCommand(), "drag history remained open after commit");

        require(history.undo(document), "coalesced drag undo failed");
        const l2d::TransformState undone = document.findObject(1u)->prefab.transform;
        require(near(undone.position, initial.position),
                "drag undo did not restore start position");
        require(near(undone.rotation, initial.rotation) && near(undone.scale, initial.scale),
                "drag undo did not restore exact transform state");

        require(history.redo(document), "coalesced drag redo failed");
        const l2d::TransformState redone = document.findObject(1u)->prefab.transform;
        require(near(redone.position, live.position), "drag redo did not restore final position");
    }

    void testCancellationAndNoOpDragPreserveHistory()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "cancellation document setup failed");
        require(document.selectObject(1u), "cancellation selection setup failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configureViewport(viewport);

        require(history.execute(document, "Rename second", [](l2d_editor::EditorDocument& editor)
                                { return editor.renameObject(2u, "Renamed second"); }),
                "redo preservation setup command failed");
        require(history.undo(document), "redo preservation setup undo failed");
        require(history.redoCount() == 1u, "redo preservation setup is incorrect");

        const l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        const sf::Vector2f handle = viewport.worldToViewport(initial.position);
        require(viewport.beginTranslationDrag(handle), "cancel drag begin failed");
        require(viewport.updateTranslationDrag({handle.x + 40.f, handle.y + 20.f}),
                "cancel drag update failed");
        require(viewport.cancelTranslationDrag(), "cancel drag rollback failed");
        require(near(document.findObject(1u)->prefab.transform.position, initial.position),
                "cancelled drag did not restore start transform");
        require(history.undoCount() == 0u, "cancelled drag entered undo history");
        require(history.redoCount() == 1u, "cancelled drag invalidated existing redo history");

        require(viewport.beginTranslationDrag(handle), "no-op drag begin failed");
        require(viewport.updateTranslationDrag({handle.x + 25.f, handle.y}),
                "no-op drag outward update failed");
        require(viewport.updateTranslationDrag(handle), "no-op drag return update failed");
        require(!viewport.endTranslationDrag(), "net-zero drag unexpectedly committed history");
        require(near(document.findObject(1u)->prefab.transform.position, initial.position),
                "net-zero drag did not restore exact start transform");
        require(history.undoCount() == 0u, "net-zero drag entered undo history");
        require(history.redoCount() == 1u, "net-zero drag invalidated redo history");
    }

    void testSelectionChangeCancelsExclusiveGesture()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "selection-change document setup failed");
        require(document.selectObject(1u), "selection-change initial selection failed");
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configureViewport(viewport);

        const l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        const sf::Vector2f handle = viewport.worldToViewport(initial.position);
        require(viewport.beginTranslationDrag(handle), "selection-change drag begin failed");
        require(viewport.updateTranslationDrag({handle.x + 10.f, handle.y + 5.f}),
                "selection-change drag update failed");
        require(document.selectObject(2u), "mid-drag selection change failed");
        require(!viewport.updateTranslationDrag({handle.x + 20.f, handle.y + 10.f}),
                "drag continued after selection identity changed");

        require(!viewport.isDragging(), "selection change did not cancel viewport drag");
        require(!history.hasOpenCoalescedCommand(),
                "selection change did not close coalesced history state");
        require(document.selectedObject() == 1u,
                "selection-change cancellation did not restore gesture-start selection");
        require(near(document.findObject(1u)->prefab.transform.position, initial.position),
                "selection-change cancellation did not restore start transform");
        require(history.undoCount() == 0u, "cancelled selection-change drag entered history");
    }

    void testRejectedCoalescedUpdateRollsBackLocally()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "rejected-update document setup failed");
        l2d_editor::EditorCommandHistory history;

        require(history.beginCoalescedCommand(document, "Rejected gesture"),
                "rejected-update coalesced begin failed");
        require(!history.updateCoalescedCommand(document,
                                                [](l2d_editor::EditorDocument& editor)
                                                {
                                                    (void)editor.renameObject(1u, "Temporary");
                                                    return false;
                                                }),
                "rejected coalesced update unexpectedly succeeded");
        require(document.findObject(1u)->prefab.name == "First",
                "rejected coalesced update changed document state");
        require(history.hasOpenCoalescedCommand(),
                "rejected coalesced update unexpectedly closed gesture");
        require(!history.commitCoalescedCommand(document),
                "gesture without successful updates entered history");
        require(!history.hasOpenCoalescedCommand(),
                "empty coalesced gesture remained open after commit attempt");
        require(history.undoCount() == 0u, "empty coalesced gesture entered undo history");
    }
}

int main()
{
    try
    {
        testViewportMappingAndValidation();
        testSelectionBoundHandleAndDragStart();
        testContinuousDragCoalescesIntoOneUndoCommand();
        testCancellationAndNoOpDragPreserveHistory();
        testSelectionChangeCancelsExclusiveGesture();
        testRejectedCoalescedUpdateRollsBackLocally();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Viewport transform regression failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "Viewport transform regression passed\n";
    return 0;
}

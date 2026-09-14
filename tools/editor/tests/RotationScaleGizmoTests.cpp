#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/ViewportTransformModel.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cmath>
#include <iostream>
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
        return std::abs(lhs - rhs) < 0.001f;
    }

    bool near(sf::Vector2f lhs, sf::Vector2f rhs)
    {
        return near(lhs.x, rhs.x) && near(lhs.y, rhs.y);
    }

    l2d::LevelDocument makeLevel()
    {
        l2d::Prefab first;
        first.name = "First";
        first.transform.position = {10.f, 20.f};
        first.transform.rotation = 15.f;
        first.transform.scale = {2.f, 3.f};

        l2d::Prefab second;
        second.name = "Second";
        second.transform.position = {-30.f, 5.f};

        l2d::LevelDocument level;
        level.name = "Rotation Scale Gizmo Test";
        level.objects.push_back(std::move(first));
        level.objects.push_back(std::move(second));
        return level;
    }

    void configure(l2d_editor::EditorDocument& document,
                   l2d_editor::ViewportTransformModel& viewport)
    {
        require(document.replace(makeLevel()), "document setup failed");
        require(document.selectObject(1u), "selection setup failed");
        require(viewport.setViewport({{0.f, 0.f}, {400.f, 300.f}}), "viewport setup failed");
        require(viewport.setView({0.f, 0.f}, 1.f), "view setup failed");
    }

    void testHandleGeometryAndHitTesting()
    {
        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configure(document, viewport);

        const auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "snapshot missing");
        require(near(snapshot->gizmoPosition, {210.f, 170.f}), "gizmo center is incorrect");
        require(near(snapshot->rotationHandlePosition, {210.f, 106.f}),
                "rotation handle position is incorrect");
        require(near(snapshot->scaleXHandlePosition, {262.f, 170.f}),
                "X scale handle position is incorrect");
        require(near(snapshot->scaleYHandlePosition, {210.f, 222.f}),
                "Y scale handle position is incorrect");

        const float diagonal = l2d_editor::ViewportTransformModel::ScaleHandleDistance *
                               0.70710678118654752440f;
        require(near(snapshot->scaleUniformHandlePosition, {210.f + diagonal, 170.f + diagonal}),
                "uniform scale handle position is incorrect");

        require(viewport.hitTestRotationHandle(snapshot->rotationHandlePosition),
                "rotation handle did not hit-test");
        require(viewport.hitTestScaleHandle(l2d_editor::ViewportScaleHandle::X,
                                            snapshot->scaleXHandlePosition),
                "X scale handle did not hit-test");
        require(viewport.hitTestScaleHandle(l2d_editor::ViewportScaleHandle::Y,
                                            snapshot->scaleYHandlePosition),
                "Y scale handle did not hit-test");
        require(viewport.hitTestScaleHandle(l2d_editor::ViewportScaleHandle::Uniform,
                                            snapshot->scaleUniformHandlePosition),
                "uniform scale handle did not hit-test");
        require(!viewport.hitTestRotationHandle(snapshot->gizmoPosition),
                "translation center unexpectedly hit rotation handle");
        require(!viewport.hitTestScaleHandle(l2d_editor::ViewportScaleHandle::X,
                                             snapshot->gizmoPosition),
                "translation center unexpectedly hit scale handle");
    }

    void testRotationCoalescesAndSupportsMultipleTurns()
    {
        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configure(document, viewport);

        const l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        const auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "rotation snapshot missing");

        require(viewport.beginRotationDrag(snapshot->rotationHandlePosition),
                "rotation drag did not begin");
        require(viewport.dragKind() == l2d_editor::ViewportTransformDragKind::Rotation,
                "rotation drag kind is incorrect");

        const sf::Vector2f center = snapshot->gizmoPosition;
        const float radius = l2d_editor::ViewportTransformModel::RotationHandleDistance;
        require(viewport.updateRotationDrag({center.x + radius, center.y}),
                "rotation quarter-turn update failed");
        require(viewport.updateRotationDrag({center.x, center.y + radius}),
                "rotation half-turn update failed");
        require(viewport.updateRotationDrag({center.x - radius, center.y}),
                "rotation three-quarter-turn update failed");
        require(viewport.updateRotationDrag({center.x, center.y - radius}),
                "rotation full-turn update failed");
        require(viewport.updateRotationDrag({center.x + radius, center.y}),
                "rotation beyond one full turn failed");

        const l2d::TransformState live = document.findObject(1u)->prefab.transform;
        require(near(live.rotation, initial.rotation + 450.f),
                "rotation gesture did not accumulate wrapped pointer angles continuously");
        require(near(live.position, initial.position) && near(live.scale, initial.scale),
                "rotation gesture changed position or scale");
        require(history.undoCount() == 0u, "live rotation emitted multiple undo entries");

        require(viewport.endRotationDrag(), "rotation gesture commit failed");
        require(history.undoCount() == 1u, "rotation gesture was not one undo command");
        require(history.undoLabel() == "Rotate object in viewport",
                "rotation history label is incorrect");

        require(history.undo(document), "rotation undo failed");
        require(near(document.findObject(1u)->prefab.transform.rotation, initial.rotation),
                "rotation undo did not restore the start value");
        require(history.redo(document), "rotation redo failed");
        require(near(document.findObject(1u)->prefab.transform.rotation, live.rotation),
                "rotation redo did not restore the final value");
    }

    void testAxisAndUniformScaleCoalesce()
    {
        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configure(document, viewport);

        const l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "scale snapshot missing");

        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::X,
                                        snapshot->scaleXHandlePosition),
                "X scale drag did not begin");
        require(viewport.updateScaleDrag(
                    {snapshot->gizmoPosition.x +
                         l2d_editor::ViewportTransformModel::ScaleHandleDistance * 1.5f,
                     snapshot->gizmoPosition.y}),
                "X scale update failed");
        require(viewport.endScaleDrag(), "X scale commit failed");

        l2d::TransformState scaled = document.findObject(1u)->prefab.transform;
        require(near(scaled.scale, {3.f, 3.f}), "X scale gesture changed the wrong components");
        require(near(scaled.position, initial.position) && near(scaled.rotation, initial.rotation),
                "X scale gesture changed position or rotation");
        require(history.undoCount() == 1u, "X scale gesture was not one undo command");

        snapshot = viewport.snapshot();
        require(snapshot.has_value(), "post-X-scale snapshot missing");
        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::Y,
                                        snapshot->scaleYHandlePosition),
                "Y scale drag did not begin");
        require(viewport.updateScaleDrag(
                    {snapshot->gizmoPosition.x,
                     snapshot->gizmoPosition.y +
                         l2d_editor::ViewportTransformModel::ScaleHandleDistance * 0.5f}),
                "Y scale update failed");
        require(viewport.endScaleDrag(), "Y scale commit failed");
        scaled = document.findObject(1u)->prefab.transform;
        require(near(scaled.scale, {3.f, 1.5f}), "Y scale gesture changed the wrong components");

        snapshot = viewport.snapshot();
        require(snapshot.has_value(), "post-axis-scale snapshot missing");
        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::Uniform,
                                        snapshot->scaleUniformHandlePosition),
                "uniform scale drag did not begin");
        const float diagonal = l2d_editor::ViewportTransformModel::ScaleHandleDistance *
                               0.70710678118654752440f * 2.f;
        require(viewport.updateScaleDrag(
                    {snapshot->gizmoPosition.x + diagonal, snapshot->gizmoPosition.y + diagonal}),
                "uniform scale update failed");
        require(viewport.endScaleDrag(), "uniform scale commit failed");
        scaled = document.findObject(1u)->prefab.transform;
        require(near(scaled.scale, {6.f, 3.f}),
                "uniform scale gesture did not preserve component ratio");
        require(history.undoCount() == 3u, "scale gestures did not produce one entry each");
        require(history.undoLabel() == "Scale object uniformly in viewport",
                "uniform scale history label is incorrect");
    }

    void testCancellationNoOpAndSelectionSafety()
    {
        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configure(document, viewport);

        require(history.execute(document, "Rename second", [](l2d_editor::EditorDocument& editor)
                                { return editor.renameObject(2u, "Renamed second"); }),
                "redo preservation setup failed");
        require(history.undo(document), "redo preservation undo failed");
        require(history.redoCount() == 1u, "redo stack setup is incorrect");

        const l2d::TransformState initial = document.findObject(1u)->prefab.transform;
        auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "cancellation snapshot missing");

        require(viewport.beginRotationDrag(snapshot->rotationHandlePosition),
                "cancelled rotation did not begin");
        require(viewport.updateRotationDrag({snapshot->gizmoPosition.x + 64.f,
                                             snapshot->gizmoPosition.y}),
                "cancelled rotation update failed");
        require(viewport.cancelActiveDrag(), "active rotation cancellation failed");
        require(near(document.findObject(1u)->prefab.transform.rotation, initial.rotation),
                "cancelled rotation did not restore the start transform");
        require(history.undoCount() == 0u && history.redoCount() == 1u,
                "cancelled rotation changed completed history");

        snapshot = viewport.snapshot();
        require(snapshot.has_value(), "no-op scale snapshot missing");
        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::X,
                                        snapshot->scaleXHandlePosition),
                "no-op scale did not begin");
        require(!viewport.endScaleDrag(), "untouched scale gesture entered history");
        require(history.undoCount() == 0u && history.redoCount() == 1u,
                "no-op scale changed completed history");

        snapshot = viewport.snapshot();
        require(snapshot.has_value(), "selection safety snapshot missing");
        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::Uniform,
                                        snapshot->scaleUniformHandlePosition),
                "selection safety scale did not begin");
        require(viewport.updateScaleDrag({snapshot->scaleUniformHandlePosition.x + 20.f,
                                          snapshot->scaleUniformHandlePosition.y + 20.f}),
                "selection safety scale update failed");
        require(document.selectObject(2u), "mid-gesture selection change failed");
        require(!viewport.updateScaleDrag({snapshot->scaleUniformHandlePosition.x + 30.f,
                                           snapshot->scaleUniformHandlePosition.y + 30.f}),
                "scale gesture continued after selection identity changed");
        require(!viewport.isDragging() && !history.hasOpenCoalescedCommand(),
                "selection change did not cancel the exclusive gesture");
        require(document.selectedObject() == 1u,
                "selection-change cancellation did not restore gesture-start selection");
        require(near(document.findObject(1u)->prefab.transform.scale, initial.scale),
                "selection-change cancellation did not restore scale");
    }

    void testGestureExclusivityAndRejectedScale()
    {
        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ViewportTransformModel viewport(document, history);
        configure(document, viewport);

        const auto snapshot = viewport.snapshot();
        require(snapshot.has_value(), "exclusivity snapshot missing");
        require(viewport.beginRotationDrag(snapshot->rotationHandlePosition),
                "exclusive rotation did not begin");
        require(!viewport.beginTranslationDrag(snapshot->gizmoPosition),
                "translation began during rotation gesture");
        require(!viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::X,
                                         snapshot->scaleXHandlePosition),
                "scale began during rotation gesture");
        require(!viewport.setView({1.f, 1.f}, 2.f), "view changed during rotation gesture");
        require(viewport.cancelRotationDrag(), "exclusive rotation cancellation failed");

        const l2d::TransformState before = document.findObject(1u)->prefab.transform;
        require(viewport.beginScaleDrag(l2d_editor::ViewportScaleHandle::X,
                                        snapshot->scaleXHandlePosition),
                "rejected scale did not begin");
        require(!viewport.updateScaleDrag(snapshot->gizmoPosition),
                "near-zero scale update was accepted");
        require(near(document.findObject(1u)->prefab.transform.scale, before.scale),
                "rejected scale update changed the document");
        require(viewport.cancelScaleDrag(), "rejected scale gesture cancellation failed");
    }
}

int main()
{
    try
    {
        testHandleGeometryAndHitTesting();
        testRotationCoalescesAndSupportsMultipleTurns();
        testAxisAndUniformScaleCoalesce();
        testCancellationNoOpAndSelectionSafety();
        testGestureExclusivityAndRejectedScale();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Rotation/scale gizmo regression failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "Rotation/scale gizmo regression passed\n";
    return 0;
}

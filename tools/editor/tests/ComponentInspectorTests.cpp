#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cstddef>
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

    l2d::Prefab makeInspectablePrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "Player";
        prefab.tag = "actor";
        prefab.transform.position = {12.f, -4.f};
        prefab.rectangleRenderer.emplace();
        prefab.customComponents.push_back({"game.health", 1u, true, "100"});
        return prefab;
    }

    l2d::Prefab makeControllerPrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "Controller";
        prefab.rigidBody.emplace();
        prefab.rigidBody->bodyType = l2d::BodyType2D::Kinematic;
        prefab.boxCollider.emplace();
        prefab.characterMotor.emplace();
        prefab.topDownController.emplace();
        return prefab;
    }

    l2d::Prefab makeRichInspectorPrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "Rich Inspector";
        prefab.rectangleRenderer.emplace();
        prefab.circleRenderer.emplace();

        l2d::SpriteRendererPrefab sprite;
        sprite.texture = "textures/player.png";
        sprite.textureRect.position = {0, 0};
        sprite.textureRect.size = {16, 16};
        prefab.spriteRenderer = sprite;

        l2d::AnimatorPrefab animator;
        animator.clips = {"animations/idle.anim", "animations/run.anim"};
        animator.initialClip = animator.clips.front();
        prefab.animator = animator;

        prefab.rigidBody.emplace();
        prefab.rigidBody->bodyType = l2d::BodyType2D::Dynamic;
        return prefab;
    }

    l2d_editor::EditorDocument makeSelectedDocument(l2d::Prefab prefab)
    {
        l2d::LevelDocument level;
        level.name = "Inspector Test";
        level.objects.push_back(std::move(prefab));

        l2d_editor::EditorDocument document;
        require(document.replace(std::move(level)), "inspector source replacement failed");
        require(document.selectObject(1u), "inspector selection failed");
        return document;
    }

    bool hasComponent(const l2d_editor::ComponentInspectorSnapshot& snapshot,
                      l2d_editor::InspectorComponentKind kind)
    {
        for (const l2d_editor::InspectorComponentEntry& entry : snapshot.components)
            if (entry.kind == kind) return true;
        return false;
    }

    void testSnapshotIsDeterministicAndSelectionBound()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeInspectablePrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        const auto snapshot = inspector.snapshot();
        require(snapshot.has_value(), "selected object produced no inspector snapshot");
        require(snapshot->objectId == 1u, "inspector snapshot exposed the wrong editor ID");
        require(snapshot->name == "Player", "inspector snapshot name is incorrect");
        require(snapshot->tag == "actor", "inspector snapshot tag is incorrect");
        require(snapshot->components.size() == 3u,
                "inspector snapshot exposed an unexpected component count");
        require(snapshot->components[0].kind == l2d_editor::InspectorComponentKind::Transform,
                "Transform is not the first deterministic inspector component");
        require(!snapshot->components[0].removable, "mandatory Transform was exposed as removable");
        require(snapshot->components[1].kind ==
                    l2d_editor::InspectorComponentKind::RectangleRenderer,
                "RectangleRenderer ordering is not deterministic");
        require(snapshot->components[2].kind == l2d_editor::InspectorComponentKind::Custom,
                "custom component ordering is not deterministic");
        require(snapshot->components[2].customIndex && *snapshot->components[2].customIndex == 0u,
                "custom component index was not preserved");
        require(snapshot->components[2].displayName == "Custom: game.health",
                "custom component display name is incorrect");

        document.clearSelection();
        require(!inspector.snapshot(), "cleared selection still produced an inspector snapshot");
        require(!inspector.setActive(false), "inspector edited an object without selection");
        require(!history.canUndo(), "failed unselected edit entered command history");
    }

    void testPropertyEditsAreUndoableAndValidated()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeInspectablePrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        require(inspector.setName("Hero"), "inspector rename failed");
        require(inspector.setTag("player"), "inspector tag edit failed");
        require(inspector.setActive(false), "inspector active edit failed");
        require(inspector.setZOrder(7), "inspector z-order edit failed");

        l2d::TransformState moved = document.findObject(1u)->prefab.transform;
        moved.position = {48.f, 24.f};
        moved.rotation = 15.f;
        moved.scale = {1.5f, 0.75f};
        require(inspector.setTransform(moved), "inspector transform edit failed");

        const l2d::Prefab& edited = document.findObject(1u)->prefab;
        require(edited.name == "Hero" && edited.tag == "player" && !edited.active &&
                    edited.zOrder == 7,
                "inspector property edits did not publish expected state");
        require(edited.transform.position == moved.position && edited.transform.rotation == 15.f &&
                    edited.transform.scale == moved.scale,
                "inspector transform edit did not publish expected state");
        require(history.undoCount() == 5u,
                "inspector property edits were not individually undoable");

        require(!inspector.setTransform(moved), "no-op transform edit unexpectedly succeeded");
        require(history.undoCount() == 5u, "no-op transform edit entered history");

        require(!inspector.setTag("broken\ntag"), "invalid tag edit unexpectedly succeeded");
        require(document.findObject(1u)->prefab.tag == "player",
                "invalid tag edit changed document state");
        require(history.undoCount() == 5u, "invalid tag edit entered history");

        require(history.undo(document), "transform undo failed");
        require(document.findObject(1u)->prefab.transform.position == sf::Vector2f{12.f, -4.f},
                "transform undo did not restore prior value");
        require(history.redo(document), "transform redo failed");
        require(document.findObject(1u)->prefab.transform.position == moved.position,
                "transform redo did not restore edited value");
    }

    void testBuiltInComponentAddRemoveAndValidation()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeInspectablePrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        require(inspector.addComponent(l2d_editor::InspectorComponentKind::CircleRenderer),
                "valid default component addition failed");
        auto snapshot = inspector.snapshot();
        require(snapshot &&
                    hasComponent(*snapshot, l2d_editor::InspectorComponentKind::CircleRenderer),
                "added component was not exposed by inspector snapshot");
        require(!inspector.addComponent(l2d_editor::InspectorComponentKind::CircleRenderer),
                "duplicate component addition unexpectedly succeeded");

        require(inspector.removeComponent(l2d_editor::InspectorComponentKind::CircleRenderer),
                "component removal failed");
        snapshot = inspector.snapshot();
        require(snapshot &&
                    !hasComponent(*snapshot, l2d_editor::InspectorComponentKind::CircleRenderer),
                "removed component remained in inspector snapshot");
        require(history.undo(document), "component removal undo failed");
        snapshot = inspector.snapshot();
        require(snapshot &&
                    hasComponent(*snapshot, l2d_editor::InspectorComponentKind::CircleRenderer),
                "component removal undo did not restore component");

        const std::size_t commandsBeforeInvalidAdd = history.undoCount();
        require(!inspector.addComponent(l2d_editor::InspectorComponentKind::SpriteRenderer),
                "invalid default SpriteRenderer addition unexpectedly succeeded");
        require(!document.findObject(1u)->prefab.spriteRenderer,
                "invalid SpriteRenderer addition changed document state");
        require(history.undoCount() == commandsBeforeInvalidAdd,
                "invalid component addition entered command history");

        require(!inspector.addComponent(l2d_editor::InspectorComponentKind::Transform),
                "mandatory Transform was addable");
        require(!inspector.removeComponent(l2d_editor::InspectorComponentKind::Transform),
                "mandatory Transform was removable");
    }

    void testDependencyBreakingRemovalIsRejected()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeControllerPrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        require(!inspector.removeComponent(l2d_editor::InspectorComponentKind::CharacterMotor),
                "dependency-breaking CharacterMotor removal unexpectedly succeeded");
        require(document.findObject(1u)->prefab.characterMotor.has_value(),
                "rejected dependency-breaking removal changed the document");
        require(!history.canUndo(), "rejected dependency-breaking removal entered history");

        require(inspector.removeComponent(l2d_editor::InspectorComponentKind::TopDownController),
                "TopDownController removal failed");
        require(inspector.removeComponent(l2d_editor::InspectorComponentKind::CharacterMotor),
                "CharacterMotor removal failed after dependent controller was removed");
        require(!document.findObject(1u)->prefab.characterMotor,
                "CharacterMotor remained after valid removal");
    }

    void testCustomComponentEditing()
    {
        l2d::Prefab prefab;
        prefab.name = "Custom Host";
        l2d_editor::EditorDocument document = makeSelectedDocument(std::move(prefab));
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        l2d::SerializedComponentPrefab component{"game.health", 1u, true, "100"};
        require(inspector.addCustomComponent(component), "custom component addition failed");
        require(document.findObject(1u)->prefab.customComponents.size() == 1u,
                "custom component addition did not publish");

        component.version = 2u;
        component.required = false;
        component.data = "125";
        require(inspector.updateCustomComponent(0u, component), "custom component update failed");
        const auto& updated = document.findObject(1u)->prefab.customComponents[0];
        require(updated.version == 2u && !updated.required && updated.data == "125",
                "custom component update published incorrect values");

        l2d::SerializedComponentPrefab invalid = component;
        invalid.type = "broken\ntype";
        const std::size_t commandsBeforeInvalidUpdate = history.undoCount();
        require(!inspector.updateCustomComponent(0u, std::move(invalid)),
                "invalid custom component update unexpectedly succeeded");
        require(document.findObject(1u)->prefab.customComponents[0].type == "game.health",
                "invalid custom component update changed the document");
        require(history.undoCount() == commandsBeforeInvalidUpdate,
                "invalid custom component update entered history");

        require(!inspector.updateCustomComponent(8u, component),
                "out-of-range custom component update unexpectedly succeeded");
        require(inspector.removeCustomComponent(0u), "custom component removal failed");
        require(document.findObject(1u)->prefab.customComponents.empty(),
                "custom component removal did not publish");
        require(history.undo(document), "custom component removal undo failed");
        require(document.findObject(1u)->prefab.customComponents.size() == 1u,
                "custom component removal undo did not restore data");
    }

    void testComponentSpecificControlsAreTransactional()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeRichInspectorPrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        require(inspector.setRectangleSize({64.f, 32.f}), "rectangle size control failed");
        require(inspector.setRectangleColor(sf::Color::Red), "rectangle color control failed");
        require(inspector.setCircleRadius(24.f), "circle radius control failed");
        require(inspector.setCircleColor(sf::Color::Blue), "circle color control failed");
        require(inspector.setSpriteSize({48.f, 24.f}), "sprite size control failed");
        require(inspector.setSpriteOrigin({8.f, 8.f}), "sprite origin control failed");
        require(inspector.setSpriteFlipX(true), "sprite flip X control failed");
        require(inspector.setAnimatorPlaybackSpeed(1.5f), "animator speed control failed");
        require(inspector.setAnimatorPlaying(false), "animator playing control failed");
        require(inspector.setRigidBodyMass(2.f), "rigid-body mass control failed");
        require(inspector.setRigidBodyUseGravity(true), "rigid-body gravity control failed");
        require(inspector.setRigidBodyGravityScale(0.5f), "rigid-body gravity scale failed");

        const l2d::Prefab& edited = document.findObject(1u)->prefab;
        require(edited.rectangleRenderer->size == sf::Vector2f{64.f, 32.f},
                "rectangle control published wrong size");
        require(edited.circleRenderer->radius == 24.f, "circle control published wrong radius");
        require(edited.spriteRenderer->flipX, "sprite flip control did not publish");
        require(edited.animator->playbackSpeed == 1.5f && !edited.animator->playing,
                "animator controls published wrong state");
        require(edited.rigidBody->mass == 2.f && edited.rigidBody->useGravity &&
                    edited.rigidBody->gravityScale == 0.5f,
                "rigid-body controls published wrong state");

        const std::size_t commandsBeforeRejected = history.undoCount();
        require(!inspector.setCircleRadius(-1.f), "invalid negative circle radius was accepted");
        require(!inspector.setAnimatorPlaybackSpeed(-1.f),
                "invalid negative animator speed was accepted");
        require(!inspector.setRigidBodyMass(0.f), "invalid zero rigid-body mass was accepted");
        require(history.undoCount() == commandsBeforeRejected,
                "rejected component edits polluted undo history");
        require(document.findObject(1u)->prefab.circleRenderer->radius == 24.f,
                "rejected component edit changed document state");

        require(!inspector.setSpriteFlipX(true), "no-op sprite flip unexpectedly succeeded");
        require(history.undoCount() == commandsBeforeRejected,
                "no-op component edit entered undo history");

        require(inspector.clearAnimatorInitialClip(), "clearing animator initial clip failed");
        require(inspector.removeAnimatorClipAsset("animations/idle.anim"),
                "removing a non-final animator clip failed");
        require(document.findObject(1u)->prefab.animator->clips.size() == 1u,
                "animator clip removal published wrong clip count");
        require(!inspector.removeAnimatorClipAsset("animations/run.anim"),
                "removing the final animator clip unexpectedly succeeded");
        require(document.findObject(1u)->prefab.animator->clips.size() == 1u,
                "rejected final clip removal changed animator state");

        require(history.undo(document), "component-specific edit undo failed");
        require(document.findObject(1u)->prefab.animator->clips.size() == 2u,
                "component-specific undo did not restore animator clips");
    }

    void testRigidBodyControlRespectsComponentDependencies()
    {
        l2d_editor::EditorDocument document = makeSelectedDocument(makeControllerPrefab());
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);

        require(!inspector.setRigidBodyType(l2d::BodyType2D::Dynamic),
                "dependency-breaking rigid-body type change unexpectedly succeeded");
        require(document.findObject(1u)->prefab.rigidBody->bodyType == l2d::BodyType2D::Kinematic,
                "rejected rigid-body type change modified document state");
        require(!history.canUndo(), "rejected rigid-body type change entered history");
    }
}

int main()
{
    try
    {
        testSnapshotIsDeterministicAndSelectionBound();
        testPropertyEditsAreUndoableAndValidated();
        testBuiltInComponentAddRemoveAndValidation();
        testDependencyBreakingRemovalIsRejected();
        testCustomComponentEditing();
        testComponentSpecificControlsAreTransactional();
        testRigidBodyControlRespectsComponentDependencies();
        std::cout << "Lorenzo2D component inspector tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lorenzo2D component inspector test failure: " << error.what() << '\n';
        return 1;
    }
}

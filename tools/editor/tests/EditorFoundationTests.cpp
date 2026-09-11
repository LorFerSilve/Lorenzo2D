#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/SceneHierarchyModel.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    l2d::Prefab makePrefab(std::string name, float x)
    {
        l2d::Prefab prefab;
        prefab.name = std::move(name);
        prefab.transform.position = {x, 0.f};
        return prefab;
    }

    l2d::LevelDocument makeLevel()
    {
        l2d::LevelDocument level;
        level.name = "Editor Test";
        level.objects.push_back(makePrefab("First", 1.f));
        level.objects.push_back(makePrefab("Second", 2.f));
        level.objects.push_back(makePrefab("Third", 3.f));
        return level;
    }

    std::string serialize(const l2d_editor::EditorDocument& document)
    {
        std::ostringstream output;
        require(document.save(output), "editor document serialization failed");
        return output.str();
    }

    void testDocumentReplacementAndStableIds()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "valid level replacement failed");
        require(document.objectCount() == 3u, "unexpected editor object count");
        require(document.objects()[0].id == 1u, "first editor ID is not deterministic");
        require(document.objects()[1].id == 2u, "second editor ID is not deterministic");
        require(document.objects()[2].id == 3u, "third editor ID is not deterministic");

        require(document.selectObject(2u), "valid selection failed");
        require(document.selectedObject() == 2u, "selection identity was not retained");
        require(!document.selectObject(999u), "invalid selection unexpectedly succeeded");
        require(document.selectedObject() == 2u, "failed selection changed the document");

        const std::string beforeInvalidReplace = serialize(document);
        l2d::LevelDocument invalid = makeLevel();
        invalid.objects.front().transform.position.x = std::numeric_limits<float>::quiet_NaN();
        require(!document.replace(std::move(invalid)), "invalid level replacement succeeded");
        require(serialize(document) == beforeInvalidReplace,
                "invalid replacement changed document contents");
        require(document.selectedObject() == 2u,
                "invalid replacement changed document selection");

        const std::optional<l2d_editor::EditorObjectId> added =
            document.addObject(makePrefab("Fourth", 4.f));
        require(added && *added == 4u, "new editor ID did not continue monotonically");
        require(document.removeObject(1u), "existing object removal failed");
        const std::optional<l2d_editor::EditorObjectId> replacement =
            document.addObject(makePrefab("Fifth", 5.f));
        require(replacement && *replacement == 5u, "removed editor ID was unexpectedly reused");
    }

    void testHierarchySelectionModel()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "hierarchy source replacement failed");
        l2d_editor::SceneHierarchyModel hierarchy(document);

        const std::vector<l2d_editor::SceneHierarchyItem> initial = hierarchy.items();
        require(initial.size() == 3u, "hierarchy did not expose every editor object");
        require(initial[0].name == "First" && initial[1].name == "Second" &&
                    initial[2].name == "Third",
                "hierarchy order diverged from level order");
        require(!initial[0].selected && !initial[1].selected && !initial[2].selected,
                "hierarchy reported an unexpected initial selection");

        require(hierarchy.selectNext(), "first hierarchy next-selection failed");
        require(document.selectedObject() == 1u, "next-selection did not choose the first object");
        require(hierarchy.selectNext(), "second hierarchy next-selection failed");
        require(document.selectedObject() == 2u, "next-selection did not advance");
        require(hierarchy.selectPrevious(), "hierarchy previous-selection failed");
        require(document.selectedObject() == 1u, "previous-selection did not move backward");
        require(!hierarchy.selectPrevious(), "hierarchy moved before the first object");

        hierarchy.clearSelection();
        require(hierarchy.selectPrevious(), "unselected previous-selection failed");
        require(document.selectedObject() == 3u,
                "unselected previous-selection did not choose the last object");
        require(!hierarchy.select(999u), "hierarchy accepted an unknown editor ID");
    }

    void testTransactionalCommandHistory()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "command source replacement failed");
        l2d_editor::EditorCommandHistory history;

        require(history.execute(document, "Rename object",
                                [](l2d_editor::EditorDocument& editor)
                                { return editor.renameObject(1u, "Renamed"); }),
                "rename command failed");
        require(document.findObject(1u)->prefab.name == "Renamed", "rename did not apply");
        require(history.undoLabel() == "Rename object", "undo label is incorrect");

        const l2d::TransformState moved{{42.f, -7.f}, 15.f, {2.f, 0.5f}};
        require(history.execute(document, "Move object",
                                [moved](l2d_editor::EditorDocument& editor)
                                { return editor.setObjectTransform(1u, moved); }),
                "transform command failed");
        require(document.findObject(1u)->prefab.transform.position == moved.position,
                "transform did not apply");

        require(history.undo(document), "transform undo failed");
        require(document.findObject(1u)->prefab.transform.position == sf::Vector2f{1.f, 0.f},
                "transform undo did not restore exact state");
        require(history.undo(document), "rename undo failed");
        require(document.findObject(1u)->prefab.name == "First",
                "rename undo did not restore exact state");
        require(history.redo(document), "rename redo failed");
        require(history.redo(document), "transform redo failed");
        require(document.findObject(1u)->prefab.name == "Renamed",
                "redo did not restore renamed state");
        require(document.findObject(1u)->prefab.transform.position == moved.position,
                "redo did not restore transform state");

        require(history.undo(document), "redo-branch setup undo failed");
        require(history.execute(document, "Alternate rename",
                                [](l2d_editor::EditorDocument& editor)
                                { return editor.renameObject(2u, "Alternate"); }),
                "alternate command failed");
        require(!history.canRedo(), "new command did not invalidate redo history");
    }

    void testFailedCommandsRollback()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "rollback source replacement failed");
        l2d_editor::EditorCommandHistory history;

        require(!history.execute(document, "Rejected mutation",
                                 [](l2d_editor::EditorDocument& editor)
                                 {
                                     (void)editor.setName("Temporary");
                                     (void)editor.renameObject(1u, "Temporary object");
                                     return false;
                                 }),
                "rejected command unexpectedly succeeded");
        require(document.name() == "Editor Test", "rejected command changed the level name");
        require(document.findObject(1u)->prefab.name == "First",
                "rejected command changed object state");
        require(!history.canUndo(), "rejected command entered undo history");

        bool threw = false;
        try
        {
            (void)history.execute(document, "Throwing mutation",
                                  [](l2d_editor::EditorDocument& editor) -> bool
                                  {
                                      (void)editor.setName("Temporary");
                                      throw std::runtime_error("expected editor command failure");
                                  });
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        require(threw, "throwing command did not propagate its exception");
        require(document.name() == "Editor Test",
                "throwing command was not rolled back transactionally");
        require(!history.canUndo(), "throwing command entered undo history");
    }

    void testDeleteUndoRestoresSelectionAndIdentity()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "delete source replacement failed");
        require(document.selectObject(2u), "delete test selection failed");

        l2d_editor::EditorCommandHistory history;
        require(history.execute(document, "Delete selected",
                                [](l2d_editor::EditorDocument& editor)
                                { return editor.removeObject(2u); }),
                "delete command failed");
        require(document.findObject(2u) == nullptr, "delete command kept removed object");
        require(document.selectedObject() == l2d_editor::InvalidEditorObjectId,
                "delete command kept an invalid selection");

        require(history.undo(document), "delete undo failed");
        require(document.findObject(2u) != nullptr, "delete undo did not restore object identity");
        require(document.selectedObject() == 2u, "delete undo did not restore selection");
        require(history.redo(document), "delete redo failed");
        require(document.findObject(2u) == nullptr, "delete redo did not remove object again");
    }

    void testBoundedHistory()
    {
        l2d_editor::EditorDocument document;
        l2d::LevelDocument level;
        level.objects.push_back(makePrefab("Initial", 0.f));
        require(document.replace(std::move(level)), "bounded-history source replacement failed");

        l2d_editor::EditorCommandHistory history;
        constexpr std::size_t ExtraCommands = 4u;
        const std::size_t commandCount =
            l2d_editor::EditorCommandHistory::MaximumCommandCount + ExtraCommands;
        for (std::size_t index = 0u; index < commandCount; ++index)
        {
            const std::string name = "Name" + std::to_string(index);
            require(history.execute(document, "Rename", [name](l2d_editor::EditorDocument& editor)
                                    { return editor.renameObject(1u, name); }),
                    "bounded-history rename failed");
        }

        require(history.undoCount() == l2d_editor::EditorCommandHistory::MaximumCommandCount,
                "undo history exceeded its configured bound");
        for (std::size_t index = 0u;
             index < l2d_editor::EditorCommandHistory::MaximumCommandCount; ++index)
        {
            require(history.undo(document), "bounded-history undo failed");
        }
        require(!history.canUndo(), "bounded-history undo retained an unexpected command");
        require(document.findObject(1u)->prefab.name == "Name3",
                "bounded-history eviction was not deterministic");

        for (std::size_t index = 0u;
             index < l2d_editor::EditorCommandHistory::MaximumCommandCount; ++index)
        {
            require(history.redo(document), "bounded-history redo failed");
        }
        require(document.findObject(1u)->prefab.name == "Name259",
                "bounded-history redo did not restore the latest state");
    }

    void testRuntimeFormatRoundTrip()
    {
        l2d_editor::EditorDocument source;
        require(source.replace(makeLevel()), "round-trip source replacement failed");
        require(source.selectObject(2u), "round-trip selection setup failed");

        const std::string encoded = serialize(source);
        std::istringstream input(encoded);
        l2d_editor::EditorDocument loaded;
        require(loaded.load(input), "editor failed to load runtime level serialization");
        require(loaded.objectCount() == source.objectCount(), "round-trip object count changed");
        require(loaded.objects()[0].id == 1u && loaded.objects()[1].id == 2u &&
                    loaded.objects()[2].id == 3u,
                "editor IDs were not regenerated deterministically after load");
        require(loaded.selectedObject() == l2d_editor::InvalidEditorObjectId,
                "editor-only selection leaked through runtime serialization");
        require(serialize(loaded) == encoded, "runtime level bytes changed after editor round-trip");
    }
}

int main()
{
    try
    {
        testDocumentReplacementAndStableIds();
        testHierarchySelectionModel();
        testTransactionalCommandHistory();
        testFailedCommandsRollback();
        testDeleteUndoRestoresSelectionAndIdentity();
        testBoundedHistory();
        testRuntimeFormatRoundTrip();
        std::cout << "Lorenzo2D editor foundation tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lorenzo2D editor foundation test failure: " << error.what() << '\n';
        return 1;
    }
}

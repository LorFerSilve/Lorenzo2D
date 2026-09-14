# End-to-end editor authoring tutorial

This tutorial is the Phase 13 completion workflow. It creates a small runtime-readable level, edits it through Lorenzo2D's editor models, saves it with the public level serializer, and sends the current unsaved authoring state through the play/test workflow. No JSON is hand-edited and no editor-private level format is introduced.

## What this proves

The workflow deliberately crosses the same boundaries exercised throughout Phase 13:

1. `EditorDocument` owns editor-local identities while storing public runtime `Prefab` data.
2. `EditorCommandHistory` records supported authoring mutations deterministically.
3. `EditorDocument::saveToFile()` publishes the canonical `.l2dlevel` format used by runtime-only builds.
4. `PlayTestWorkflowModel` snapshots the current editor state and gives a host launcher an absolute runtime-readable level path.
5. The engine package remains independent of editor code; the standalone editor project is built against the installed Lorenzo2D package.

The executable regression `Lorenzo2DEditorEndToEndAuthoringTutorialTests` follows this tutorial mechanically in CI, so this document and the supported authoring path cannot silently diverge.

## 1. Build and install Lorenzo2D

Build and install the engine into a prefix using the normal project workflow. Then configure the editor as an independent consumer of that installed package:

```sh
cmake -S tools/editor -B build-editor \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d-install \
  -DL2D_EDITOR_BUILD_TESTS=ON
cmake --build build-editor --parallel
ctest --test-dir build-editor --output-on-failure --no-tests=error
```

The editor must not be built by reaching into private engine source directories. `find_package(Lorenzo2D CONFIG REQUIRED)` is the dependency boundary.

## 2. Create a level through editor APIs

Start with an empty editor document and command history:

```cpp
l2d_editor::EditorDocument document;
l2d_editor::EditorCommandHistory history;

document.setName("Phase 13 Tutorial");
```

Create the first object through the editor document instead of writing a `.l2dlevel` file manually:

```cpp
l2d::Prefab player;
player.name = "Player";
player.transform.position = {64.f, 96.f};

l2d_editor::EditorObjectId playerId = l2d_editor::InvalidEditorObjectId;
history.execute(document, "Create Player",
    [&](l2d_editor::EditorDocument& editor)
    {
        const auto created = editor.addObject(player);
        if (!created) return false;
        playerId = *created;
        return editor.selectObject(playerId);
    });
```

Create a second object the same way:

```cpp
l2d::Prefab goal;
goal.name = "Goal";
goal.transform.position = {256.f, 96.f};

history.execute(document, "Create Goal",
    [&](l2d_editor::EditorDocument& editor)
    {
        return editor.addObject(goal).has_value();
    });
```

This remains inside the runtime `Prefab` validation contract. Editor object IDs are authoring-only identities and are not serialized into the runtime level.

## 3. Edit through deterministic command history

Move the player as one undoable command:

```cpp
history.execute(document, "Move Player",
    [&](l2d_editor::EditorDocument& editor)
    {
        const auto* object = editor.findObject(playerId);
        if (object == nullptr) return false;

        l2d::TransformState transform = object->prefab.transform;
        transform.position = {96.f, 128.f};
        return editor.setObjectTransform(playerId, transform);
    });
```

For interactive viewport gestures, the editor uses the coalesced command path so a drag becomes one undo entry. Rotation and scaling use the same transaction boundary added in Phase 13.10. Component-specific edits continue to route through validated inspector controls rather than mutating serialized state out of band.

## 4. Save a canonical runtime level

Save through the editor document:

```cpp
if (!document.saveToFile("tutorial.l2dlevel"))
    throw std::runtime_error("unable to save tutorial level");
```

A runtime-only consumer can read the result directly:

```cpp
l2d::LevelDocument runtimeLevel;
if (!l2d::LevelSerializer::loadFromFile("tutorial.l2dlevel", runtimeLevel))
    throw std::runtime_error("runtime could not read tutorial level");
```

There is no conversion step and no editor-only persistence schema.

## 5. Run the current authoring state through play/test

Configure one `{level}` placeholder. The workflow replaces it with an absolute path to a temporary snapshot of the current editor state:

```cpp
l2d_editor::PlayTestWorkflowModel workflow;
workflow.configure(
    "/path/to/your/game",
    "/path/to/your/game/working-directory",
    {"--level", "{level}"});
```

The portable model intentionally does not guess how to create or terminate operating-system processes. Supply host-specific hooks. A real editor host can map these to `CreateProcess` on Windows, `posix_spawn`/equivalent on POSIX, an IDE launcher, or another process layer:

```cpp
l2d_editor::PlayTestProcessHooks hooks;
hooks.launch = [](const l2d_editor::PlayTestLaunchRequest& request)
{
    // Start request.executable in request.workingDirectory with request.arguments.
    return launchProcess(request);
};
hooks.stop = []()
{
    return stopProcess();
};

workflow.start(document, ".lorenzo2d/playtest", std::move(hooks));
```

The launched game receives the current authoring snapshot even if `tutorial.l2dlevel` has not been saved again. Stopping the workflow delegates process termination first and removes only the workflow-owned snapshot after the host confirms termination.

## 6. CI-backed tutorial evidence

Run only the end-to-end regression with:

```sh
ctest --test-dir build-editor \
  -R Lorenzo2DEditorEndToEndAuthoringTutorialTests \
  --output-on-failure
```

The regression creates `Player` and `Goal` through editor APIs, edits the player through command history, saves the canonical level, reloads it through the public runtime `LevelSerializer`, starts a play/test session, verifies that the launch hook receives a runtime-readable snapshot containing the authored state, and then verifies clean session shutdown.

That test is intentionally small. It is completion evidence for the authoring path, not a replacement for the focused Phase 13 regressions for inspector validation, asset picking, tilemap editing, collider/navigation editing, animation preview, play/test session ownership, or transform gizmos.

## Phase 13 boundary after this tutorial

Phase 13 establishes a practical editor/tooling foundation. It does not claim a Phase 14 content pipeline, a universal process launcher, editor-private serialization, hot reload, a general scripting system, or full production-game authoring ergonomics. Those remain separate roadmap work so the runtime/editor dependency direction stays explicit.

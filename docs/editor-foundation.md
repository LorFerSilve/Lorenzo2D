# Lorenzo2D Editor foundation

Phase 13 starts with a strict dependency boundary: the editor is a separate CMake project under
`tools/editor` and discovers Lorenzo2D with `find_package(Lorenzo2D CONFIG REQUIRED)`. The engine
build, installed headers, and exported CMake package do not contain editor code or editor-only
dependencies.

## Phase 13.1 scope

The first slice provides:

- a standalone `Lorenzo2DEditor` application target;
- an editor-only document model backed by the public `LevelDocument` and `Prefab` contracts;
- deterministic editor object IDs for hierarchy and selection state;
- a flat scene-hierarchy view model that preserves runtime level order;
- keyboard selection in the application shell (`Up` / `Down`, `Escape` to close);
- bounded transactional undo/redo with exact before/after document snapshots;
- scene load/save through the existing public `LevelSerializer` runtime format;
- regression tests built against an installed Lorenzo2D package.

The current runtime level format is flat, so the editor does not invent parent/child relationships
that the engine cannot persist. Editor IDs are allocated from a document-local monotonic high-water
mark and are deliberately not written into `.l2dlevel` files. Successful scene replacement or load
allocates a fresh ID range and clears selection, so IDs retained from an older scene cannot alias new
objects.

Undo/redo snapshots retain the allocator high-water value required to validate restoration, but
restoration never blindly assigns that historical value. The restored allocator is the monotonic
maximum of the current and snapshotted high-water marks; the exhausted sentinel remains exhausted.
An object restored by redo retains the ID stored in that snapshot, while later newly created objects
continue above every ID allocated previously during that `EditorDocument` lifetime. Rejected or
throwing commands therefore burn any IDs they allocated instead of making them reusable.

`EditorDocument` supports copy and move construction for creating another document lifetime, but
copy/move assignment is intentionally disabled. Replacing the contents of an existing identity-bearing
document must use `replace()` or the load APIs so its allocator cannot be rewound by implicit
memberwise assignment. This prevents stale editor references from silently resolving to unrelated
objects after scene replacement, history branching, or rollback.

## Phase 13.2 component inspector

The second slice turns hierarchy selection into a visible authoring surface and introduces
`ComponentInspectorModel` as the editor-side boundary for selected-object inspection and property
editing.

The inspector snapshot exposes the selected object's editor ID, name, tag, active state, z-order,
transform, built-in component list, and serialized custom components in deterministic order.
`Transform` is always present and non-removable. Optional built-in components follow the stable
runtime `Prefab` order, followed by custom components in serialized order. The model never stores a
pointer into `EditorDocument` object storage; every operation resolves the current stable editor ID
again, avoiding dangling references after history restoration or document mutation.

All inspector writes route through `EditorCommandHistory`. A mutation edits a copy of the selected
runtime `Prefab`, then publishes it only through `EditorDocument::replaceObject()`. Consequently the
existing public `isValidPrefab()` contract remains the source of truth for editor validity. Invalid
states are rejected transactionally and do not enter undo history. Examples include an unconfigured
default `SpriteRenderer`, a malformed custom component type, or removing `CharacterMotor` while a
controller still depends on it.

Phase 13.2 supports:

- undoable object name, tag, active-state, z-order, and transform edits;
- deterministic inspection of all currently serialized built-in components;
- add/remove for optional built-in components when the resulting runtime `Prefab` is valid;
- an escape hatch for configured component edits through a validated `Prefab` mutation callback;
- add/update/remove for serialized custom components;
- separate component-inspector regression coverage against the installed engine package;
- visible scene-hierarchy and component-inspector panels in the editor application shell.

The shell intentionally keeps interaction minimal while the editor framework is still dependency
light. `Up`/`Down` changes selection, `A`/`D`/`W`/`S` nudges the selected transform through command
history, `Left`/`Right` adjusts z-order, `Space` toggles active state, `F1` toggles the default
`RectangleRenderer`, `Z` performs undo, and `Q` performs redo. The panels use SFML directly and try a
small platform-specific list of system UI fonts; failure to locate one disables panel text without
changing editor document behavior.

This slice does not claim viewport transform gizmos, arbitrary graphical field editors, asset
picking, or component-specific rich controls. Those remain later Phase 13 work. In particular,
asset-backed components such as `SpriteRenderer` and `Animator` require valid asset IDs/configuration;
the generic default-add operation is expected to fail until a caller supplies a valid configured
component through the validated mutation path or a later asset-browser UI.

## Undo/redo contract

`EditorCommandHistory` retains at most 256 successful commands. A command receives an
`EditorDocument` mutation callback. If the callback returns `false`, document contents and selection
are restored and no history entry is created. If it throws, those same visible document properties
are restored before the exception propagates. In both cases, the allocator high-water mark is merged
monotonically rather than rewound.

The current implementation stores full document snapshots. This prioritizes deterministic and
transactional behavior over memory efficiency while editor operations are still small. Later Phase
13 slices may add specialized delta commands for high-frequency gizmo or painting operations without
changing the history semantics.

## Building the editor

First build and install Lorenzo2D and its SFML dependency into a prefix. Then configure the editor as
an independent project with that prefix on `CMAKE_PREFIX_PATH`:

```sh
cmake -S tools/editor -B build-editor \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d-install \
  -DL2D_EDITOR_BUILD_TESTS=ON
cmake --build build-editor --parallel
ctest --test-dir build-editor --output-on-failure --no-tests=error
```

CI performs this workflow after the installed-package consumer checks. This proves that the editor
uses the same public package surface available to external games rather than relying on source-tree
or private engine internals.

## Next editor slice

The next Phase 13 slice should build on the validated inspector mutations with viewport-oriented
transform editing/gizmos rather than adding ad-hoc direct mutations. Gizmo drag publication should
have explicit command coalescing semantics so a continuous drag becomes one deterministic undo step.
After that boundary is stable, asset browsing/picking can provide configured values for asset-backed
components without weakening runtime `Prefab` validation.

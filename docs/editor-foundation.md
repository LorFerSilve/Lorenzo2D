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

Asset picking and component-specific rich controls remain later Phase 13 work. In particular,
asset-backed components such as `SpriteRenderer` and `Animator` require valid asset IDs/configuration;
the generic default-add operation is expected to fail until a caller supplies a valid configured
component through the validated mutation path or a later asset-browser UI.

## Phase 13.3 viewport transform foundation

The third slice adds a real editor viewport interaction boundary without introducing engine-side
editor dependencies. `ViewportTransformModel` maps between editor viewport pixels and runtime world
positions using explicit viewport bounds, world center, and bounded zoom. It resolves selected
objects by stable editor ID on every operation and exposes the selected transform plus its on-screen
translation handle position.

The first gizmo operation is deterministic translation. A drag can emit many pointer updates, but it
is represented by one explicit coalesced `EditorCommandHistory` transaction:

- `beginCoalescedCommand()` captures the exact gesture-start document snapshot;
- `updateCoalescedCommand()` publishes each live transform update transactionally without creating a
  completed undo entry per frame;
- `commitCoalescedCommand()` records exactly one before/after command and invalidates redo only when
  the gesture is actually committed;
- `cancelCoalescedCommand()` restores the gesture-start snapshot and leaves existing redo history
  intact;
- ordinary execute/undo/redo operations are rejected while a coalesced gesture is open, preventing
  ambiguous history interleaving.

The viewport model owns no runtime scene objects and retains no pointer into `EditorDocument` object
storage. It stores only the selected editor ID and the drag-start transform. A selection identity
change during a drag cancels the exclusive gesture and restores the drag-start state. Returning the
pointer to the exact start position before release is treated as a net-zero gesture and creates no
undo entry.

The application shell now renders a central viewport between the hierarchy and inspector panels. It
shows the world origin axes, document-object position markers, and a translation gizmo for the
selected object. Dragging the selected center handle moves the object live in world space; releasing
commits one undo step. `Escape` cancels an active drag, otherwise it closes the editor. Existing
keyboard transform nudges remain separate ordinary history commands.

Focused viewport regressions validate:

- viewport/world coordinate round-tripping and bounded configuration validation;
- selection-bound gizmo hit testing;
- multiple drag updates coalescing into exactly one undo/redo command;
- preservation of rotation and scale during translation;
- cancellation, net-zero drags, and redo preservation;
- selection-change cancellation and exact gesture-start restoration;
- rejected coalesced mutations rolling back locally without closing the gesture or entering history.

Phase 13.3 deliberately establishes only the translation-gizmo and gesture-history foundation. It
does not yet claim rotation/scale handles, camera pan/zoom controls, renderer-accurate scene previews,
asset picking, tilemap painting, or general component widgets.

## Phase 13.4 asset browser/picking foundation

The fourth slice introduces an editor-owned boundary between configured resource roots and
asset-backed serialized component fields. It deliberately reuses the public `ResourceLocator` and
`AssetId` contracts rather than introducing the metadata, import, cook, dependency, or manifest model
reserved for Phase 14.

`AssetBrowserModel` owns the filesystem-facing state. Configured roots are normalized through
`ResourceLocator`, scanned with explicit limits, and published transactionally only after the complete
scan succeeds. The default limits bound scanned directory entries, published files, and recursion
depth. A failed scan therefore leaves the previous asset index intact instead of exposing a partial
generation.

Published browser entries use portable root-relative paths as the Phase 13 asset-ID candidate. The
browser rejects absolute IDs, parent traversal, platform-specific backslash separators, and invalid
path components before asking `ResourceLocator` to resolve them. Symlink entries are not published,
and `ResourceLocator` remains authoritative for canonical root containment. If multiple ordered roots
contain the same relative ID, only the first resolvable root is published, matching runtime resource
lookup precedence. Successful indexes are sorted by `AssetId`, and case-insensitive filtering
preserves that deterministic order.

This slice intentionally does not claim that a browsed file is already registered in an
`AssetManager`. Lorenzo2D currently separates a resource path from the logical registry key used by
`AssetManager` (for example, a texture may be loaded under a caller-selected name). Until Phase 14
introduces stable content metadata, using the root-relative resource path as a serialized `AssetId`
is a simple editor convention whose runtime registration remains the application's responsibility.
The picking boundary guarantees path/root validity and valid `Prefab` publication; it does not invent
or silently populate the runtime asset registry.

`AssetPickingModel` re-resolves the selected browser entry immediately before each mutation, so a
file removed after the browser was refreshed cannot be applied from stale UI state. Texture picks are
limited to the image extensions supported by the lightweight browser classification. Applying a
texture to an object without `SpriteRenderer` creates the smallest schema-valid `1x1` source
rectangle; richer source-rectangle/preview controls remain later Phase 13 work. Animator clip picks
preserve the existing runtime invariant that `initialClip`, when non-empty, belongs to `clips`.
Duplicate clip picks are no-ops and do not enter command history.

All picks route through `ComponentInspectorModel`, `EditorCommandHistory`, and ultimately
`EditorDocument::replaceObject()`. Runtime `isValidPrefab()` validation therefore remains
authoritative, and successful picks are ordinary deterministic undo/redo commands.

The application shell now provides:

- a bounded asset list in the lower hierarchy-side panel;
- pointer selection and wheel scrolling for the visible list;
- an inspector-side picking surface for Sprite texture, Animator clip, and Animator initial-clip
  targets;
- repeated `--resource-root <path>` / `--resource-root=<path>` options;
- a conservative fallback root policy: `./assets` when present, otherwise the opened level's parent,
  otherwise the current working directory.

Focused installed-package regressions cover deterministic ordering, ordered-root precedence,
case-insensitive filtering, portable-ID validation, traversal rejection, optional symlink skipping,
transactional scan/asset limits, stale selection rejection, Sprite/Animator publication, no-op
behavior, and undo/redo.

Phase 13.4 does not add source-asset metadata, import settings, background cooking, manifests,
dependency graphs, thumbnail generation, asset drag-and-drop, runtime registry introspection, or
renderer-accurate previews. Those boundaries either belong to later editor slices or Phase 14.

## Phase 13.5 bounded tilemap authoring foundation

The fifth slice introduces `TilemapAuthoringModel` as an editor-only mutation boundary over the
installed public `TileMapData` and `TiledJsonImporter` contracts. The editor does not create a second
tilemap schema and does not modify engine modules to depend on authoring state.

Tiled JSON enters the model only through `TiledJsonImporter::load()`. Import first builds a candidate
`TileMapData`; the candidate is published only after the runtime document is valid and the stricter
editor limits are satisfied. Failed parsing, runtime validation, or editor-limit validation therefore
leaves the previously published map, selection, and completed history unchanged.

The default editor envelope is intentionally much smaller than the runtime hard maximums:

- at most 256 Ki map cells;
- at most 64 layers;
- at most 4,096 tile definitions;
- at most 65,536 imported objects;
- at most 1,048,576 authored tile slots across all layers;
- at most 4,096 unique changed cells in one continuous paint stroke;
- at most 128 completed tile-paint history commands;
- at most 16 MiB input for one Tiled JSON import.

These are authoring-workload limits rather than new runtime support claims. Callers may construct a
model with a different non-zero envelope as long as runtime maxima are not exceeded.

Layer selection follows canonical `TileMapData::layers()` order. The tile palette is rebuilt from
runtime definitions and sorted numerically by `TileId`, so authoring order never depends on
`unordered_map` iteration. `EmptyTile` is the erase brush and cannot carry flip flags. Non-empty
brushes must resolve to an existing runtime tile definition, and flip flags are restricted to the
public horizontal/vertical/diagonal mask.

Painting always delegates the actual cell mutation to `TileMapData::setTile()`. A one-cell edit is
one undoable command. Continuous painting uses an explicit begin/update/commit/cancel gesture. The
open gesture snapshots the selected layer and brush, blocks selection/history interleaving, records
only unique touched-cell deltas, and commits all successful pointer updates as exactly one history
entry. Exceeding the per-stroke unique-cell limit rejects the extra cell without discarding already
valid live updates. Cancellation restores every touched cell to its gesture-start state and preserves
pre-existing redo history. A no-op stroke creates no history entry.

Unlike the small scene-document history, tile painting does not copy the complete tilemap per pointer
update. Completed commands store bounded before/after cell deltas. This keeps the externally visible
transactional undo/redo contract while preventing a large map from being multiplied by every paint
sample. History depth and per-stroke delta count independently bound retained memory.

This foundation deliberately does not implement a second editor-side Tiled writer. Lorenzo2D
currently exposes a public Tiled importer but no public exporter, so inventing a private serializer in
Phase 13 would create a competing persistence contract. Phase 13.5 therefore establishes validated
in-memory authoring and installed-package regressions only. A future persistence/export decision must
extend or deliberately define the public content contract rather than hiding one inside the editor.

Focused installed-package regressions cover:

- public Tiled import and transactional failed-import preservation;
- deterministic numeric tile-palette ordering;
- map, layer, definition, object, tile-slot, input, stroke, and history limits;
- validated layer/tile/flip selection;
- one-cell paint/erase undo and redo;
- continuous paint coalescing into one command;
- out-of-bounds and over-limit mutation rejection without history pollution;
- gesture cancellation and redo preservation;
- bounded history eviction.

Phase 13.5 does not yet claim a visual tile palette/grid widget, renderer-accurate tile previews,
Tiled export, tileset metadata editing, object-layer authoring, collision/nav overlays, or Phase 14
asset import/cook behavior. Those remain separate slices so this foundation stays deterministic and
bounded.

## Undo/redo contract

`EditorCommandHistory` retains at most 256 successful completed commands. A normal command receives an
`EditorDocument` mutation callback. If the callback returns `false`, document contents and selection
are restored and no history entry is created. If it throws, those same visible document properties
are restored before the exception propagates. In both cases, the allocator high-water mark is merged
monotonically rather than rewound.

Coalesced commands extend the same snapshot contract to continuous editor gestures. Live updates are
transactional, but the command is not appended to undo history until commit. A cancelled gesture
restores its initial snapshot. Completed undo/redo history remains bounded by the same 256-command
limit, and redo is cleared only on successful normal-command execution or successful coalesced
commit.

The scene-document implementation stores full document snapshots because normal inspector/gizmo
operations are still small. Tilemap painting uses a specialized bounded delta history instead: each
paint command stores only the unique touched cells and their before/after states while preserving the
same commit/cancel/undo/redo semantics. This avoids multiplying large tilemap snapshots by pointer
sample count.

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

After the bounded tilemap authoring model is validated, the next Phase 13 slice should establish a
bounded collider/navigation visualization and editing foundation over the existing public physics,
tilemap-collider, and navigation contracts. It should keep diagnostic/authoring overlays editor-owned
and avoid introducing editor dependencies into engine modules.

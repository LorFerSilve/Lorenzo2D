# Collider and navigation authoring foundation

Phase 13.6 establishes the first editor-owned collider/navigation authoring boundary without adding editor dependencies to Lorenzo2D runtime modules.

## Collider authoring

`ColliderNavigationAuthoringModel` reads the currently selected runtime `Prefab` and exposes a deterministic collider snapshot in box, circle, capsule, then convex-polygon order. The snapshot carries the selected object's transform so a viewport can derive world-space overlays without duplicating runtime collider state.

Collider mutations are deliberately routed through `ComponentInspectorModel`. This means the existing `Prefab` validator remains authoritative and successful mutations enter the shared `EditorCommandHistory`; invalid mutations are rejected transactionally and do not pollute undo/redo history.

The Phase 13.6 API covers collider offsets and the shape-defining properties needed for editor manipulation:

- box size;
- circle radius;
- capsule radius and height;
- convex-polygon vertices.

Material/filter/sensor editing remains available through the existing component-inspector mutation boundary and can receive richer dedicated controls in a later Phase 13 slice.

## Navigation authoring

The model owns an optional `NavigationGrid2D` editor snapshot and exposes deterministic overlay cells containing grid coordinates, world-space cell centers, and public `NavigationCell2D` values. Navigation grids can be supplied directly or derived from public tilemap-navigation conversion contracts.

Editor publication is intentionally bounded to 262,144 cells. A rejected or over-limit candidate leaves the previously published grid and its edit history unchanged.

Navigation edits operate on individual cells and store bounded per-cell deltas rather than whole-grid copies. The local navigation undo/redo history is capped at 256 entries. This keeps interactive edits predictable while avoiding memory growth proportional to `grid size × history depth`.

Supported edits are:

- complete public navigation-cell replacement;
- walkability changes;
- traversal-cost changes;
- undo/redo of accepted navigation edits.

Replacing or clearing the navigation grid resets this local cell-delta history because prior cell coordinates no longer have a stable target identity.

## Architectural boundary

This slice is editor-owned. It consumes installed public Lorenzo2D APIs only and does not introduce editor headers, state, diagnostics, or rendering concerns into engine modules. Runtime validation and serialization contracts remain the source of truth for authored game data.

The overlay snapshot is intentionally renderer-agnostic. A richer editor viewport can draw collider shapes and navigation cells from this model without coupling the runtime engine to editor rendering code.

## Validation

`ColliderNavigationAuthoringTests` covers:

- deterministic collider snapshot ordering and transform context;
- collider mutation publication through shared document history;
- transactional rejection of invalid collider values;
- navigation-overlay ordering and world-space cell centers;
- navigation no-op and bounds rejection;
- per-cell undo/redo semantics;
- preservation of prior state when an over-limit grid is rejected;
- clearing grid-local edit history when the navigation grid is cleared.

The test target is built by the standalone editor CMake project against the installed Lorenzo2D package, preserving the editor's package-boundary purity check.

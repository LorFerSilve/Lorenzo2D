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
that the engine cannot persist. Editor IDs and selection are session-local and are deliberately not
written into `.l2dlevel` files.

## Undo/redo contract

`EditorCommandHistory` retains at most 256 successful commands. A command receives an
`EditorDocument` mutation callback. If the callback returns `false`, the complete pre-command state
is restored and no history entry is created. If it throws, the document is restored before the
exception propagates.

The Phase 13.1 implementation stores full document snapshots. This prioritizes deterministic and
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

The next Phase 13 slice should turn the hierarchy/selection model into an actual editor panel and add
the first component-inspector surface. Transform editing should route through the existing command
history so undo/redo remains deterministic from the first interactive editing workflow.

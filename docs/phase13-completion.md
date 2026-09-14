# Phase 13 completion evidence

Phase 13 establishes Lorenzo2D's tooling/editor foundation while preserving the architectural rule that engine/runtime modules never depend on editor code. The editor remains a standalone `tools/editor` CMake project that consumes the installed public Lorenzo2D package.

## Definition-of-done mapping

| Requirement | Evidence |
| --- | --- |
| Editor is a separate target/application | `tools/editor` is an independent CMake project using `find_package(Lorenzo2D CONFIG REQUIRED)` and builds `Lorenzo2DEditor` plus editor-only core/tests. |
| Existing level formats remain readable by runtime-only builds | `EditorDocument` persists through the public `LevelSerializer` contract. The Phase 13.11 end-to-end regression saves a level and reloads it directly with `l2d::LevelSerializer`. |
| Undo/redo is deterministic for supported editor operations | Phase 13.1 introduced bounded transactional history; later inspector and gizmo slices route mutations through that contract, including coalesced continuous gestures. Focused installed-package regressions cover normal commands, cancellation, no-op behavior, rollback, redo preservation, and bounded history. |
| Tutorial creates and runs a small level without hand-editing JSON | `docs/editor-authoring-tutorial.md` documents the workflow. `Lorenzo2DEditorEndToEndAuthoringTutorialTests` mechanically creates `Player` and `Goal`, edits the player, saves the canonical level, runtime-loads it, and publishes the current editor state through `PlayTestWorkflowModel` to a launch hook. No JSON is written by the tutorial or test. |
| Engine package remains consumable without editor dependencies | The engine package is installed first; CI then configures `tools/editor` independently against that installation. Editor headers/sources are not exported by the engine package and engine modules do not link editor targets. |

## Implemented slices

Phase 13 was delivered as bounded vertical slices:

- 13.1 — editor document, hierarchy, stable editor IDs, scene open/save, deterministic undo/redo;
- 13.2 — component inspector foundation;
- 13.3 — viewport transform and translation-gizmo/coalesced-history foundation;
- 13.4 — bounded asset browser and asset picking;
- 13.5 — bounded tilemap authoring and Tiled import;
- 13.6 — collider/navigation visualization and editing foundation;
- 13.7 — animation preview foundation;
- 13.8 — play/test workflow foundation;
- 13.9 — richer component-specific inspector controls;
- 13.10 — rotation and scale gizmos;
- 13.11 — end-to-end authoring tutorial and completion evidence.

## Validation boundary

Phase 13 completion does not promote unsupported runtime features. The editor continues to reuse public runtime validation and serialization instead of maintaining parallel schemas. Workload-bounded models remain responsible for editor-specific state such as browser indexes, tile paint history, preview state, and play/test snapshot ownership.

The new end-to-end tutorial regression is additive to the focused editor tests; it is not intended to duplicate their detailed edge-case coverage. Its purpose is to prove the complete supported authoring path across editor creation/editing, runtime serialization, and play/test snapshot publication using only installed public engine contracts.

## CI completion gate

The Phase 13.11 pull request is complete only after the repository's required CI gates pass for its exact head, including the installed-package/editor consumer path. Once merged, Phase 13 is considered complete and the next roadmap dependency is Phase 14 — Content pipeline 2.0.

# Genre support roadmap

Lorenzo2D is growing toward four officially supported development paths without splitting into
four separate engines:

1. point-and-click movement with navigation;
2. orthogonal top-down games with free or grid-step movement;
3. side-view platformers;
4. isometric games with navigation and placement.

Support claims are governed by [`support-matrix.md`](support-matrix.md). A planned phase is not an
available feature.

## Phases

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | baseline, versioning, architecture, support policy, deterministic test helper, benchmark reporting, and licensing | Implemented |
| 1 | general input actions, input contexts, controllers, and pointer abstraction | Implemented |
| 2 | physics/world queries plus character-oriented collider foundations | Implemented |
| 3 | render context, coordinate projections, and depth sorting | Implemented |
| 4 | layered tile/content model and extensible level serialization | Implemented |
| 5 | shared collision-aware character motor | Implemented |
| 6 | top-down and grid-step controllers with a focused template | Implemented |
| 7 | platformer motor, slopes, one-way platforms, and moving platforms | Implemented |
| 8 | navigation grid, deterministic A*, path following, and local avoidance | Implemented |
| 9 | isometric projection, picking, culling, depth, and placement | Implemented |
| 10 | four project templates, UI, audio, saves, documentation, and 1.0 hardening | Implemented |

The original 1.0 desktop baseline is complete. Continued development is tracked in
[the post-1.0 roadmap](post-1.0-roadmap.md), beginning with Phase 11 production hardening and
continuing through the planned Lorenzo2D 2.0 preparation phase.

### Current post-1.0 implementation pointer

Phase 13 tooling/editor foundation is complete. Slices 13.1 through 13.11 are implemented, including
the end-to-end authoring tutorial and CI-backed authoring regression recorded in
[`phase13-completion.md`](phase13-completion.md).

**Phase 14 — Content pipeline 2.0 is now in progress.** Phase 14.1 establishes stable source-asset
metadata on top of the existing serialized `AssetId` contract: typed source descriptors,
project-relative paths, importer/settings metadata, declared dependencies, deterministic registry
ordering, bounded validation, transactional replacement semantics, headless regression coverage,
and installed-package consumption. The contract and boundaries are documented in
[`asset-metadata.md`](asset-metadata.md).

The next Phase 14 dependency is **14.2 — deterministic metadata manifests and import/cook
contracts**, which can build persistence and cooked-output identity on the 14.1 descriptor model
without changing runtime `AssetManager` ownership semantics.

## Phase completion rule

A phase is complete only when its public API, validation, tests, example integration,
documentation, installed-package consumption, and relevant diagnostic benchmark all land together.
Large phases should be delivered as small vertical pull requests that leave the default branch
buildable after every merge.

## Dependency order

Input, physics queries, and render projection can evolve independently after phase 0. The shared
content and character-movement foundations follow. Top-down and platformer support can then proceed
independently, while navigation builds on queries, tile data, and character movement. Isometric
support reuses the projection, tilemap, navigation, and depth contracts instead of duplicating them.

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
| 0 | baseline, versioning, architecture, support policy, deterministic test helper, benchmark reporting, and licensing | In progress; license choice pending |
| 1 | general input actions, input contexts, controllers, and pointer abstraction | Implemented |
| 2 | physics/world queries plus character-oriented collider foundations | Implemented |
| 3 | render context, coordinate projections, and depth sorting | Implemented |
| 4 | layered tile/content model and extensible level serialization | Planned |
| 5 | shared collision-aware character motor | Planned |
| 6 | top-down and grid-step controllers with a focused template | Planned |
| 7 | platformer motor, slopes, one-way platforms, and moving platforms | Planned |
| 8 | navigation grid, deterministic A*, path following, and local avoidance | Planned |
| 9 | isometric projection, picking, culling, depth, and placement | Planned |
| 10 | four project templates, UI, audio, saves, documentation, and 1.0 hardening | Planned |

Optional online multiplayer, mobile lifecycle/distribution, scripting, and a native editor follow
the desktop genre baseline rather than blocking it.

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

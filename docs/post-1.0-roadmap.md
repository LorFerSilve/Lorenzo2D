# Lorenzo2D post-1.0 roadmap

Lorenzo2D 1.0.0 completed the original desktop genre baseline. The post-1.0 roadmap shifts the
priority from feature breadth toward production hardening, game-development productivity, scalable
content workflows, and carefully staged platform expansion.

This roadmap covers Phases 11 through 22. It is intentionally ordered so that new complexity is
added only after the underlying diagnostics, compatibility contracts, and authoring workflows can
support it.

Support claims remain governed by [support-matrix.md](support-matrix.md). A phase marked
`Planned` is not an available feature.

## Release bands

| Release band | Primary goal | Phases |
| --- | --- | --- |
| 1.1–1.3 | reliability, diagnostics, and rendering quality | 11–12 |
| 1.4–1.7 | authoring productivity and game-development systems | 13–18 |
| 1.8+ | scale, additional platforms, and networking foundations | 19–21 |
| 2.0 | deliberate breaking cleanup after sustained 1.x usage | 22 |

Version numbers are indicative rather than mandatory. A phase should land when its definition of
done is satisfied, not merely to match a version target.

## Phase overview

| Phase | Scope | Status |
| --- | --- | --- |
| 11 | production hardening, diagnostics, profiling, fuzzing, stress validation, and CI policy | Complete |
| 12 | rendering 2.0: shaders, materials, render targets, batching, post-processing, and render diagnostics | In progress |
| 13 | tooling and editor foundation using installed public engine API | Planned |
| 14 | content pipeline 2.0: asset metadata, import/cook pipeline, dependency graph, and rebuild cache | Planned |
| 15 | gameplay framework and scripting boundary | Planned |
| 16 | UI 2.0: layout, focus, text input, widgets, themes, and scalable navigation | Planned |
| 17 | audio 2.0: mixer hierarchy, fades, priorities, spatial audio, snapshots, and music transitions | Planned |
| 18 | save/data evolution: slots, migrations, metadata, integrity, async save, and backup policy | Planned |
| 19 | advanced navigation and world systems: hierarchical search, crowds, dynamic obstacles, and chunk streaming | Planned |
| 20 | platform expansion: macOS first, then web, with mobile evaluated separately | Planned |
| 21 | networking architecture: identity, snapshots, authority, prediction/reconciliation, and deterministic boundaries | Planned |
| 22 | Lorenzo2D 2.0 preparation and controlled breaking cleanup | Planned |

---

## Phase 11 — Production hardening, diagnostics, and stress validation

### Goal

Turn the 1.0 feature-complete desktop baseline into a production-observable engine whose failure
modes, long-running behavior, performance envelopes, and determinism can be measured rather than
guessed.

This phase should be completed before adding another large subsystem.

### Implementation progress

- **11.1 Diagnostics foundation:** named bounded profiler, rolling frame aggregation, standard
  counters, deterministic report format, tests, example, documentation, and package-consumer
  coverage are implemented.
- **11.2 Deterministic replay foundation:** bounded input/state traces, canonical 64-bit hashing,
  first-divergence detection, long-trace regressions, and package-consumer coverage are implemented.
- **11.3 Subsystem diagnostics wiring:** additive adapters connect scene, physics, navigation,
  render/tilemap, asset, audio, and persistence telemetry to the standard counters, with stable
  profiler scope names and focused regression coverage.
- **11.4 Stress/soak validation foundation:** a bounded headless runner now provides smoke,
  representative standard, and accelerated soak profiles across tile streaming, renderables and
  handles, dynamic physics/query churn, navigation replans, fixed-step replay, asset lifetime, UI,
  audio, and near-limit save persistence. Pull-request CI executes the smoke profile, while heavier
  profiles remain available manually and now run through the scheduled Phase 11.7 workflow with
  retained JSON reports.
- **11.5 Fuzz/malformed-input validation:** a deterministic seedable property runner covers
  save-game JSON, level/prefab deserialization, Tiled JSON, physics queries, navigation-grid
  bounds/costs, UI bounds/pointer sequences, and resource lookup. Level/Tiled parsers now enforce
  configurable byte envelopes, relative resource lookup cannot escape configured roots, and
  parser failures preserve prior destination state.
- **11.6 Integrated deterministic replay:** an 8,192-tick headless regression now drives canonical
  fixed-tick input through Scene/ECS, physics/query snapshots, navigation replans, and UI state.
  It proves equivalence across fixed-frame cadences and detects exact first input/state divergence
  ticks using a documented quantized floating-point state-hash contract.
- **11.7 Nightly extended validation:** a scheduled read-only workflow now runs Release standard
  and soak stress, ASan/UBSan standard stress, four fixed-seed 2,048-case fuzz passes, and
  benchmark trend capture. JSON/CSV/log artifacts are retained for 21 days without turning noisy
  wall-clock benchmark values into hard pass/fail thresholds.
- **11.8 Public API/failure-path audit:** all 77 installed headers were audited for ownership,
  lifetime, mutability, silent fallback, validation, units, transactionality, workload bounds, and
  concurrency. Borrowed input snapshots reject temporaries, checked compatibility/render/camera
  variants expose failure, level instantiation now rolls back Scene objects created after a pre-call
  identity snapshot even when callbacks compact the Scene, and retained 1.x escape hatches have
  explicit contracts. The measured Phase 11 production baseline and limits
  are published.
- **Repository policy:** `master` is protected by an active repository ruleset requiring pull
  requests, all seven CI gates, an up-to-date branch, and blocking force-pushes/deletion.
- **Phase 11 status: complete.** The production-hardening definition of done is satisfied without
  promoting support claims beyond the evidence recorded in the support matrix.

### Scope

#### Diagnostics and profiling

- Add a lightweight profiling API with named scopes and frame aggregation.
- Track fixed-step, render, physics, navigation, asset, UI, and audio timings.
- Expose frame-time statistics such as current, rolling average, maximum, and percentile-friendly
  sample windows.
- Add engine counters for:
  - active entities/components;
  - collider/query counts;
  - navigation expansions/replans;
  - draw calls/rendered items;
  - loaded/live assets;
  - active audio voices;
  - save/load byte counts and timings.
- Add a diagnostic snapshot/report format suitable for attaching to bug reports.
- Keep diagnostics optional and low-overhead in release builds.

#### Stress and soak validation

Add deterministic or bounded stress scenarios for:

- large tilemaps and repeated streaming-region changes;
- thousands of renderables;
- hundreds of dynamic colliders;
- repeated physics queries and contact churn;
- hundreds of navigation agents and replans;
- long-running fixed-step simulation;
- asset load/unload/reload cycles;
- UI interaction churn;
- audio voice lifecycle churn using the null playback device;
- large but valid save documents;
- repeated save replacement and load cycles.

Long-running soak scenarios should detect crashes, leaks, invalid handles, counter drift, and
determinism divergence.

#### Fuzzing and malformed-input validation

Introduce targeted fuzz/property tests for:

- level and prefab deserialization;
- save-game JSON;
- Tiled JSON import;
- physics geometry/query inputs;
- navigation-grid bounds/costs;
- UI button bounds and pointer event sequences;
- asset/resource lookup input.

Fuzz targets must enforce workload limits and must never turn malformed input into unbounded work.

#### Determinism and replay

- Extend deterministic replay tests from short regression cases to long simulations.
- Capture replay inputs and a compact deterministic state hash.
- Detect first divergence tick.
- Document which subsystems are expected to be deterministic and which presentation services are
  explicitly outside the deterministic contract.

#### CI and repository policy

- Add branch protection for `master` with mandatory CI.
- Add a scheduled/nightly extended validation workflow for expensive stress/fuzz scenarios.
- Keep pull-request CI bounded and fast enough for normal development.
- Add benchmark/regression trend artifacts without using unstable microbenchmarks as hard pass/fail
  gates unless thresholds are demonstrably reliable.
- Audit warnings-as-errors, sanitizers, package consumers, and dependency pinning.

#### 1.0 API and failure-path audit

Review all installed public headers for:

- unclear ownership;
- accidental mutability;
- lifetime hazards;
- silent fallback behavior;
- inconsistent validation;
- ambiguous units/coordinate spaces;
- exception-safety and transactionality;
- missing workload limits;
- concurrency assumptions.

No breaking cleanup should be performed casually in 1.x. Prefer compatible additions or
deprecations.

### Diagnostic scenarios

At minimum, publish repeatable scenarios for:

1. 10k+ visible/streamed tiles;
2. 1k+ renderable scene objects;
3. 500+ colliders with repeated queries;
4. 250+ navigation agents;
5. repeated asset replacement/unload loops;
6. save documents near configured limits;
7. a multi-hour or accelerated deterministic simulation soak.

The exact counts may be adjusted if measurements show a better representative workload, but the
rationale must be documented.

### Definition of done

Phase 11 is complete only when:

- profiling/diagnostic APIs are public and documented;
- stress scenarios are reproducible;
- deterministic replay identifies divergence reliably;
- fuzz/malformed-input targets cover persistence/import boundaries;
- nightly CI runs the expensive validation suite;
- `master` is protected by required CI;
- package consumers remain green;
- no unresolved sanitizer or reproducible determinism failures remain;
- performance results and known production limits are documented;
- the support matrix is updated only where evidence justifies a promotion.

All Phase 11 definition-of-done items are satisfied by the 11.1-11.8 implementation slices,
repository policy, installed-package consumers, and the published production baseline.

---

## Phase 12 — Rendering 2.0

### Goal

Move Lorenzo2D beyond basic sprite/tile rendering without introducing a heavyweight general-purpose
render graph.

### Implementation progress

- **12.1 Shader/material foundation:** public `Shader2D` adds bounded transactional in-memory
  compilation and an explicit validated uniform layout; `Material2D` adds typed uniform/texture
  state, required-uniform completeness, blend configuration, and transactional RenderStates
  publication. `SpriteRenderer` can opt into a material while retaining the original simple draw
  path when no material is bound. Focused xvfb regressions and package-consumer coverage are included.
- **12.2 Render surfaces/off-screen foundation:** public `RenderSurface2D` adds bounded
  transactional off-screen allocation, borrowed target/texture access, generation tracking, and
  material-aware compositing into any SFML render target. Camera and the legacy post-process overlay
  gain additive generic-target overloads. Scene/Component's existing RenderWindow virtual contract
  remains unchanged until pass orchestration can provide an explicit migration boundary.
- **12.3 Configurable render-pass orchestration:** public `RenderPipeline2D` adds bounded
  deterministic pass ordering, explicit backbuffer/surface outputs, read-only declared surface
  inputs, clear/present policies, full-sequence preflight before side effects, stable failure
  reporting, reentrancy/mutation guards, and an explicit RenderWindow+Scene compatibility bridge for
  legacy Component rendering. The pipeline deliberately performs no automatic dependency scheduling.
- **12.4 Shader-based post-processing:** public `ShaderPostProcessChain2D` adds bounded ordered
  full-screen shader passes over published `RenderSurface2D` input, lazy reusable ping-pong
  workspaces, explicit preflight/failure reporting, deterministic pass reordering/enabling, and
  direct `RenderPipeline2D` integration. The lightweight `PostProcessStack2D` color-overlay path
  remains available unchanged, and a public Phase 12 example demonstrates the new workflow.
- **12.5 Ordered sprite batching/atlas-friendly submission:** public `SpriteBatch2D` adds a bounded
  opt-in six-vertex sprite submission path that coalesces only adjacent texture/material-compatible
  items, preserving deterministic visible ordering. Atlas rectangles, stable failure reporting,
  structural/actual draw statistics, diagnostics integration, a public example, installed-package
  consumption, and nightly individual-vs-batched atlas trend benchmarks are included.
- **12.6.1 Camera/layer composition foundation:** public `RenderComposition2D` adds a bounded,
  deterministic CPU-side composition contract with up to 16 named entries. Each entry borrows one
  `Camera2D`, selects a render pass, an inclusive integer layer range, and a normalized viewport.
  `RenderContext2D` gains an all-layers-compatible range and `RenderQueue2D` filters its existing
  deterministic pass snapshot by that range. Configuration validation, ordering, disabled-entry
  behavior, and generated camera views/contexts are covered by focused headless regressions without
  changing existing simple rendering behavior.
- **12.6.2 Multi-camera composition execution:** `RenderComposition2D` now executes its configured
  entries in order against a generic `sf::RenderTarget` callback or the legacy `RenderWindow` +
  `Scene` bridge. Execution preflights enabled entries, snapshots camera views/contexts before the
  first callback, applies normalized viewports without mutating borrowed cameras, restores the
  incoming target view on success/failure/exception unwinding, rejects reentrant execution and
  configuration mutation while executing, and preserves original entry indices while skipping
  disabled entries. Split-screen/minimap-style repeated Scene rendering therefore composes through
  the existing deterministic pass/layer queue while clear policy, surfaces, dependencies,
  post-processing, publication, and presentation remain `RenderPipeline2D`/application concerns.
  Focused Xvfb regressions and installed/add_subdirectory consumer coverage validate the execution
  and stable failure/result API.
- Remaining Phase 12 work is the complete render-statistics surface, including submitted primitive,
  batch, material/shader-switch, and culling telemetry across the coherent presentation pipeline.

### Scope

- Public shader abstraction.
- Material abstraction with explicit shader/uniform/texture state.
- Uniform validation and typed setters.
- Render targets and off-screen rendering.
- Configurable render passes.
- Post-processing chain.
- Blend-mode configuration.
- Sprite/tile batching where state compatibility permits it.
- Texture-atlas-friendly rendering path.
- Camera/layer composition.
- Render statistics:
  - draw calls;
  - submitted primitives;
  - batches;
  - material/shader switches;
  - culled items.
- Optional 2D lighting foundation only if it fits the render-pass model without distorting the core.

### Constraints

- Preserve existing simple rendering workflows.
- Do not require games to adopt materials for trivial sprites.
- Avoid editor-specific dependencies.
- Keep render ordering and isometric projected-Y behavior deterministic.

### Definition of done

- New public rendering API and validation.
- Focused shader/material/render-target tests.
- Installed-package example.
- At least one post-processing example.
- Batching correctness and diagnostic benchmarks.
- Documentation for lifetime, pass ordering, coordinate spaces, and failure modes.

---

## Phase 13 — Tooling and editor foundation

### Goal

Create practical authoring tools without making the engine depend on editor code.

### Architectural rule

The editor must consume Lorenzo2D through public APIs wherever practical. Engine modules must never
depend on the editor.

### Initial editor scope

- Scene hierarchy.
- Entity selection.
- Component inspector.
- Transform editing and gizmos.
- Asset browser.
- Tilemap painting.
- Collider visualization/editing.
- Navigation-grid visualization.
- Animation preview.
- Scene open/save.
- Play/test workflow.
- Undo/redo command foundation.

### Definition of done

- Editor is a separate target/application.
- Existing level formats remain readable by runtime-only builds.
- Undo/redo is deterministic for supported editor operations.
- A tutorial creates and runs a small level without hand-editing JSON.
- Engine package remains consumable without editor dependencies.

---

## Phase 14 — Content pipeline 2.0

### Goal

Replace ad-hoc runtime asset loading with an optional deterministic source-to-runtime content
pipeline suitable for larger projects.

### Scope

- Stable asset metadata/IDs.
- Source asset descriptors.
- Import settings.
- Cooked/runtime asset output.
- Dependency graph.
- Incremental invalidation.
- Content build cache.
- Deterministic manifests.
- Texture atlas generation.
- Audio preprocessing where useful.
- Editor integration.
- Background rebuilds without publishing incomplete generations.

### Definition of done

- Rebuilding unchanged content is cache-stable.
- Dependency invalidation is tested.
- Cooked content is reproducible for the same inputs/toolchain.
- Runtime resource lookup can use a manifest.
- Existing simple resource-root workflow remains available.

---

## Phase 15 — Gameplay framework and scripting boundary

### Goal

Provide reusable gameplay composition before exposing engine internals directly to a scripting
language.

### Native gameplay layer

- Typed gameplay event bus.
- Commands.
- Triggers/interactions.
- Timers.
- Reusable state-machine foundation.
- Scene/gameplay signals.
- Explicit lifecycle hooks.

### Scripting

Prefer Lua or Luau unless a later evaluation identifies a stronger fit.

The scripting boundary should expose a curated gameplay API rather than raw internal engine
objects.

### Definition of done

- Native event/command layer is independently useful.
- Script VM lifetime and error handling are explicit.
- Scripts cannot retain invalid engine references silently.
- Hot reload behavior is documented.
- A scripted example uses installed public API/bindings.
- Determinism implications are documented.

---

## Phase 16 — UI 2.0

### Goal

Grow the Phase 10 button baseline into a complete runtime game UI system.

### Scope

- Panels and images.
- Labels.
- Horizontal/vertical layout.
- Anchors.
- Margins/padding.
- Resolution-independent sizing.
- Buttons.
- Checkboxes/toggles.
- Sliders.
- Scroll containers.
- Text input.
- Keyboard/gamepad focus.
- Focus traversal.
- Themes/styles.
- Disabled/hovered/pressed/focused states.
- Optional accessibility metadata foundation.

### Definition of done

- Layout is deterministic and tested across resolutions.
- Pointer and keyboard/gamepad focus coexist predictably.
- Text input handles composition constraints appropriate to SFML/platform support.
- A settings/menu example is fully built with public UI API.

---

## Phase 17 — Audio 2.0

### Goal

Turn the Phase 10 playback service into a game-scale mixer and transition system.

### Scope

- Mixer hierarchy, for example:
  - Master
  - Music
  - SFX
  - UI
  - Ambient
  - Dialogue
- Fades and crossfades.
- Music playlists/state transitions.
- Voice limits.
- Priority and voice stealing.
- Persistent mixer settings.
- Audio snapshots.
- Ducking.
- Spatial/positional 2D audio.
- Category mute/solo where useful.

### Definition of done

- Voice stealing is deterministic for equal-priority cases.
- Bus/snapshot transitions are tested.
- Null-device regression coverage remains available.
- Music transition example and documentation are included.

---

## Phase 18 — Save and data evolution

### Goal

Build migration and user-facing save management on top of the safe Phase 10 persistence envelope.

### Scope

- Save slots/profiles.
- Save metadata.
- Migration registry.
- Revision-to-revision migration chains.
- Backup retention policy.
- Integrity checks.
- Optional compression.
- Asynchronous save jobs.
- Autosave coordination.
- Recovery from interrupted writes.
- Migration test fixtures.

### Definition of done

- Old supported schema revisions can be migrated through tested chains.
- Failed migrations preserve the original save.
- Slot metadata can be read without loading full game state where practical.
- Async save lifetime/shutdown behavior is deterministic and documented.

---

## Phase 19 — Advanced navigation and world systems

### Goal

Scale current tile/physics navigation and world content to larger maps and larger agent counts.

### Scope

- Hierarchical A* or equivalent coarse/fine search.
- Chunked navigation data.
- Dynamic obstacle updates.
- Cost layers.
- Flow fields where beneficial.
- Crowd/local congestion management.
- Deadlock diagnostics.
- Large-world chunk streaming.
- Async chunk preparation where safe.
- Navigation/world-streaming observability.

### Definition of done

- Large-map search demonstrates a measured improvement over flat search for representative cases.
- Streaming never exposes partially initialized chunks.
- Agent behavior remains bounded under dense congestion.
- Existing small-map deterministic A* remains available.

---

## Phase 20 — Platform expansion

### Goal

Expand only after the desktop 1.x baseline is production-observable.

### Order

1. macOS;
2. web/Emscripten;
3. evaluate Android/iOS as a separate commitment.

### macOS

- Add CI.
- Validate installed package.
- Validate all four starters.
- Audit filesystem, audio, rendering, input, and packaging.

### Web

Web support requires explicit handling for:

- Emscripten build model;
- browser filesystem;
- asynchronous asset access;
- audio activation restrictions;
- threading constraints;
- save persistence;
- timing/event loop integration.

### Mobile evaluation

Do not declare mobile support until lifecycle, touch, orientation, app suspension, virtual controls,
filesystem, and GPU constraints have explicit designs.

### Definition of done

A platform is not called supported until its build, tests, package-consumer path, starter workflow,
and documented platform caveats are in CI.

---

## Phase 21 — Networking architecture

### Goal

Introduce multiplayer foundations only after identity, serialization, diagnostics, and deterministic
boundaries are mature.

### Scope

- Stable network entity identity.
- Snapshot serialization.
- Input/command replication.
- Authoritative server model.
- State interpolation.
- Client prediction.
- Reconciliation.
- Network time/tick model.
- Bandwidth diagnostics.
- Snapshot delta strategy.
- Deterministic simulation boundary documentation.
- Replay/debug tooling for network sessions.

### Constraints

- Do not begin with a broad “multiplayer API”.
- Separate transport concerns from game-state replication.
- Networking must not make single-player engine paths depend on a network runtime.

### Definition of done

- Minimal authoritative client/server sample.
- Prediction/reconciliation regression tests.
- Reproducible latency/loss simulation.
- Snapshot compatibility/versioning documented.
- Diagnostics identify bandwidth and correction behavior.

---

## Phase 22 — Lorenzo2D 2.0 preparation

### Goal

Use sustained 1.x evidence to perform deliberate breaking cleanup rather than accumulating accidental
compatibility debt forever.

### Candidate work

- Remove APIs deprecated during 1.x.
- Normalize inconsistent naming or ownership contracts.
- Remove obsolete serializer formats only when migration tooling exists.
- Revisit public dependency boundaries.
- Evaluate a newer C++ language baseline.
- Consolidate duplicated compatibility adapters.
- Tighten support claims using actual production evidence.
- Write a 1.x -> 2.0 migration guide.

### Entry criteria

Do not start Phase 22 merely because Phase 21 is complete. It should begin only when real 1.x usage
has produced enough evidence that specific breaking changes are worth their migration cost.

### Definition of done

- Every breaking change is enumerated.
- Migration guidance exists.
- 1.x persisted data has a supported conversion path where practical.
- All examples/templates are migrated.
- 2.0 package consumers validate the new contract.
- The support matrix reflects evidence rather than version-number expectations.

---

## Cross-phase engineering rules

The Phase 0–10 completion rule remains in force.

A phase is complete only when its:

- public API;
- validation and failure semantics;
- regression tests;
- example/template integration;
- documentation;
- installed-package consumption;
- relevant diagnostics/benchmarks

land together.

Additional post-1.0 rules:

1. **Compatibility first.** 1.x changes should be backward compatible unless an exceptional
   correctness/security issue is documented.
2. **Evidence before support claims.** Version numbers do not automatically promote experimental
   paths to supported or production-tested.
3. **Measure before optimizing.** New performance complexity must be justified by diagnostics.
4. **No hidden unbounded work.** Parsers, serializers, navigation, content pipelines, and network
   decoders require workload limits.
5. **Transactional publication.** Failed loads, imports, cooks, saves, or reloads must preserve the
   last valid state.
6. **Public API drives tools.** Editor/templates/examples should consume the same contracts available
   to external games.
7. **Keep the default branch buildable.** Large phases should be delivered as vertical pull requests
   with green CI after every merge.

## Immediate next step

Continue **Phase 12 — Rendering 2.0** with **12.7 complete render diagnostics and statistics**.
Camera/layer composition is now configured and executable through 12.6.1/12.6.2; the next slice
should unify draw calls, submitted primitives, batches, material/shader switches, and culled-item
telemetry across the existing sprite, tilemap, pipeline, post-process, and composition paths without
disturbing deterministic ordering or the existing simple rendering path.

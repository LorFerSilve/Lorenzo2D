# Lorenzo2D regression tests

The focused regression executables use a small first-party harness. Forty-three focused suites
share assertion and named-test execution support through `TestSupport.hpp`. The separately
registered `Lorenzo2DStressValidation` and `Lorenzo2DFuzzValidation` executables are
self-reporting Phase 11 runners. Stress validation supports standard/soak profiles and JSON output;
fuzz validation supports deterministic seed/case/scenario reproduction outside CTest.
Physics and renderer assertions use the same value-rich diagnostics while
keeping their subsystem comparison tolerances explicit in the owning source.
Value-rich equality and approximate assertions, scalar and explicit-epsilon 2D
comparisons, and temporary-file cleanup are centralized there. Target creation,
compiler warnings, sanitizer instrumentation, CTest registration, labels, and
timeouts are centralized in `tests/CMakeLists.txt`.

## Suites

| Target | Primary coverage | Runtime partition | Timeout |
| --- | --- | --- | --- |
| `Lorenzo2DTestSupportTests` | Harness diagnostics, explicit tolerances, 2D comparison, and temporary-file cleanup | `headless` | 30 s |
| `Lorenzo2DVersionTests` | Generated build/install version constants and macros | `headless` | 30 s |
| `Lorenzo2DApiFailureAuditTests` | Borrowed input lifetime, checked compatibility APIs, projection/camera failure reporting, and transactional level rollback | `headless` | 60 s |
| `Lorenzo2DDiagnosticsTests` | Bounded profiler configuration, frame aggregation, counters, scoped timing, and deterministic reports | `headless` | 30 s |
| `Lorenzo2DReplayTests` | Bounded replay capture, canonical hashing, and first-divergence detection | `headless` | 30 s |
| `Lorenzo2DIntegratedReplayTests` | 8,192-tick Scene/ECS, physics, navigation, UI, frame-cadence equivalence, and exact divergence detection | `headless` | 120 s |
| `Lorenzo2DSubsystemDiagnosticsTests` | Scene, physics, navigation, render, asset, audio, and persistence counter adapters | `headless` | 60 s |
| `Lorenzo2DStressValidation` | Bounded smoke stress across tile streaming, renderables/handles, dynamic physics, navigation replans, fixed-step replay, assets, UI, audio, and saves | `headless` | 180 s |
| `Lorenzo2DFuzzValidation` | Deterministic malformed-input/property validation for save/level/Tiled parsing, physics, navigation, UI, and resource lookup | `headless` | 120 s |
| `Lorenzo2DCharacterMotorTests` | Sweep/slide, overlap recovery, slopes, contacts, capsule movement, moving platforms, and replay determinism | `headless` | 120 s |
| `Lorenzo2DTopDownControllerTests` | Validation, analog acceleration/deceleration, diagonal normalization, wall sliding, facing, failures, and replay determinism | `headless` | 120 s |
| `Lorenzo2DGridStepControllerTests` | Direction ties, alignment, smooth steps, transactional blocking, turn buffering, rollback, failures, and replay determinism | `headless` | 120 s |
| `Lorenzo2DPlatformerControllerTests` | Run/jump policy, one-way drop-through, transactional steps, moving platforms, failures, and replay determinism | `headless` | 120 s |
| `Lorenzo2DNavigationGridTests` | Grid/tile/physics baking, coordinates, costs, deterministic A*, diagonal policy, limits, and failures | `headless` | 120 s |
| `Lorenzo2DNavigationAgentTests` | Path following, arrival, stale paths, stuck/repath signaling, local avoidance, failures, and replay determinism | `headless` | 120 s |
| `Lorenzo2DIsometricTests` | Projection, picking, placement, culling, and deterministic projected ordering | `headless` | 60 s |
| `Lorenzo2DCoreTimingTests` | Identity contracts, fixed-step scheduling, and transform interpolation | `headless` | 60 s |
| `Lorenzo2DInputTests` | Device codes, typed actions, layouts, deadzones, contexts, fixed-tick edges, reconnects, and pointer projection | `headless` | 30 s |
| `Lorenzo2DEcsSceneTests` | Component mutation, activation, scene dispatch, handles, indexing, and deferred destruction | `headless` | 90 s |
| `Lorenzo2DPhysicsIntegrationTests` | Scene-to-physics fixed-tick participation and render-cadence independence | `headless` | 90 s |
| `Lorenzo2DTilemapTests` | Atlas mappings, chunk statistics, view culling, collision merging, reload ownership, moves, and file loading | `xvfb` | 90 s |
| `Lorenzo2DTimingAccountingTests` | Cumulative fixed-step and frame-clamp accounting | `headless` | 30 s |
| `Lorenzo2DPhysicsBodyTests` | Body configuration, integration, impulses, restitution, and friction | `headless` | 90 s |
| `Lorenzo2DPhysicsCollisionTests` | Manifolds, filters, sensors, contacts, grounded state, and extreme values | `headless` | 120 s |
| `Lorenzo2DPhysicsWorldTests` | Scene identity, reset behavior, collider participation, stacks, and legacy response | `headless` | 90 s |
| `Lorenzo2DPhysicsBroadPhaseTests` | Uniform-grid boundaries, deduplication, fallback, equivalence, and telemetry | `headless` | 120 s |
| `Lorenzo2DPhysicsQueryTests` | Deterministic queries, filtering, shape casts, snapshots, new collider manifolds, and invalid input | `headless` | 120 s |
| `Lorenzo2DPhysicsTilemapTests` | Physics continuity across merged tile-map collision geometry | `headless` | 90 s |
| `Lorenzo2DPhysicsStabilityTests` | Long-horizon contacts, stacks, determinism, broad-phase equivalence, and tunnelling baseline | `headless` | 240 s |
| `Lorenzo2DPhysicsAdvancedTests` | CCD, angular response, sleeping, warm starting, and joint behavior | `headless` | 120 s |
| `Lorenzo2DAssetTests` | Font and texture handle lifetime, registries, and renderer leases | `xvfb` | 60 s |
| `Lorenzo2DAnimationTests` | Clip validation, atlas frames, animator timing, looping, pause, speed, and completion | `xvfb` | 60 s |
| `Lorenzo2DResourceTests` | Ordered roots, absolute paths, executable-relative lookup, and asset-manager integration | `headless` | 30 s |
| `Lorenzo2DSerializationTests` | Prefab validation, versioned level round trips, transactional rejection, files, and ECS instantiation | `xvfb` | 60 s |
| `Lorenzo2DSaveTests` | Versioned save JSON, workload limits, transactional file replacement, and malformed input | `headless` | 30 s |
| `Lorenzo2DUiTests` | Button bounds, pointer transitions, capture, overlap ordering, and invalid input | `headless` | 30 s |
| `Lorenzo2DAudioTests` | Null-device playback options, voice lifecycle, buses, and sound-buffer asset binding | `headless` | 30 s |
| `Lorenzo2DMaterialTests` | Shader compilation/reload, uniform-layout validation, typed material state, shared-shader reset behavior, blend configuration, and optional SpriteRenderer material binding; GPU cases self-skip when the runner reports no shader capability | `xvfb` | 60 s |
| `Lorenzo2DRenderSurfaceTests` | Bounded/transactional off-screen target allocation, pixel publication, surface compositing, self-feedback rejection, generic camera targeting, and post-process targeting | `xvfb` | 60 s |
| `Lorenzo2DRenderPipelineTests` | Bounded pass configuration, deterministic ordering/reordering, preflight side-effect isolation, read-only surface dependencies, publish/present chaining, callback/reentrancy failure semantics, and the legacy Scene window bridge | `xvfb` | 60 s |
| `Lorenzo2DShaderPostProcessTests` | Bounded shader-chain configuration, unpublished/input failure isolation, no-pass copy behavior, view restoration, full-screen shader pixels, deterministic multi-pass ping-pong ordering/reuse, feedback rejection, and stable failure names | `xvfb` | 60 s |
| `Lorenzo2DSpriteBatchTests` | Bounded ordered submissions, atlas-rect validation, compatible coalescing, state-split ordering, pixel output, draw statistics, and stable failure names | `xvfb` | 90 s |
| `Lorenzo2DRenderCompositionTests` | Bounded camera/layer composition configuration, stable ordering/reordering, viewport and layer-range validation, camera context capture, and stable failure names | `headless` | 60 s |
| `Lorenzo2DRenderCompositionExecutionTests` | Ordered multi-camera execution, camera snapshots, normalized viewport application, disabled-entry behavior, target-view restoration, callback/reentrancy/mutation failure semantics, exception unwinding, and the legacy Scene layer bridge | `xvfb` | 60 s |
| `Lorenzo2DRendererTests` | Camera/transform numeric safety, contexts, projections, pass filtering, deterministic depth, sprite origins/flips, and physics independence | `xvfb` | 60 s |

The previous broad regression targets mixed unrelated core, scene, physics,
and tilemap behavior. Their individual cases are now owned by focused suites;
the physics fixtures shared by those suites live in
`physics/PhysicsTestSupport.hpp`. New regressions should be added to the
narrowest applicable suite or to a new focused executable.

## Shared support

`TestSupport.hpp` provides:

- `L2D_REQUIRE` with the existing line-based failure message;
- `L2D_REQUIRE_EQUAL`, which reports both expressions and values, including
  enum underlying values and vector-like `x`/`y` components;
- `L2D_REQUIRE_APPROX`, which reports values and the caller-supplied epsilon
  for scalar or vector-like values;
- `L2D_REQUIRE_APPROX_2D`, which explicitly compares `x` and `y` components
  with a caller-supplied epsilon and reports both vectors;
- `L2D_REQUIRE_DETERMINISTIC_REPLAY`, which executes a snapshot-returning
  replay twice and reports both results when they diverge;
- scalar `approximatelyEqual` overloads for `float` and `double`;
- `approximatelyEqual2D` for vector-like values with `x` and `y` members;
- `runTest` for named pass/fail reporting without aborting the remaining suite;
- `TemporaryFile`, which removes its generated file during destruction.

`Lorenzo2DTestSupportTests` directly verifies the harness failure messages,
enum and vector formatting, scalar/vector dispatch, explicit-epsilon behavior,
2D component comparison, deterministic replay comparison, and temporary-file
cleanup.
The original `L2D_REQUIRE` output remains unchanged for compatibility.

Every focused first-party regression executable uses the shared assertion and runner.
`Lorenzo2DStressValidation` and `Lorenzo2DFuzzValidation` intentionally use their own
scenario/report runners. Stress CTest registration executes only the bounded `smoke` profile;
fuzz CTest registration executes 96 cases per scenario with a fixed seed. Subsystem-specific fixtures and tolerance
choices remain in their owning source file. No approximate assertion selects an implicit subsystem tolerance:
callers must provide it explicitly. Physics therefore retains its established
`0.001f` policy, renderer retains `0.0001f`, and timing accounting uses
`0.000000001`. Physics and renderer now pass these policies directly to the
shared rich-diagnostic assertions instead of maintaining local comparison
overloads.

## Runtime partition labels

Every regression executable must declare exactly one runtime label:

- `headless`: CI unsets `DISPLAY` and `WAYLAND_DISPLAY` before execution.
- `xvfb`: CI executes each suite on its own fresh Xvfb server on Linux, preventing
  graphics-context state from leaking between test executables.

`Lorenzo2DAssetTests`, `Lorenzo2DAnimationTests`, `Lorenzo2DTilemapTests`,
`Lorenzo2DSerializationTests`, `Lorenzo2DMaterialTests`, `Lorenzo2DRenderSurfaceTests`,
`Lorenzo2DRenderPipelineTests`, `Lorenzo2DShaderPostProcessTests`,
`Lorenzo2DSpriteBatchTests`, `Lorenzo2DRenderCompositionExecutionTests`, and
`Lorenzo2DRendererTests` use the Xvfb partition because their current execution path may construct
or exercise SFML graphics resources. The remaining suites are executed with display variables
removed.

Subsystem labels such as `test-support`, `timing`, `ecs`, `scene`, `physics`,
`tilemap`, `assets`, `resources`, `serialization`, `animation`, `renderer`,
`version`, `compatibility`, and `determinism`
support focused local runs. A suite may have
several subsystem labels, but it must still have exactly one runtime partition
label.

## Common commands

Run every registered test:

```sh
ctest --test-dir build -C Debug --output-on-failure
```

Run the display-free partition on Linux:

```sh
unset DISPLAY
unset WAYLAND_DISPLAY
ctest --test-dir build -C Debug --output-on-failure -L headless
```

Run the Xvfb partition:

```sh
xvfb-run --auto-servernum \
  ctest --test-dir build -C Debug --output-on-failure -L xvfb
```

Run tests associated with a subsystem:

```sh
ctest --test-dir build -C Debug --output-on-failure -L physics
```

Reproduce or expand the Phase 11 malformed-input runner directly:

```sh
./build/tests/Lorenzo2DFuzzValidation --seed 0x4c324446555a5a31 --cases 96
./build/tests/Lorenzo2DFuzzValidation --seed 0x4c324446555a5a31 --cases 2048 --scenario tiled-json
```

Run the subsystem-integrated deterministic replay directly:

```sh
ctest --test-dir build -C Debug --output-on-failure -R Lorenzo2DIntegratedReplayTests
```

## Adding a suite

Register new executables through `l2d_add_regression_test` in
`tests/CMakeLists.txt`. Provide:

- one source file;
- a focused target name;
- exactly one of `headless` or `xvfb`;
- one or more subsystem labels;
- a practical timeout;
- private include directories only when a test intentionally exercises an
  internal implementation contract;
- optional `ARGUMENTS` when a registered executable needs a fixed bounded CTest invocation.

New focused suites should use `TestSupport.hpp` for the common harness instead
of copying assertion or test-runner implementations. Prefer value-rich
assertions when the compared values are streamable, and always pass subsystem
tolerances explicitly. Do not weaken assertions, disable warnings, or move a
test to `headless` merely to reduce CI setup. A runtime-partition change must
pass the actual no-display CI execution.

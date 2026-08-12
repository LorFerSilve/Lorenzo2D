# Lorenzo2D regression tests

The regression executables use a small first-party harness. All twenty-one suites
share assertion and named-test execution support through `TestSupport.hpp`.
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
| `Lorenzo2DCharacterMotorTests` | Sweep/slide, overlap recovery, slopes, contacts, capsule movement, moving platforms, and replay determinism | `headless` | 120 s |
| `Lorenzo2DCoreTimingTests` | Identity contracts, fixed-step scheduling, and transform interpolation | `headless` | 60 s |
| `Lorenzo2DInputTests` | Device codes, typed actions, layouts, deadzones, contexts, fixed-tick edges, reconnects, and pointer projection | `headless` | 30 s |
| `Lorenzo2DEcsSceneTests` | Component mutation, activation, scene dispatch, handles, indexing, and deferred destruction | `headless` | 90 s |
| `Lorenzo2DPhysicsIntegrationTests` | Scene-to-physics fixed-tick participation and render-cadence independence | `headless` | 90 s |
| `Lorenzo2DTilemapTests` | Atlas mappings, chunk statistics, view culling, collision merging, reload ownership, moves, and file loading | `headless` | 90 s |
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
| `Lorenzo2DSerializationTests` | Prefab validation, versioned level round trips, transactional rejection, files, and ECS instantiation | `headless` | 60 s |
| `Lorenzo2DRendererTests` | Camera/transform numeric safety, contexts, projections, pass filtering, deterministic depth, sprite origins/flips, and physics independence | `headless` | 60 s |

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

Every first-party regression executable uses the shared assertion and runner.
Subsystem-specific fixtures and tolerance choices remain in their owning source
file. No approximate assertion selects an implicit subsystem tolerance:
callers must provide it explicitly. Physics therefore retains its established
`0.001f` policy, renderer retains `0.0001f`, and timing accounting uses
`0.000000001`. Physics and renderer now pass these policies directly to the
shared rich-diagnostic assertions instead of maintaining local comparison
overloads.

## Runtime partition labels

Every regression executable must declare exactly one runtime label:

- `headless`: CI unsets `DISPLAY` and `WAYLAND_DISPLAY` before execution.
- `xvfb`: CI executes the suite through Xvfb on Linux.

`Lorenzo2DAssetTests` and `Lorenzo2DAnimationTests` use the Xvfb partition
because they construct SFML texture or font resources. The harness, core, ECS,
physics, tilemap, resource, serialization, timing, and renderer suites contain
no window, graphics-context, texture, or font construction and are executed
with display variables removed.

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

## Adding a suite

Register new executables through `l2d_add_regression_test` in
`tests/CMakeLists.txt`. Provide:

- one source file;
- a focused target name;
- exactly one of `headless` or `xvfb`;
- one or more subsystem labels;
- a practical timeout;
- private include directories only when a test intentionally exercises an
  internal implementation contract.

New focused suites should use `TestSupport.hpp` for the common harness instead
of copying assertion or test-runner implementations. Prefer value-rich
assertions when the compared values are streamable, and always pass subsystem
tolerances explicitly. Do not weaken assertions, disable warnings, or move a
test to `headless` merely to reduce CI setup. A runtime-partition change must
pass the actual no-display CI execution.

# Lorenzo2D regression tests

The regression executables use a small first-party harness. The four suites
extracted from the former broad source share assertion, approximate-comparison,
temporary-file, and named-test execution support through `TestSupport.hpp`.
Target creation, compiler warnings, sanitizer instrumentation, CTest
registration, labels, and timeouts are centralized in `tests/CMakeLists.txt`.

## Suites

| Target | Primary coverage | Runtime partition | Timeout |
| --- | --- | --- | --- |
| `Lorenzo2DCoreTimingTests` | Identity contracts, fixed-step scheduling, and transform interpolation | `headless` | 60 s |
| `Lorenzo2DEcsSceneTests` | Component mutation, activation, scene dispatch, handles, indexing, and deferred destruction | `headless` | 90 s |
| `Lorenzo2DPhysicsIntegrationTests` | Scene-to-physics fixed-tick participation and render-cadence independence | `headless` | 90 s |
| `Lorenzo2DTilemapTests` | Chunk statistics, view culling, collision merging, reload ownership, moves, and file loading | `headless` | 90 s |
| `Lorenzo2DTimingAccountingTests` | Cumulative fixed-step and frame-clamp accounting | `headless` | 30 s |
| `Lorenzo2DPhysicsTests` | Collision geometry, materials, contacts, solver behavior, and broad phase | `headless` | 180 s |
| `Lorenzo2DAssetTests` | Font and texture handle lifetime, registries, and renderer leases | `xvfb` | 60 s |
| `Lorenzo2DRendererTests` | Camera, transform, and renderer numeric contracts without GPU resources | `headless` | 60 s |

The previous `Lorenzo2DTests.cpp` target mixed unrelated core, scene, physics,
and tilemap regressions. Its individual test cases and assertion bodies are now
owned by the four focused suites above. New regressions should be added to the
narrowest applicable suite or to a new focused executable.

## Shared support

`TestSupport.hpp` provides:

- `L2D_REQUIRE` with the existing line-based failure message;
- scalar `approximatelyEqual` overloads for `float` and `double`;
- `runTest` for named pass/fail reporting without aborting the remaining suite;
- `TemporaryFile`, which removes its generated file during destruction.

Subsystem-specific fixtures and comparison policies remain in their owning
source file. The pre-existing timing-accounting, physics, asset, and renderer
executables retain their local harness code in this limited cycle; migrating
those large focused sources can be done independently without coupling it to the
broad-suite split. Physics and renderer vector comparisons also deliberately
retain their existing local tolerances instead of being hidden in a global
utility.

## Runtime partition labels

Every regression executable must declare exactly one runtime label:

- `headless`: CI unsets `DISPLAY` and `WAYLAND_DISPLAY` before execution.
- `xvfb`: CI executes the suite through Xvfb on Linux.

Only `Lorenzo2DAssetTests` currently uses the Xvfb partition because it
constructs SFML texture and font resources. The focused core, ECS, physics,
tilemap, timing, and renderer suites contain no window, graphics-context,
texture, or font construction and are executed with display variables removed.

Subsystem labels such as `timing`, `ecs`, `scene`, `physics`, `tilemap`,
`assets`, and `renderer` support focused local runs. A suite may have several
subsystem labels, but it must still have exactly one runtime partition label.

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
of copying assertion or test-runner implementations. Do not weaken assertions,
disable warnings, or move a test to `headless` merely to reduce CI setup. A
runtime-partition change must pass the actual no-display CI execution.

# Lorenzo2D regression tests

The test executables intentionally use a small first-party harness. Test target
creation, compiler warnings, sanitizer instrumentation, CTest registration,
labels, and timeouts are centralized in `tests/CMakeLists.txt`.

## Suites

| Target | Primary coverage | Runtime partition | Timeout |
| --- | --- | --- | --- |
| `Lorenzo2DTests` | Broad legacy coverage for core timing, ECS, scenes, fixed-step physics integration, and tilemaps | `xvfb` | 180 s |
| `Lorenzo2DTimingAccountingTests` | Cumulative fixed-step and frame-clamp accounting | `headless` | 30 s |
| `Lorenzo2DPhysicsTests` | Collision geometry, materials, contacts, solver behavior, and broad phase | `xvfb` | 180 s |
| `Lorenzo2DAssetTests` | Font and texture handle lifetime, registries, and renderer leases | `xvfb` | 60 s |
| `Lorenzo2DRendererTests` | Camera, transform, and renderer numeric contracts without GPU resources | `headless` | 60 s |

The broad `Lorenzo2DTests` executable is retained for compatibility. New
regressions should normally be added to an existing focused suite or to a new
focused executable instead of expanding the broad target further.

## Runtime partition labels

Every regression executable must declare exactly one runtime label:

- `headless`: CI unsets `DISPLAY` and `WAYLAND_DISPLAY` before execution.
- `xvfb`: CI executes the suite through Xvfb on Linux.

The `xvfb` partition is conservative. `Lorenzo2DAssetTests` genuinely constructs
SFML graphics resources. The broad and physics suites remain in the same
partition until they have dedicated no-display validation covering all of their
current code paths.

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

Do not weaken assertions, disable warnings, or move a test to `headless` merely
to reduce CI setup. A runtime-partition change should be supported by an actual
no-display execution in CI.

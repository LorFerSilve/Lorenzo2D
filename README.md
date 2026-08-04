# Lorenzo2D

Lorenzo2D is an experimental 2D game engine written in C++17 on top of
[SFML 3.1](https://www.sfml-dev.org/). The repository contains a static engine
library and a sandbox platformer that exercises the engine's current systems.

The project is intentionally small and readable. It is suitable for learning
engine architecture and iterating on core 2D systems, but it is not yet a
production-ready engine.

## Current features

- SFML application loop with fixed simulation ticks and bounded catch-up
- Previous/current transform interpolation for smooth presentation
- Frame timing, keyboard, mouse, and window events
- Action bindings with multiple keys per gameplay action
- Game objects, transforms, polymorphic components, tags, and deferred deletion
- Scenes, scene switching, object queries, and lifetime-aware object handles
- Circle and rectangle rendering plus texture-backed sprites
- Smooth bounded 2D camera, resize handling, follow behavior, and wheel zoom
- ASCII tilemap loading with chunked, view-culled rendering and merged collision geometry
- Static, kinematic, and dynamic rigid bodies with configurable gravity
- Circle/box collision manifolds, impulse response, friction, and restitution
- Collision layers, sensors, contact events, and physics debug drawing
- Lifetime-safe read-only font and texture handles with transactional named
  storage
- Debug overlay and independently switchable world, physics, and UI layers
- Regression tests for timing, transforms, scenes, camera and renderer numeric
  contracts, assets, physics, and tilemaps

## Requirements

- CMake 3.28 or newer
- A C++17 compiler (MSVC, GCC, or Clang)
- Git and an internet connection for the default first configure

SFML is fetched automatically and pinned to version 3.1.0. To use an installed
SFML 3.1 package instead, configure with `-DL2D_USE_SYSTEM_SFML=ON`.

On Debian or Ubuntu, install SFML's native graphics dependencies before the
first configure:

```sh
sudo apt-get update
sudo apt-get install xorg-dev xauth xvfb libharfbuzz-dev libfreetype-dev \
  libgl1-mesa-dev libegl1-mesa-dev libudev-dev
```

Package names differ on other Linux distributions.

## Build and test

From the repository root:

```sh
cmake -S . -B build -DL2D_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CMake presets provide matching developer configurations when Ninja is
available:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

`release`, `sanitize`, `lint`, `coverage`, and `benchmarks` presets keep
specialized build trees isolated under `build/<preset>`.

The sandbox executable is written to `build/bin`. Disable it for a test- or
library-only build with `-DL2D_BUILD_SANDBOX=OFF`.

`Lorenzo2DRendererTests` and `Lorenzo2DTimingAccountingTests` are explicitly
labeled `headless` and can run on Linux with `DISPLAY` and `WAYLAND_DISPLAY`
unset. The remaining Linux suite runs under Xvfb because the asset tests
exercise SFML graphics resources:

```sh
xvfb-run --auto-servernum \
  ctest --test-dir build -C Debug --output-on-failure
```

Useful configuration options:

| Option | Top-level default | Dependency-mode default | Purpose |
| --- | --- | --- | --- |
| `L2D_BUILD_SANDBOX` | `ON` | `OFF` | Build the interactive sandbox |
| `L2D_BUILD_TESTS` | `ON` | `OFF` | Build and register regression tests |
| `L2D_BUILD_BENCHMARKS` | `OFF` | `OFF` | Build the standalone performance benchmarks |
| `L2D_USE_SYSTEM_SFML` | `OFF` | `OFF` | Use an installed SFML package |
| `L2D_WARNINGS_AS_ERRORS` | `OFF` | `OFF` | Promote first-party warnings to errors |
| `L2D_ENABLE_ASAN` | `OFF` | `OFF` | Enable AddressSanitizer on GCC, Clang, or AppleClang |
| `L2D_ENABLE_UBSAN` | `OFF` | `OFF` | Enable UndefinedBehaviorSanitizer on GCC, Clang, or AppleClang |
| `L2D_ENABLE_COVERAGE` | `OFF` | `OFF` | Add GCC/Clang coverage instrumentation and a `coverage` report target |
| `L2D_ENABLE_CLANG_TIDY` | `OFF` | `OFF` | Run the repository clang-tidy policy while compiling first-party targets |
| `L2D_INSTALL` | `ON` | `OFF` | Generate install and CMake package export rules |

AddressSanitizer and UndefinedBehaviorSanitizer may be enabled independently or
together on supported compilers. The options instrument the engine, sandbox,
and regression executables and add the corresponding link flags. Unsupported
compiler combinations fail during configuration instead of silently producing
an uninstrumented build.

```sh
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DL2D_BUILD_SANDBOX=ON \
  -DL2D_BUILD_TESTS=ON \
  -DL2D_ENABLE_ASAN=ON \
  -DL2D_ENABLE_UBSAN=ON
cmake --build build-sanitize --parallel
```

### Developer quality tools

With clang-format available, top-level builds expose `format` and
`format-check` targets. The repository pins the CI formatter to version
22.1.3 so formatting results remain reproducible. Static analysis is enabled
through the `lint` preset:

```sh
cmake --preset lint
cmake --build build/lint --target format-check
cmake --build --preset lint
```

On Linux, install `gcovr` before using the coverage preset. Run the test
partitions first, then generate HTML and XML reports under
`build/coverage/coverage`:

```sh
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
cmake --build --preset coverage-report
```

Coverage is reported in CI but is intentionally not a merge threshold yet.
This establishes a measurable baseline without rewarding superficial tests.

### Performance benchmarks

The standalone benchmark executable measures uniform-grid and brute-force
physics steps plus full tile-map construction. Results are diagnostic rather
than pass/fail gates:

```sh
cmake --preset benchmarks
cmake --build --preset benchmarks
./build/benchmarks/benchmarks/Lorenzo2DBenchmarks
```

Multi-config generators may place the executable in a configuration-specific
subdirectory.

### Use as a CMake dependency

When Lorenzo2D is added with `add_subdirectory` or FetchContent, its sandbox,
regression tests, and install rules default to `OFF`. The parent project retains
control of those targets and of any pre-existing SFML cache choices.

```cmake
add_subdirectory(external/Lorenzo2D)

target_link_libraries(MyGame PRIVATE Lorenzo2D::Lorenzo2D)
```

A parent may explicitly enable any Lorenzo2D option before adding the
subdirectory. The public target propagates the C++17 requirement, public include
path, and `SFML::Graphics` dependency. If the parent already provides an
`SFML::Graphics` target, Lorenzo2D reuses it without running its own SFML
discovery or FetchContent step.

### Install and consume the package

A top-level build generates conventional install and package-export rules:

```sh
cmake -S . -B build-package \
  -DCMAKE_BUILD_TYPE=Release \
  -DL2D_BUILD_SANDBOX=OFF \
  -DL2D_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --build build-package --parallel
cmake --install build-package
```

The installed package exports `Lorenzo2D::Lorenzo2D` and locates its required
SFML 3.1 Graphics package through `find_dependency`. A consumer can then use:

```cmake
find_package(Lorenzo2D 0.1 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE Lorenzo2D::Lorenzo2D)
```

The selected installation prefix must also make an SFML 3.1 CMake package
available, either in the same prefix or through the consumer's
`CMAKE_PREFIX_PATH`.

## Sandbox controls

| Input | Action |
| --- | --- |
| `Q` / Left arrow | Move left |
| `D` / Right arrow | Move right |
| Space / Up arrow | Jump |
| `F1` | Toggle physics debug outlines |
| `Escape` | Quit the sandbox |
| Mouse wheel | Zoom the camera |
| Left mouse button | Inspect the object under the cursor |

Missing player or coin textures are handled by shape-renderer fallbacks. The
debug overlay also remains disabled if no supported system font is available.

## Level format

The sandbox loads `assets/levels/level1.txt`. Each character represents one
tile-sized cell:

| Character | Meaning |
| --- | --- |
| `#` | Solid tile with a box collider |
| `P` | Player spawn marker |
| `C` | Coin marker |
| `E` | Enemy marker |
| `.` or space | Empty cell |

Rows may have different lengths, and blank rows are preserved. The map's world
width is based on its longest row.

### Tilemap scalability

Solid tiles are rendered in non-empty chunks instead of one object and draw
call per cell. Each solid tile contributes six triangle vertices to its
chunk's batch. At render time, chunks outside a conservative bound for the
active `sf::View` are culled, and each candidate-visible chunk is submitted
with one draw call. Rotated views can retain extra chunks, but never discard a
potentially visible one. This keeps draw submission close to the visible,
occupied parts of a large map; the culling pass still evaluates every
non-empty chunk.

Render chunks default to `16 x 16` tiles. Call
`TileMap::setRenderChunkSize()` before loading to configure the next load. Like
the tile-size configuration, this is a next-load setting: an already loaded
map keeps its snapshot, available through `loadedRenderChunkSize()`, until the
next successful load. Zero chunk dimensions are sanitized to one.
Positive tile dimensions below the collider precision floor (`0.0001`) are
clamped so rendered cells and their collision geometry remain aligned.
Loads whose tile dimensions and layout would overflow finite world coordinates
or collapse cell/collider boundaries are rejected before the current map or
its generated objects are changed.

Collision geometry is independent of render chunks. A deterministic greedy
pass merges adjacent solid cells into larger collider-only rectangles across
the whole layout, including across chunk boundaries. Collider-only objects are
static in the physics world, so dense floors and walls require far fewer
physics proxies than one collider per tile.

`TileMap::buildStats()` reports solid tiles, non-empty render chunks, and merged
collision rectangles from the current successful load. For rendering,
`renderStatsForView()` conservatively predicts visible/culled chunks,
submitted tiles and vertices, and draw calls without opening a window;
`lastRenderStats()` reports the same counters from the most recent real render.

The generated game-object topology, object names, and physics contact object
IDs are implementation details. They may change after a reload or as batching
and merging evolve; gameplay code should not use them as persistent tile
identity.

## Asset lifetime

`FontHandle` and `TextureHandle` are shared leases that provide read-only access
to SFML assets. Copying a handle shares ownership of the same published asset
generation; a default or missing handle is empty and can be checked before
dereferencing. The asset remains alive until the registry and every copied
handle have released it.

`AssetManager` provides matching `load`, `store`, `get`, `unload`, `count`, and
`clear` operations for named fonts and textures. `loadFont()` and
`loadTexture()` publish a new asset only after the complete file load succeeds.
`storeFont()` and `storeTexture()` register an existing non-empty handle, while
`getFont()` and `getTexture()` return an empty handle when the name is absent.
The `has` and `count` queries describe only the manager's current registry.

Loading or storing under an existing name replaces that registry entry with a
new published generation. Handles acquired earlier continue to own the previous
generation, and later `get` calls acquire the replacement. A failed file load
or an attempt to store an empty handle leaves the current entry unchanged.
Similarly, unloading a name, clearing a registry, or destroying the manager
releases only registry ownership; outstanding handles remain valid.

`SpriteRenderer` retains its `TextureHandle`, and `DebugOverlay` retains its
`FontHandle`, for as long as SFML borrows the corresponding resource. Rebinding
either renderer rejects an empty handle without disturbing its current asset.
This prevents manager operations from leaving renderer-owned SFML drawables
with dangling texture or font pointers.

Automatic hot reload and file watching are deliberately deferred. Replacing a
named asset does not silently update renderers that hold an older snapshot;
acquire the new handle and explicitly rebind each renderer that should use it.

## Source layout

```text
include/Lorenzo2D/  Public engine headers
src/Lorenzo2D/      Engine implementations
sandbox/            Integration demo and sample game
assets/             Text levels and optional runtime assets
tests/              Regression and consumer integration tests
benchmarks/         Standalone physics and tile-map performance probes
cmake/              Installed-package configuration templates
```

The public engine is separated into `Core`, `ECS`, `Scene`, `Renderer`,
`Physics`, `Assets`, and `Tilemap` modules. Scenes own game objects, and game
objects own their components. Destruction is queued so an object can safely
request its own removal during an update.

Identity-bearing `Component`, `GameObject`, `Scene`, and `SceneManager` objects
cannot be copied or moved. Create object handles through `Scene::createHandle`,
and mutate scene ownership only through `Scene` methods; `gameObjects()` exposes
the collection for structurally read-only iteration. Scenes retain that stable
creation order while maintaining a synchronized ID index, so ID lookup, handle
resolution, and ownership checks are average constant time. Destroy-queued
objects become unresolvable immediately and leave the index when they are
physically removed.

`SceneManager::clear()` is safe during an owned scene's update or render
dispatch. It clears the active-scene selection immediately and releases scene
ownership after the outermost dispatch returns. Scene names are unique within a
manager; creating a duplicate throws `std::invalid_argument`. Manager-level
fixed updates are non-reentrant, including when a callback switches the active
scene before attempting a recursive update.

`PhysicsWorld2D` is also non-copyable and non-movable because each instance owns
configuration, contact history, and contact-event state for its simulation.
Solver iteration counts use their defaults when set to zero and are capped at 64
to keep malformed or untrusted configuration from stalling a simulation step.

## Simulation timing

`Application` separates wall-clock frames from deterministic simulation ticks.
The default fixed delta is `1 / 60` second. A rendered frame may run zero, one,
or several fixed ticks, but every simulation tick receives exactly the same
delta.

The callback order for each frame is:

| Callback | Frequency | Intended work |
| --- | --- | --- |
| `onFrameStart` | Once, before fixed ticks | Sample and buffer frame-scoped input |
| `onFixedPreSimulation` | Zero or more times | Controllers and scene component updates |
| `onFixedSimulation` | Zero or more times | Physics integration and collision solving |
| `onFixedPostSimulation` | Zero or more times | Contact-dependent gameplay and cleanup |
| `onUpdate` | Once, after fixed ticks | Camera, UI, and other presentation state |
| `onRender` | Once | Draw using the supplied interpolation alpha |

Call `Scene::fixedUpdate` from `onFixedPreSimulation`; the retained
`Scene::update` name is a compatibility alias for the same fixed-update path.
Rigid bodies no longer integrate during component updates, so each fixed tick
must call `PhysicsWorld2D::step` from `onFixedSimulation`. Objects created or
activated while a scene is running its fixed callbacks join component and
physics processing on the following fixed tick, keeping both phases on the same
participant set. A fixed update is non-reentrant for a given scene; a recursive
call made during that scene's active fixed tick is ignored.

Keyboard and mouse pressed/released edges are frame-scoped. The value remains
visible to every fixed tick in that frame, so one-shot gameplay actions must be
buffered in `onFrameStart` and consumed only once by fixed simulation. The
sandbox uses this pattern for jumping.

The timing values deliberately describe different clocks and accounting
categories:

| Value | Meaning |
| --- | --- |
| `Time::rawDeltaTime()` | Sanitized wall-clock duration presented to the current frame |
| `Time::frameDeltaTime()` / `deltaTime()` | Current raw delta after the `maximumFrameDeltaTime` clamp |
| `Time::fixedDeltaTime()` | Constant delta supplied to every completed fixed tick |
| `Time::realElapsedTime()` | Cumulative sanitized raw wall-clock time |
| `Time::elapsedTime()` | Cumulative clamped frame time admitted to the fixed-step accumulator |
| `Time::simulationTime()` | Cumulative time represented by completed fixed ticks |
| `Time::clampedFrameTime()` | Cumulative wall-clock time rejected by the initial frame clamp |
| `Time::droppedTickCount()` | Cumulative whole fixed ticks discarded after the per-frame catch-up cap |
| `Time::droppedSimulationTime()` | Cumulative simulation duration represented by those discarded whole ticks |
| `FixedStepScheduler::accumulator()` | Remaining admitted fractional simulation time retained for interpolation and the next frame |

`frameCount`, `tickCount`, and `ticksThisFrame` expose the corresponding frame
and completed-tick counters. FPS is calculated from raw wall-clock time rather
than the clamped simulation budget.

To prevent a long stall from causing a spiral of death, raw frame time is first
clamped and only `maximumTicksPerFrame` ticks may execute. `clampedFrameTime`
accounts for time rejected by that initial wall-clock clamp. Whole fixed ticks
still left after the execution cap are counted separately by `droppedTickCount`
and `droppedSimulationTime`. The fractional accumulator remainder is never
reported as a dropped tick and is retained for interpolation and the next
frame.

Within floating-point tolerance, the cumulative accounting invariants are:

```text
real elapsed time = elapsed clamped time + frame-clamped wall-clock time
elapsed clamped time = completed simulation time
                     + dropped simulation time
                     + accumulator remainder
```

For an individual scheduler frame, `rawDeltaTime` equals `frameDeltaTime` plus
that frame's `clampedFrameTime`. These categories are intentionally distinct:
frame-clamped wall-clock time never becomes fixed-step work, while dropped
simulation time was admitted to the accumulator but discarded as whole ticks
because the catch-up limit had already been reached.

Rendering samples between each transform's previous and current fixed state.
An alpha of zero selects the previous state and an alpha of one selects the
current state, creating the usual one-fixed-tick presentation latency in return
for smooth motion. Call `Transform::resetInterpolation()` after teleports,
respawns, or other discontinuous movement to prevent a visible sweep from the
old position.

## Camera and renderer numeric contract

`Camera2D` keeps its base and effective view extents finite and strictly
positive, and keeps zoom within its supported positive range. Invalid camera
coordinates, movement offsets, bounds, follow targets, and time deltas are
rejected transactionally, so they cannot replace the last valid state. Finite
reversed bounds are normalized. Camera-controller zoom limits and step factors
are likewise sanitized before they can reach the camera.

Follow smoothing and bounds calculations use widened intermediate arithmetic.
This keeps small follow steps stable and prevents otherwise valid extreme
coordinates from overflowing during interpolation, midpoint, or extent
calculations. Transform interpolation uses the same widened approach while
retaining shortest-angle rotation. Nonfinite transform mutations are rejected;
finite zero and negative scale remain supported for hiding and mirroring.

Circle radii and rectangle or sprite dimensions are stored as finite,
nonnegative values. Invalid dimensions collapse only the affected dimension to
zero rather than passing NaN or infinity into SFML. Before drawing, renderers
validate the complete transformed local bounds, including translation,
normalized rotation, scale, and the active sprite texture rectangle. Physics
debug outlines use the same conservative coordinate domain. If a derived draw
state leaves that domain, the draw is skipped without poisoning the drawable's
retained state.

## Physics model

Physics is advanced only by `PhysicsWorld2D::step`. The default world gravity
is `(0, 980)`, matching the engine's positive-down Y axis, and a rigid body opts
into gravity with `setUseGravity(true)`. A force accumulator persists until the
next valid physics step in which its active body participates, and is cleared
after that integration. A non-positive or non-finite delta time is a true no-op,
including contact and force state.

`RigidBody2D` defaults to `BodyType2D::Dynamic`. Dynamic bodies respond to
forces, gravity, impulses, and collisions. Kinematic bodies move with their
prescribed velocity and can push dynamic bodies, but have infinite effective
mass. Static bodies neither move nor accept velocity. A collider without a
rigid body is also treated as static, preserving the convenient level-geometry
workflow used by the tilemap. `isGrounded()` is defined only for Dynamic bodies;
Static and Kinematic bodies report false.

The narrow phase supports circle-circle, circle-box, and axis-aligned box-box
pairs. Exact tangency counts as contact. Collision response uses iterative
normal and friction impulses plus positional correction; rotation, angular
velocity, and transform scale do not participate. Solver behavior can be tuned
through `PhysicsWorld2DConfig`, including iteration counts, penetration slop,
correction strength, restitution threshold, grounded-normal threshold, and the
broad-phase settings.

The default broad phase places conservative collider bounds in a signed
uniform grid with a cell size of `128`. It supports negative world coordinates,
keeps maximum bounds inclusive so exact tangency is retained, removes duplicate
pairs produced by multi-cell shapes, and sorts candidates before the narrow
phase. `broadPhaseCellSize` tunes spatial resolution. A proxy that would occupy
more than `broadPhaseMaxCellsPerProxy` cells, or whose bounds cannot be mapped
safely, is tested through a bounded all-peer fallback instead of expanding an
unbounded grid range. `PhysicsBroadPhaseMode2D::BruteForce` remains available
for validation and unusually small worlds.

`broadPhaseStats()` describes the most recent valid step. It reports proxy and
occupied-cell counts, fallback proxies, the eligible all-pairs baseline, unique
broad-phase candidates, and narrow-phase tests remaining after collision
filtering. These counters make spatial tuning measurable without
timing-dependent tests. An invalid-delta no-op preserves the previous counters;
`reset()` and `reset(Scene&)` clear them.

The current model supports one collider per game object; when several are
attached, only the first `Collider2D` component participates in world
simulation. Exact, symmetric degenerate raw manifold queries, such as coincident
shapes, use fixed tie axes for deterministic output; swapping the query
arguments therefore need not reverse the normal in those otherwise
directionless cases.

Each collider has a `PhysicsMaterial2D`. Restitution and friction coefficients
are sanitized to the range `[0, 1]`, with dynamic friction kept at or below
static friction. A pair uses the greater restitution and the geometric mean of
each friction coefficient. The zero-valued defaults preserve the earlier
inelastic, frictionless behavior, including leaving tangential and separating
velocity unchanged.

`CollisionFilter2D` supplies category and mask bit fields. Two colliders are
considered only when each collider's mask accepts the other's category.
Sensors use the same filtering and manifold generation as solid colliders but
never apply impulses or positional correction. In the sandbox, coins are
sensors and are collected from contact events rather than overlap polling. To
avoid static tile-pair spam, at least one object in a reported pair must have an
active Dynamic or Kinematic body. The shape-specific `overlaps` helpers remain
raw geometry queries and deliberately ignore activity, filters, and sensors.

After each valid step, `contacts()` exposes the current deterministic contact
list and `contactEvents()` reports `Begin`, `Stay`, and `End` transitions.
Contacts contain stable object IDs, collider types, the manifold, and a sensor
flag; `isTouching` provides a convenient ID-pair query. Consume these results in
`onFixedPostSimulation`, before the following physics step replaces them. The
world resets its contact history when switching to a different scene.
`reset()` explicitly clears that history, contact events, and the associated
scene, but cannot alter component flags because it has no scene to traverse.
`reset(Scene&)` additionally clears `isColliding` on each object's first
participating collider and clears rigid-body grounded flags in that scene.

For existing code, the main migration points are that a newly added rigid body
is dynamic by default, collider-only objects remain static, and integration is
owned by the world's fixed simulation step rather than component update.

## Current limitations and next milestones

- Physics shapes are axis-aligned and ignore transform rotation and scale.
  Dense single-cell scenes and fallback proxies can still approach quadratic
  pair counts; continuous collision detection, sleeping, joints, and angular
  dynamics are future work.
- Tilemap chunks rebuild as a whole when a layout changes; incremental chunk
  editing, streamed regions, textured tilesets, and animated tiles remain
  future work.
- Render ordering is a fixed layer mask rather than a general render queue.
- Asset handles provide lifetime-safe, read-only access to published
  generations, but automatic file watching, hot reload propagation, dependency
  tracking, and background loading remain future work.
- The sandbox is still a single integration example. Smaller examples and more
  subsystem tests should be added as APIs stabilize.
- Regression sources use a lightweight first-party harness with shared support
  and focused subsystem executables. More data-driven cases and richer failure
  context can be added as the suite grows.
- A project license is not yet provided and requires an explicit owner choice.

These constraints are kept explicit so future changes can improve one contract
at a time without hiding unsupported behavior.

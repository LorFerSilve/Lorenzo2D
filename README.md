# Lorenzo2D

Lorenzo2D is an experimental 2D game engine written in C++17 on top of
[SFML 3.1](https://www.sfml-dev.org/). The repository contains a static engine
library and a sandbox platformer that exercises the engine's current systems.

The project is intentionally small and readable. It is suitable for learning
engine architecture and iterating on core 2D systems, but it is not yet a
production-ready engine.

The public engine version is generated from CMake's project version and is
available through `<Lorenzo2D/Core/Version.hpp>`. Project-wide contracts and
current support claims are documented separately:

- [`docs/architecture.md`](docs/architecture.md) defines ownership, update
  order, dependency direction, numeric safety, and determinism boundaries.
- [`docs/versioning.md`](docs/versioning.md) defines the pre-1.0 compatibility
  and deprecation policy.
- [`docs/support-matrix.md`](docs/support-matrix.md) distinguishes planned,
  experimental, supported, and production-tested development paths.
- [`docs/roadmap.md`](docs/roadmap.md) records the dependency-ordered phases for
  point-and-click, top-down, platformer, and isometric support.
- [`docs/benchmarking.md`](docs/benchmarking.md) defines diagnostic scenarios,
  machine-readable reports, and the policy for future performance budgets.
- [`docs/input.md`](docs/input.md) documents typed actions, context blocking,
  gamepad handling, fixed-tick edge consumption, and the unified pointer model.
- [`docs/physics-queries.md`](docs/physics-queries.md) documents deterministic
  world queries, shape casts, filtering, capsules, and convex slope polygons.

## Current features

- SFML application loop with fixed simulation ticks and bounded catch-up
- Previous/current transform interpolation for smooth presentation
- Frame timing, keyboard, mouse, gamepad, touch, and window events
- Typed button/1D/2D actions, multiple bindings, analog deadzones, normalized
  diagonals, fixed-tick edge consumption, and blocking input contexts
- Unified mouse/touch pointers with camera-aware world conversion and drag state
- Game objects, transforms, polymorphic components, tags, stable z-order, and deferred deletion
- Scenes, scene switching, object queries, and lifetime-aware object handles
- Circle and rectangle rendering plus texture-backed sprites and sprite-sheet animation
- Smooth bounded 2D camera, resize handling, follow behavior, and wheel zoom
- Editable ASCII tilemaps with streamed chunks, animated atlas tiles, view culling, and merged collision geometry
- Static, kinematic, and dynamic rigid bodies with linear and angular dynamics
- Compound circle, oriented-box, capsule, and convex-polygon colliders with scale-aware transforms
- CCD, sleeping, persistent warm-started contacts, and distance joints
- All-pair collider manifolds, impulse response, friction, and restitution
- Ray, point, overlap, and swept circle/box queries with reusable fixed-tick snapshots
- Collision layers, sensors, contact events, and physics debug drawing
- Snapshot and live font/texture handles, background loading, hot reload,
  dependency tracking, and ordered runtime resource lookup
- Deterministic particles, screen-space color passes, data-only prefabs, and a versioned level format
- Debug overlay and independently switchable world, physics, and UI layers
- Focused minimal, animation, physics, and phase-4 examples plus regression tests for timing,
  scenes, rendering, resources, serialization, animation, physics, and tilemaps

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

The sandbox and the focused `Lorenzo2DMinimalExample`,
`Lorenzo2DAnimationExample`, `Lorenzo2DPhysicsExample`, and
`Lorenzo2DPhase4Example` executables are
written to `build/bin`. Disable them independently with
`-DL2D_BUILD_SANDBOX=OFF` and `-DL2D_BUILD_EXAMPLES=OFF`.

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
| `L2D_BUILD_EXAMPLES` | `ON` | `OFF` | Build the focused minimal, animation, and physics examples |
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
physics steps, scene updates, render-queue construction, full tile-map
construction, and tile-map view culling. Results are diagnostic rather than
pass/fail gates:

```sh
cmake --preset benchmarks
cmake --build --preset benchmarks
./build/benchmarks/benchmarks/Lorenzo2DBenchmarks
```

Pass `--json <path>` and/or `--csv <path>` to retain machine-readable reports
while keeping the human-readable table on standard output. See
[`docs/benchmarking.md`](docs/benchmarking.md) for the report contract and the
rules for introducing blocking performance budgets.

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
find_package(Lorenzo2D 0.6 CONFIG REQUIRED)
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
| Gamepad left stick | Move horizontally |
| Gamepad button 0 | Jump |
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

### Textured tilesets

`TileSet` maps any layout character to a positive pixel rectangle in a texture
atlas. `setTileFromGrid()` calculates that rectangle from an atlas cell and a
pixel tile size. Assign the tileset before loading so `TileMap` snapshots its
mappings and texture lease with the generated render batches:

```cpp
l2d::TileSet tiles;
tiles.setTexture(assets.getTexture("terrain"));
tiles.setTileFromGrid('#', {0u, 0u}, {16u, 16u});
tiles.setTileFromGrid('G', {1u, 0u}, {16u, 16u});

tileMap.setTileSize({32.f, 32.f});
tileMap.setTileSet(tiles);
tileMap.loadFromLayout(scene, {"#GG#"});
```

Mapped characters render even when they are not the solid collision character.
An unmapped solid character retains the flat `solidTileColor()` fallback.
Colored and atlas-mapped geometry can therefore share a chunk; each non-empty
geometry kind contributes one draw call for that visible chunk.

`TileMapBuildStats` separates collision solids, all rendered tiles, and
atlas-mapped tiles. The configured `tileSet()` and active `loadedTileSet()` are
separate next-load/current-load snapshots, matching tile and chunk sizing.

## Sprite-sheet animation

`AnimationClip` owns validated texture rectangles and per-frame durations.
Frames can be appended individually or as a horizontal/vertical atlas grid.
`Animator` is an ECS component that drives the `SpriteRenderer` on the same
game object during fixed updates:

```cpp
auto& sprite = object.addComponent<l2d::SpriteRenderer>(texture);
sprite.setTextureRect({{0, 0}, {32, 32}});
sprite.setSize({96.f, 96.f});

l2d::AnimationClip run("run");
run.addGridFrames({0, 0}, {32, 32}, 6u, 0.08f);

auto& animator = object.addComponent<l2d::Animator>();
animator.addClip(std::move(run));
animator.play("run");
```

Clips support looping and one-shot playback. Animators expose pause, stop,
restart, speed, current-frame, and completion state. Invalid time deltas are
no-ops, and very large looping deltas are reduced by the clip duration before
frame traversal. Use equal-sized frames when a fixed displayed sprite size is
desired; `SpriteRenderer::setSize()` scales against its active texture rect.

## Prefabs and serialized levels

`Prefab` is a data-only object template for transform, tag, active state,
shape renderers, rigid body, one collider of each supported shape type, and z-order. `PrefabLibrary`
stores validated named snapshots and instantiates them into any `Scene`.
Custom gameplay components remain game-owned and can be attached immediately
after instantiation.

`LevelDocument` contains an ordered list of those prefabs. `LevelSerializer`
round-trips it through streams or `.l2dlevel` files and can instantiate the
complete document while returning lifetime-aware object handles. The format
is currently written as `LORENZO2D_LEVEL 3`; versions 1 and 2 remain readable while
unsupported versions, non-finite values,
invalid component data, excessive object counts, malformed records, and
trailing input are rejected without changing the destination document.
The complete field reference and a checked-in example live in
[`docs/level-format.md`](docs/level-format.md) and
[`assets/levels/phase2-showcase.l2dlevel`](assets/levels/phase2-showcase.l2dlevel).

```cpp
l2d::LevelDocument level;
if (l2d::LevelSerializer::loadFromFile("level.l2dlevel", level))
{
    const auto objects = l2d::LevelSerializer::instantiate(scene, level);
}
```

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

### Editing, streaming, and animated tiles

`TileMap::setTile()` updates an existing cell by rebuilding only its render
chunk. A solid/non-solid transition also republishes exact merged collision
rectangles transactionally. `lastUpdateStats()` distinguishes the render and
collision work. `setStreamRegion()` limits resident chunks in tile coordinates
before view culling; render telemetry reports resident and non-resident counts.

`TileSet::setAnimatedTile()` accepts atlas rectangles with positive frame
durations. Animation updates only texture coordinates, leaving chunk positions
and physics geometry stable. Configuration remains snapshot-based per load.

## Render queue and effects

Every game object has an explicit signed z-order. The per-pass `RenderQueue2D`
sorts lower values first and preserves scene insertion order for ties. Prefabs
and level-format versions 2 and 3 persist z-order; version 1 files remain readable and
default it to zero.

`ParticleEmitter2D` provides seeded, bounded bursts and continuous emission
with lifetime, velocity, gravity, color, and size evolution.
`PostProcessStack2D` applies ordered alpha/add/multiply screen-space color
passes after scene rendering. See
[`docs/rendering-and-assets.md`](docs/rendering-and-assets.md) for the complete
contracts and integration order.

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

`liveTexture()` and `liveFont()` provide opt-in named bindings without changing
snapshot-handle behavior. `SpriteRenderer::setLiveTexture()` and
`DebugOverlay::setLiveFont()` follow successful new generations while retaining
their last valid resource if a slot is empty.
`AssetPipeline` decodes requested textures in the background, publishes them
from `poll()` on the graphics-context thread, watches timestamps, and reports
transitive dependents through an acyclic dependency graph.

### Runtime resource lookup

`ResourceLocator` replaces source-tree compile definitions with ordered runtime
roots. Add a working-directory asset root and an executable-relative root,
then pass portable resource names to its `locate()` query or directly to the
`AssetManager` overloads:

```cpp
l2d::ResourceLocator resources;
resources.addRoot(std::filesystem::current_path() / "assets");
resources.addRoot(l2d::ResourceLocator::executableDirectory(argv[0]) / "assets");

assets.loadTexture("hero", resources, "textures/hero.png");
```

Roots are normalized to absolute paths, deduplicated, and queried in insertion
order. Absolute existing resource paths remain valid. The sandbox copies its
asset directory beside the executable after each build and uses only this
runtime lookup path; it no longer embeds the source asset directory at compile
time.

## Source layout

```text
include/Lorenzo2D/  Public engine headers
src/Lorenzo2D/      Engine implementations
sandbox/            Integration demo and sample game
examples/           Focused minimal, animation, physics, and phase-4 applications
assets/             Text levels and optional runtime assets
tests/              Regression and consumer integration tests
benchmarks/         Standalone physics and tile-map performance probes
cmake/              Installed-package configuration templates
```

The public engine is separated into `Core`, `ECS`, `Scene`, `Renderer`,
`Animation`, `Physics`, `Assets`, and `Tilemap` modules. Scenes own game objects, and game
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

The narrow phase supports every pair of circles, oriented boxes, capsules, and
convex polygons. Exact tangency counts as contact. Collider offsets are local center
points and inherit owner translation, rotation, and scale. Boxes and polygons use per-axis
scale; circles and capsule caps use a conservative uniform radius under non-uniform scale.
Collision response uses iterative linear
and angular normal/friction impulses plus positional correction. Rigid bodies
keep rotation fixed by default for source compatibility; call
`setFixedRotation(false)` to enable angular velocity, torque, inertia, and
off-center impulses.

Every collider receives a stable `ColliderId`, and all active colliders on a
game object participate as one compound body. Self-collision between colliders
on that body is skipped. Contacts expose both collider IDs, and
`isColliderTouching` queries a specific shape pair. Exact, symmetric degenerate
raw manifold queries use deterministic tie axes; swapping otherwise
directionless arguments therefore need not reverse the normal.

Solver behavior can be tuned through `PhysicsWorld2DConfig`. Adaptive bounded
CCD substeps fast bodies before narrow-phase solving, persistent contact
impulses warm-start the following step, and sufficiently still dynamic bodies
sleep until a mutation or moving constraint wakes them. `DistanceJoint2D`
constrains two local anchors and, by default, suppresses collision between the
connected objects. `stepStats()` reports CCD substeps, active/sleeping bodies,
contact and joint constraints, and warm-start reuse.

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
Public ray, point, overlap, and shape-cast queries instead use
`PhysicsQueryFilter2D` and may include sensors explicitly.

After each valid step, `contacts()` exposes the current deterministic contact
list and `contactEvents()` reports `Begin`, `Stay`, and `End` transitions.
Contacts contain stable object and collider IDs, collider types, the manifold,
and a sensor flag; `isTouching` and `isColliderTouching` provide object- and
shape-pair queries. Consume these results in `onFixedPostSimulation`, before
the following physics step replaces them. The world resets its contact history
when switching to a different scene.
`reset()` explicitly clears that history, contact events, and the associated
scene, but cannot alter component flags because it has no scene to traverse.
`reset(Scene&)` additionally clears `isColliding` on every collider and clears
rigid-body grounded flags in that scene.

For existing top-left-origin objects, explicitly set a collider center offset:
`size * 0.5f` for boxes and `{radius, radius}` for circles. Circle constructors
and `setRadius` no longer change the offset. Collider-only objects remain
static, and integration remains owned by the world's fixed simulation step.

## Current limitations and next milestones

- Physics supports circles, oriented boxes, capsules, convex polygons, and distance joints;
  concave/edge shapes, compound mass-property calculation, multi-point manifolds, and other joint
  types remain future work. CCD uses bounded adaptive substeps, so
  translations beyond the configured cap can still tunnel. Dense single-cell
  scenes and fallback proxies can still approach quadratic pair counts.
- Tile editing is bounded to existing rows and columns. Collision geometry is
  rebuilt globally only when cell solidity changes; render geometry remains a
  one-chunk update. Streaming controls render-submission residency, not disk-backed
  map paging or chunk storage.
- Post-processing currently provides ordered screen-space color passes rather
  than off-screen shader graphs.
- Prefab serialization currently covers built-in shape renderers and physics
  components. Sprite asset references, animation state, custom component
  codecs, and schema migrations remain future work.
- Regression sources use a lightweight first-party harness with shared support
  and focused subsystem executables. More data-driven cases and richer failure
  context can be added as the suite grows.
- A project license is not yet provided and requires an explicit owner choice.

These constraints are kept explicit so future changes can improve one contract
at a time without hiding unsupported behavior.

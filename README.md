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
- ASCII tilemap loading with generated render and collision objects
- Static, kinematic, and dynamic rigid bodies with configurable gravity
- Circle/box collision manifolds, impulse response, friction, and restitution
- Collision layers, sensors, contact events, and physics debug drawing
- Named font and texture storage
- Debug overlay and independently switchable world, physics, and UI layers
- Headless regression tests for timing, transforms, scenes, physics, and tilemaps

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
sudo apt-get install xorg-dev libharfbuzz-dev libfreetype-dev \
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

The sandbox executable is written to `build/bin`. Disable it for a test- or
library-only build with `-DL2D_BUILD_SANDBOX=OFF`.

Useful configuration options:

| Option | Default | Purpose |
| --- | --- | --- |
| `L2D_BUILD_SANDBOX` | `ON` | Build the interactive sandbox |
| `L2D_BUILD_TESTS` | `ON` | Build and register regression tests |
| `L2D_USE_SYSTEM_SFML` | `OFF` | Use an installed SFML package |
| `L2D_WARNINGS_AS_ERRORS` | `OFF` | Promote engine, sandbox, and test warnings to errors |

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

## Source layout

```text
include/Lorenzo2D/  Public engine headers
src/Lorenzo2D/      Engine implementations
sandbox/            Integration demo and sample game
assets/             Text levels and optional runtime assets
tests/              Headless regression tests
```

The public engine is separated into `Core`, `ECS`, `Scene`, `Renderer`,
`Physics`, `Assets`, and `Tilemap` modules. Scenes own game objects, and game
objects own their components. Destruction is queued so an object can safely
request its own removal during an update.

Identity-bearing `Component`, `GameObject`, `Scene`, and `SceneManager` objects
cannot be copied or moved. Create object handles through `Scene::createHandle`,
and mutate scene ownership only through `Scene` methods; `gameObjects()` exposes
the collection for structurally read-only iteration.

`PhysicsWorld2D` is also non-copyable and non-movable because each instance owns
configuration, contact history, and contact-event state for its simulation.

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
must call `PhysicsWorld2D::step` from `onFixedSimulation`. Objects created while
a scene is running its fixed callbacks join component and physics processing on
the following fixed tick, keeping both phases on the same participant set.

Keyboard and mouse pressed/released edges are frame-scoped. The value remains
visible to every fixed tick in that frame, so one-shot gameplay actions must be
buffered in `onFrameStart` and consumed only once by fixed simulation. The
sandbox uses this pattern for jumping.

The timing values deliberately describe different clocks:

| Value | Meaning |
| --- | --- |
| `Time::rawDeltaTime()` | Sanitized wall-clock duration of the frame |
| `Time::frameDeltaTime()` / `deltaTime()` | Raw delta clamped by `maximumFrameDeltaTime` |
| `Time::fixedDeltaTime()` | Constant delta supplied to every fixed tick |
| `Time::realElapsedTime()` | Accumulated raw wall-clock time |
| `Time::elapsedTime()` | Accumulated clamped frame time |
| `Time::simulationTime()` | Time represented by completed fixed ticks |

`frameCount`, `tickCount`, `ticksThisFrame`, `droppedTickCount`, and
`droppedSimulationTime` expose the corresponding counters. FPS is calculated
from raw wall-clock time rather than the clamped simulation budget.

To prevent a long stall from causing a spiral of death, raw frame time is first
clamped and only `maximumTicksPerFrame` ticks may execute. Whole ticks still
left in the clamped accumulator are dropped and counted; the fractional
remainder is retained for interpolation and the next frame. Time removed by the
initial raw-frame clamp is visible through the difference between raw and frame
delta, but is not counted as dropped simulation ticks.

Rendering samples between each transform's previous and current fixed state.
An alpha of zero selects the previous state and an alpha of one selects the
current state, creating the usual one-fixed-tick presentation latency in return
for smooth motion. Call `Transform::resetInterpolation()` after teleports,
respawns, or other discontinuous movement to prevent a visible sweep from the
old position.

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
correction strength, restitution threshold, and grounded-normal threshold.
Phase 2 supports one collider per game object; when several are attached, only
the first `Collider2D` component participates in world simulation. Exact,
symmetric degenerate raw manifold queries, such as coincident shapes, use fixed
tie axes for deterministic output; swapping the query arguments therefore need
not reverse the normal in those otherwise directionless cases.

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

- Physics shapes are axis-aligned and ignore transform rotation and scale. The
  current pair search is quadratic; a spatial broad phase, continuous collision
  detection, sleeping, joints, and angular dynamics are future work.
- Every solid tile is currently an individual entity, renderer, and collider.
  Chunked rendering, camera culling, and merged static colliders are planned for
  larger maps.
- Render ordering is a fixed layer mask rather than a general render queue.
- Fonts and textures are non-owning from the renderer's perspective; asset
  leases and hot reload need an explicit lifetime model.
- The sandbox is still a single integration example. Smaller examples and more
  subsystem tests should be added as APIs stabilize.

These constraints are kept explicit so future changes can improve one contract
at a time without hiding unsupported behavior.

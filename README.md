# Lorenzo2D

Lorenzo2D is an experimental 2D game engine written in C++17 on top of
[SFML 3.1](https://www.sfml-dev.org/). The repository contains a static engine
library and a sandbox platformer that exercises the engine's current systems.

The project is intentionally small and readable. It is suitable for learning
engine architecture and iterating on core 2D systems, but it is not yet a
production-ready engine.

## Current features

- SFML application loop with frame timing, keyboard, mouse, and window events
- Action bindings with multiple keys per gameplay action
- Game objects, transforms, polymorphic components, tags, and deferred deletion
- Scenes, scene switching, object queries, and lifetime-aware object handles
- Circle and rectangle rendering plus texture-backed sprites
- Smooth bounded 2D camera, resize handling, follow behavior, and wheel zoom
- ASCII tilemap loading with generated render and collision objects
- Basic rigid bodies, gravity, circle/box collision response, and debug drawing
- Named font and texture storage
- Debug overlay and independently switchable world, physics, and UI layers
- Headless regression tests for core scene, physics, and tilemap behavior

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
| `L2D_WARNINGS_AS_ERRORS` | `OFF` | Promote engine warnings to errors |

## Sandbox controls

| Input | Action |
| --- | --- |
| `Q` / Left arrow | Move left |
| `D` / Right arrow | Move right |
| Space / Up arrow | Jump |
| `F1` | Toggle physics debug outlines |
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

## Current limitations and next milestones

- Physics currently resolves dynamic circles against axis-aligned static boxes.
  Transform rotation/scale, circle-circle response, dynamic boxes, a fixed-step
  accumulator, continuous collision detection, and a spatial broad phase are
  future work.
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

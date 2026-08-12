# Architecture contracts

This document records the boundaries that future genre support must preserve. It describes the
current runtime first and the allowed dependency direction for upcoming modules.

## Runtime ownership

- `Application` owns the SFML window, frame clock, and fixed-step scheduler.
- `SceneManager` owns scenes and selects the active scene.
- `Scene` owns game objects and their ID index.
- `GameObject` owns its components and transform.
- `PhysicsWorld2D` owns simulation configuration, contact history, solver caches, and telemetry; it
  does not own scene objects.
- `PhysicsQueryContext2D` owns an immutable collider snapshot and lifetime-aware object handles; it
  does not mutate or borrow the world's contact state.
- Asset handles own immutable published resource generations independently from the registry that
  issued them.
- `TileMapData` owns imported tile definitions, layers, objects, and properties without depending
  on a scene. `TileMap` owns a validated data snapshot and lifetime-aware handles to generated
  render/collision scene objects. `TileMapColliderBuilder2D` remains a pure geometry builder.

Identity-bearing runtime objects remain non-copyable and non-movable unless a future design proves
that moving them preserves every handle and owner relationship.

## Frame and fixed-step order

One rendered frame uses this order:

1. poll window events;
2. sample frame-scoped input;
3. call `onFrameStart` once;
4. run zero or more complete fixed ticks;
5. call `onUpdate` once for presentation state;
6. render using the interpolation alpha;
7. display the completed frame.

One complete fixed tick uses:

1. `onFixedPreSimulation`: controllers, AI, scene component updates, and command consumption;
2. `onFixedSimulation`: physics integration and collision solving;
3. `onFixedPostSimulation`: contact-driven gameplay, destruction, and cleanup.

A close request can stop later frames but does not leave a partially completed fixed tick. New
genre systems must document which fixed phase owns their mutation.

## Dependency direction

Dependencies point downward in this diagram:

```text
Templates and game code
        |
        +-- Isometric -------+
        |                    |
        +-- Navigation ------+---- Movement
        |                    |        |
        +-- Top-down --------+        +---- Physics query contracts
        |                    |
        +-- Platformer ------+
                             |
                 Tilemap ----+---- Renderer
                       \          /
                        Scene/ECS
                            |
                           Core
```

Rules:

- Core does not depend on scene, rendering, physics, navigation, or genre modules.
- Scene/ECS may depend on Core and SFML value/graphics types already exposed by current contracts.
- Physics does not depend on Navigation, Isometric, or a gameplay controller.
- Movement may use Physics query contracts but does not own the physics world.
- Navigation may use Tilemap data, Movement, and Physics queries.
- Isometric may use Renderer, Tilemap, Navigation, and Scene/ECS.
- Templates combine public modules; engine modules never depend on templates or sandbox code.

Cycles between public modules require an architecture review before merge.

## Logical world and presentation

`Transform` stores logical Cartesian world state. Physics, navigation, occupancy, and gameplay use
that state. Orthogonal and isometric display are presentation projections; they must not encode a
second authoritative position into gameplay components.

This rule keeps pathfinding and collision reusable between orthogonal and isometric games and
prevents render rounding from feeding back into simulation.

## Mutation and lifetime

- Structural scene mutation uses `Scene` methods.
- Destruction requested during dispatch is deferred and handles become unresolvable immediately.
- Components may request destruction but must not retain untracked raw pointers across object or
  scene lifetime boundaries.
- New asynchronous systems publish results on the owning simulation or graphics thread through an
  explicit polling/commit point.
- A failed load or configuration change must not partially replace the last valid state.

## Numeric and workload safety

Public APIs accepting coordinates, time, sizes, counts, iterations, or imported content must:

- reject non-finite values before they reach SFML or the solver;
- use widened intermediates where finite input arithmetic can overflow `float`;
- cap untrusted iteration and allocation budgets;
- document whether invalid values are rejected, clamped, or converted to a no-op;
- preserve the previous valid state when a transactional mutation fails.

## Determinism boundary

The fixed-step scheduler, scene participation, physics contact ordering, and seeded particles have
deterministic contracts for identical initial state and commands on one supported build. This is not
yet a cross-compiler or network lockstep guarantee.

New simulation systems must use stable tie-breaking and avoid wall-clock decisions. Tests can use
`L2D_REQUIRE_DETERMINISTIC_REPLAY` to execute the same replay twice and compare snapshots.

## Performance policy

Readability and correct contracts remain the default. Replace scene scans, `dynamic_cast`, or full
sorts only after a benchmark identifies a relevant bottleneck. Each optimization must retain a
reference or equivalence test so that speed does not weaken deterministic behavior.

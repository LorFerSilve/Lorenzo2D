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
- `CharacterMotor2D` owns one character's collision-aware translation and transient contact/support
  state. Gameplay controllers own intent and pass fixed-tick displacement into the motor.
- `TopDownController2D` owns free-movement velocity and facing; `GridStepController2D` owns logical
  cell/step state. Both consume device-independent intent and delegate collision translation to the
  motor.
- `PlatformerController2D` owns side-view velocity, jump/drop timers, facing, and movement events;
  slopes, step attempts, one-way filtering, and platform translation remain motor/query work.
- `NavigationGrid2D` owns walkability, traversal costs, coordinates, and revision state;
  `AStarPathfinder2D` is stateless. `PathFollower2D` owns a runtime path command and progress state,
  then delegates velocity and collision translation to the top-down controller and motor.
- Asset handles own immutable published resource generations independently from the registry that
  issued them. This includes fonts, textures, animation clips, and short sound buffers.
- `AudioSystem` owns active sound voices and streamed music playback; it borrows audio assets only
  through lifetime-safe handles and never owns scene objects.
- `UiCanvas2D` owns screen-space runtime controls and transient pointer interaction state. It is a
  presentation service and does not become authoritative gameplay state.
- `SaveDocument` owns explicit game-selected persisted values. `SaveGameSerializer` validates and
  commits complete documents transactionally; it does not serialize arbitrary scene ownership.
- `TileMapData` owns imported tile definitions, layers, objects, and properties without depending
  on a scene. `TileMap` owns a validated data snapshot and lifetime-aware handles to generated
  render/collision scene objects. `TileMapColliderBuilder2D` remains a pure geometry builder.

Identity-bearing runtime objects remain non-copyable and non-movable unless a future design proves
that moving them preserves every handle and owner relationship.

## Frame and fixed-step order

One rendered frame uses this order:

1. poll window events;
2. sample frame-scoped input;
3. call `onFrameStart` once (input/UI command capture);
4. run zero or more complete fixed ticks;
5. call `onUpdate` once for presentation state and audio voice cleanup;
6. render world/debug passes using interpolation;
7. render screen-space UI;
8. display the completed frame.

One complete fixed tick uses:

1. `onFixedPreSimulation`: controllers, AI, scene component updates, and command consumption;
2. `onFixedSimulation`: physics integration and collision solving;
3. `onFixedPostSimulation`: contact-driven gameplay, destruction, and cleanup.

Moving platforms must be positioned before constructing a shared `PhysicsQueryContext2D` and
calling character motors in pre-simulation. Query-driven characters may have no rigid body or a
kinematic one; static and dynamic rigid bodies conflict with motor-owned translation.

A close request can stop later frames but does not leave a partially completed fixed tick. New
genre systems must document which fixed phase owns their mutation.

## Dependency direction

Dependencies point downward in this diagram:

```text
Templates and game code
        |
        +-- UI / Audio / Save services
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
- UI may depend on Core pointer state, asset handles, and SFML graphics, but not on Scene or Physics.
- Audio may depend on Assets/ResourceLocator and SFML audio, but not on Scene or simulation systems.
- Save persistence remains independent from Scene/Physics; game code explicitly maps gameplay state
  into and out of a `SaveDocument`.
- Templates combine public modules; engine modules never depend on templates or sandbox code.

Cycles between public modules require an architecture review before merge.

## Logical world and presentation

`Transform` stores logical Cartesian world state. Physics, navigation, occupancy, and gameplay use
that state. Orthogonal and isometric display are presentation projections; they must not encode a
second authoritative position into gameplay components.

This rule keeps pathfinding and collision reusable between orthogonal and isometric games and
prevents render rounding from feeding back into simulation.

## Mutation, lifetime, and concurrency

- Structural scene mutation uses `Scene` methods.
- Destruction requested during dispatch is deferred and handles become unresolvable immediately.
- References/raw pointers returned from Scene and GameObject queries are borrowed; use
  `GameObjectHandle` when identity must cross a deferred/lifetime boundary.
- Input maps/context stacks borrow their attached `InputSnapshot`; temporary snapshots are rejected.
- Components may request destruction but must not retain untracked raw pointers across object or
  scene lifetime boundaries.
- Unless a type explicitly documents otherwise, engine state uses single-owner-thread mutation.
  Container implementation details do not imply a concurrent API.
- New asynchronous systems publish results on the owning simulation or graphics thread through an
  explicit polling/commit point. AssetPipeline follows this rule: image decode may run in workers,
  while publication/watch/dependency state remains caller-thread work.
- A failed load or configuration change must not partially replace the last valid state.
- Level batch instantiation removes objects appended by a failed call without sweeping unrelated
  pre-existing queued destruction.
- Save-file writes stage output before replacing the previous file; failed loads leave the caller's
  destination document unchanged.

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

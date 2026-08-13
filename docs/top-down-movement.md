# Top-down movement

Lorenzo2D 0.10 adds two device-independent fixed-step controllers on top of
`CharacterMotor2D`:

- `TopDownController2D` for free eight-direction movement with analog magnitude, normalized
  diagonals, acceleration, deceleration, persistent facing and collision sliding;
- `GridStepController2D` for Pokémon-style four-direction cell movement with smooth presentation,
  deterministic axis selection, buffered turns and transactional collision checks.

Both controllers consume gameplay intent, not keys. An `InputMap`, AI system, replay, or later
network layer can therefore drive the same API.

## Required components

Attach one supported box, circle, or capsule collider, a top-down configured motor, and a
controller to the same object:

```cpp
player.addComponent<l2d::BoxCollider2D>(sf::Vector2f{24.f, 24.f});
player.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
auto& controller = player.addComponent<l2d::TopDownController2D>();
```

`topDownCharacterMotorConfig2D()` disables ground probing, snap, and moving-platform inheritance.
Those platformer policies do not belong on a flat top-down plane. Custom filters and motor limits
can be changed on the returned configuration before construction.

The character may have no rigid body or a kinematic body. Static and dynamic rigid bodies conflict
with motor-owned translation and cause a move to fail without mutation.

## Fixed-tick order

In `onFixedPreSimulation`, update scene components and moving obstacles first, construct one query
snapshot, then invoke every controller:

```cpp
m_scene.fixedUpdate(fixedDeltaTime);
const l2d::PhysicsQueryContext2D queries(m_scene);
controller.move(queries, actions.axis2D("move"), fixedDeltaTime);
```

Input vectors within `inputDeadzone` become zero. Values above it are radially remapped to the
remaining zero-to-one range; magnitudes above one are clamped. Facing follows the last accepted
nonzero intent while velocity accelerates toward the desired speed and decelerates to rest. Invalid
configuration, non-finite input, non-positive delta time, or a delta above `maximumDeltaTime` is a
transactional failure.

## Grid steps

Grid positions are `gridOrigin + cell * cellSize`. A controller initially accepts only a transform
within `alignmentTolerance` of such a point:

```cpp
l2d::GridStepControllerConfig2D config;
config.cellSize = {32.f, 32.f};
config.gridOrigin = {16.f, 16.f};
config.stepDuration = 0.14f;
auto& grid = player.addComponent<l2d::GridStepController2D>(config);
grid.synchronizeToGrid();
```

Pass a cardinal `GridDirection2D`, or pass a 2D input vector and let
`gridDirectionFromInput()` resolve it. The larger axis wins. Equal axes use the configured stable
horizontal or vertical priority. While a step runs, a different direction can be buffered and
starts on the following fixed tick after completion.

Before a step starts, `CharacterMotor2D::testMove()` checks the complete cell displacement without
changing transform or motor state. A blocked target reports `blocked` and leaves the logical and
world positions unchanged. If a moving obstacle enters during an accepted step, that step reports
`rolledBack` and returns atomically to its start cell. `cancelStep()` provides the same explicit
rollback for game-state changes. `synchronizeToGrid(true)` may be used to snap a deliberately moved
object to the nearest representable cell.

`GridStepController2D` uses linear logical progress. Rendering still receives normal Transform
interpolation, so there is no second presentation position to synchronize.

## Prefabs, examples, and exclusions

`TopDownControllerPrefab` and `GridStepControllerPrefab` round-trip through JSON level version 6.
Prefab validation requires a `CharacterMotorPrefab` and rejects invalid controller fields before
instantiation. JSON versions 4 and 5 remain readable.

`Lorenzo2DPhase6Example` demonstrates WASD free movement and arrow-key grid movement in one map.
`templates/top-down` is a standalone installed-package starting project.

This phase does not include point-and-click navigation, A*, trigger dispatch, dialogue, combat, or
platformer gravity/jumps. Navigation arrives in phase 8; triggers remain ordinary sensor/query data
until a runtime dispatcher lands.

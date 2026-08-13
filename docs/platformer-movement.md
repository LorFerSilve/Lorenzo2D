# Side-view platformer movement

Lorenzo2D 0.11 adds `PlatformerController2D`, an input-independent fixed-step controller built on
`CharacterMotor2D`. It supplies the common side-view policies that do not belong in the collision
motor: run acceleration, air control, gravity, fall-speed limiting, jumping, and gameplay events.

## Minimal setup

Reserve a collider category for one-way platforms and use the matching platformer motor preset:

```cpp
constexpr std::uint32_t OneWay = 1u << 1u;

auto& player = scene.createGameObject("Player");
player.addComponent<l2d::CapsuleCollider2D>(12.f, 40.f);
player.addComponent<l2d::CharacterMotor2D>(
    l2d::platformerCharacterMotorConfig2D(OneWay));
auto& controller = player.addComponent<l2d::PlatformerController2D>();
```

In every fixed tick, move kinematic platforms first, construct one query snapshot, and pass sampled
intent to the controller:

```cpp
l2d::PlatformerInput2D input;
input.horizontal = actions.axis1D("run");
input.jumpPressed = actions.consumePressed("jump");
input.jumpReleased = actions.consumeReleased("jump");
input.dropDown = actions.down("drop");

const auto result = controller.move(l2d::PhysicsQueryContext2D(scene), input, fixedDeltaTime);
if (result.events.landed)
    playLandingEffect();
```

`horizontal` is clamped to `[-1, 1]` after the configurable deadzone. Button fields are commands,
not hard-coded keys, so the same controller works with keyboard, gamepad, AI, replay, or network
input. `jumpPressed` and `jumpReleased` should be edge-triggered exactly once per rendered input
frame when several fixed ticks catch up.

## Movement policy

Ground and air acceleration/deceleration are independent. Gravity accelerates along the motor's
configured down direction and clamps at `maximumFallSpeed`. A jump can use the last
`coyoteTime` seconds after leaving a ledge and a press can wait in `jumpBufferTime` before landing.
Releasing jump while rising multiplies upward speed by `jumpCutMultiplier`, producing a shorter
jump without changing the full-jump launch speed.

The published state exposes world velocity, ground/wall/ceiling flags, one-way support, facing, and
the three policy timers. Per-tick events report jump, landing, leaving ground, new wall/ceiling
contact, successful step-up, and accepted drop-through. Use events for audio/animation and state for
continuous decisions.

## Slopes and steps

Walkable slopes are surfaces whose normal lies inside the motor's `maximumSlopeAngleDegrees`.
Sweep-and-slide follows their tangent and ground snapping keeps the character attached across small
downward changes. A steeper surface is classified as a wall.

After a grounded horizontal wall contact, the controller may call the motor's transactional step
sequence for the unconsumed horizontal distance. `stepHeight` is the maximum rise;
`stepDownDistance` is extra settling distance. If rise, traversal, or landing fails, the attempted
step restores the transform and motor state. Set `stepHeight` to zero for games that must never
auto-step.

## One-way and moving platforms

Set the reserved bit on a one-way collider's `CollisionFilter2D::categoryBits`. The motor passes
through it while moving up or sideways, lands only on a walkable top face while descending, and
ignores it during the controller's `dropThroughTime`. One-way behavior is a character-query policy;
it does not change rigid-body solver collisions.

Moving platforms should be static geometry moved by game code or kinematic bodies whose transforms
are finalized before constructing the query context. When grounded, the motor inherits the support
object's translation on the next call. Translation is collision-aware, so a platform cannot carry
the character through a solid wall. Platform rotation and velocity inheritance on jump are not
part of the 0.11 contract.

## Assets, template, and persistence

The focused `Lorenzo2DPhase7Example` and `templates/platformer` demonstrate the whole flow through
public API. `CharacterMotorPrefab` persists the one-way mask and `PlatformerControllerPrefab`
persists all controller tuning in JSON level version 7. Runtime velocity, timers, events, contacts,
and support handles are transient.

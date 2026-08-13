# Shared character motor

Lorenzo2D 0.11 provides `CharacterMotor2D`, a fixed-step, query-driven movement component shared
by later top-down, grid, platformer, and path-following controllers. It owns collision-aware
translation, not player intent, gravity, jump rules, pathfinding, or animation.

## Fixed-tick use

Attach one supported active collider (box, circle, or capsule) and the motor to the character. A
kinematic `RigidBody2D` is optional. Static and dynamic bodies are rejected: a static body declares
an immovable transform, while the solver and a dynamic motor would both try to own movement.

```cpp
l2d::GameObject& player = scene.createGameObject("Player");
player.transform.setPosition({100.f, 100.f});
auto& collider = player.addComponent<l2d::CapsuleCollider2D>(12.f, 40.f);
auto& motor = player.addComponent<l2d::CharacterMotor2D>();

// Every fixed tick, after moving platforms have reached this tick's position:
l2d::PhysicsQueryContext2D queries(scene);
const sf::Vector2f desiredDisplacement = inputDirection * speed * fixedDeltaTime;
const l2d::CharacterMoveResult2D result = motor.move(queries, desiredDisplacement);
```

Construct the query snapshot after static/kinematic obstacles are positioned and reuse it for all
characters during that tick. Call the motor from `onFixedPreSimulation`; do not scale the returned
displacements by delta time a second time. The caller supplies displacement, rather than velocity,
so all controllers can own their own acceleration and intent rules.

## Movement contract

`move` performs these deterministic stages:

1. inherit the previous support object's translation, when enabled;
2. recover existing overlaps, deepest first;
3. sweep the character and slide the unconsumed displacement along hit surfaces;
4. probe and optionally snap to walkable ground;
5. publish ground, wall, ceiling, support, and contact results.

`skinWidth` keeps a small separation from swept surfaces. `maximumSlideIterations` and
`maximumRecoveryIterations` bound work per call. The selected collider defaults to the lowest
active supported `ColliderId`; `setColliderId` can select another collider on the same object.
Sensors are excluded by default and `categoryMask` selects obstacle categories. The motor always
ignores all colliders on its owner, so a configured `ignoredObject` is invalid.

`CharacterMoveResult2D` separates inherited, recovery, requested movement, and ground-snap
displacements. `totalDisplacement()` adds the applied categories. `remainingDisplacement` reports
the portion the motor could not apply. A successful no-op is distinct from invalid configuration,
a missing collider, non-finite or oversized input, or an incompatible rigid body; those fail without
moving the character.

## Contacts and slopes

`upDirection` defaults to world up `(0, -1)`. A contact is ground when its normal lies within
`maximumSlopeAngleDegrees` of up, ceiling when it lies within that angle of down, and wall
otherwise. Natural sweep-and-slide follows walkable slope tangents. Jump and gravity policy live in
`PlatformerController2D`. `tryStep()` performs a transactional rise/traverse/settle sequence and
restores the transform and transient support state when any stage fails.

One-way platforms use an ordinary reserved collider category bit. Configure that bit through
`oneWayPlatformCategoryMask`; the motor ignores matching colliders while travelling up or sideways,
but accepts their walkable top face while descending. The explicit `ignoreOneWayPlatforms` overloads
of `move()` and `testMove()` support timed drop-through. Contacts and motor state report whether the
selected ground support is one-way.

Ground probing runs only when the requested displacement is not upward. With `snapToGround`, a
walkable surface within `groundProbeDistance` is approached until `skinWidth` remains. The state
stores a lifetime-aware support handle and its collider ID. On the next tick, translation of that
support is inherited before the caller's displacement. Teleports should call `clearState()` so an
old support delta is not applied.

## Capsule query detail

Overlap recovery uses exact capsule manifolds. Swept capsule queries use a fixed conservative
polygonal envelope, matching the existing conservative box-versus-capsule policy. They can report
a collision slightly early but are designed not to skip thin geometry because of fixed-step
tunnelling. Circle and box motor sweeps use their native query shapes.

## Prefabs and levels

`CharacterMotorPrefab` stores the full configuration. JSON level version 7 writes a built-in
`CharacterMotor2D` component record; versions 4 through 6 JSON and legacy text versions 1 through 3 remain
readable. Runtime contact state and the selected runtime collider ID are deliberately not saved.

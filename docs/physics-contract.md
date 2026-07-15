# Lorenzo2D physics contract

This document records the observable physics behavior on `master` before the planned internal refactors. It is a compatibility baseline, not a statement that every current behavior is the desired final design.

## Units and coordinate system

Lorenzo2D does not enforce physical units through types. The current integration formulas imply the following consistent interpretation:

- transform positions, collider dimensions, offsets, radii, penetrations, and contact points use engine/SFML coordinate units;
- velocity uses coordinate units per second;
- acceleration and gravity use coordinate units per second squared;
- mass is an arbitrary positive mass unit;
- force uses mass-units times coordinate-units per second squared;
- impulse uses mass-units times coordinate-units per second.

The default gravity is `(0, 980)`. Positive X points right and positive Y points down, matching the normal screen-space convention used by the engine.

## Fixed-step contract

Physics is stepped explicitly. Existing integration code calls `Scene::fixedUpdate(deltaTime)` before `PhysicsWorld2D::step(scene, deltaTime)`.

For a valid positive finite `deltaTime`, `PhysicsWorld2D::step` currently performs this sequence:

1. reset contact history when a different `Scene` instance is supplied;
2. preserve the previous contact list for event generation;
3. clear current contacts and events;
4. clear collider `isColliding` and rigid-body `isGrounded` flags;
5. integrate active rigid bodies;
6. collect one physics proxy per participating `GameObject`;
7. build broad-phase candidate pairs;
8. run collision filtering and narrow-phase manifold generation;
9. sort contacts and constraints deterministically;
10. solve velocity constraints for the configured iteration count;
11. solve positional correction for the configured iteration count;
12. derive grounded flags from solid contact normals;
13. generate `Begin`, `Stay`, and `End` events.

Invalid, zero, or negative delta times are a no-op and preserve the existing world state.

## Rigid-body integration

### Dynamic

Dynamic bodies have finite positive mass and non-zero inverse mass. Per step, acceleration is computed as:

`explicit acceleration + accumulated force / mass + optional world gravity * gravity scale`

Velocity is updated first, then the owning `GameObject` position is advanced using the updated velocity. This is semi-implicit Euler integration. The force accumulator is cleared after every valid integration step. Impulses change velocity immediately by `impulse / mass`.

### Kinematic

Kinematic bodies have zero solver inverse mass. They ignore acceleration, forces, gravity, and impulses, but their configured velocity moves the owning `GameObject` each step. They can push dynamic bodies through their relative velocity, while collision impulses do not alter the kinematic velocity.

### Static

Static bodies have zero solver inverse mass, are not integrated, and always expose zero velocity. Assigning a velocity to a static body leaves it at zero. A collider without an active rigid body is also treated as non-moving for broad-phase and solver purposes.

## Collider geometry and offsets

`Collider2D::worldPosition()` is `owner transform position + collider offset`.

- A box uses `worldPosition()` as its minimum corner. Its maximum corner is minimum plus size. Therefore, a box transform is currently a top-left/minimum-corner origin unless an offset is supplied.
- A circle uses `worldPosition()` as its center. The circle constructor and `setRadius` set the offset to `(radius, radius)`, so the owning transform behaves like the top-left of the circle's bounding square by default.
- Calling `CircleCollider2D::setOffset` overrides that center offset. Calling `setRadius` later resets the offset to `(radius, radius)`.

This box/circle origin asymmetry is intentional baseline behavior for now and is scheduled for normalization in a later phase.

## Collision filtering and sensors

Two colliders interact only when both bit tests pass:

- first mask intersects second category;
- second mask intersects first category.

Sensors still create contacts, set collider `isColliding`, participate in `isTouching`, and emit contact events. They do not create solver constraints, produce collision response, or contribute to grounded state.

Pairs with no moving participant are suppressed by the broad phase. Consequently, two colliders without active dynamic or kinematic bodies do not produce contacts, including when an existing rigid body component is temporarily inactive.

## Materials

Material values are clamped to `[0, 1]`, and dynamic friction is capped at static friction.

For a solid contact:

- combined restitution is the maximum of both restitution values;
- combined static friction is the geometric mean of both static-friction values;
- combined dynamic friction is the geometric mean of both dynamic-friction values.

Restitution is applied only when the initial closing speed exceeds `restitutionVelocityThreshold`.

## Contacts, normals, and ordering

Each current contact identifies two `GameObject` IDs and the two collider types. The lower object ID is always stored first. Contacts are sorted by:

1. first object ID;
2. second object ID;
3. first collider type;
4. second collider type.

The manifold normal points from the first stored collider toward the second stored collider. Position correction moves the first dynamic body opposite the normal and the second dynamic body along the normal.

Manifolds contain one representative contact point and one penetration depth. Box-box tie-breaking is deterministic in the order `+X`, `-X`, `+Y`, `-Y`.

## Contact events

Contact identity currently consists of object IDs plus collider types. Event behavior is:

- `Begin`: a current contact key was absent in the previous valid step;
- `Stay`: the same contact key exists in consecutive valid steps;
- `End`: a previous contact key is absent in the current valid step.

Disabling a collider, disabling a body when this leaves a static-static pair, changing filters, deactivating/destroying an object, or otherwise removing a pair can therefore emit `End`. Reintroducing the pair emits a new `Begin`.

The current key is insufficient for multiple same-type colliders on one object. Collider IDs are deliberately deferred to a later phase.

## Grounded state

Grounded state is reset every valid step and is assigned only to dynamic bodies from non-sensor solver constraints.

A dynamic first body is grounded when the contact normal Y component is at least `groundedNormalThreshold`. A dynamic second body is grounded when the normal Y component is at most the negative threshold. With positive Y downward, this represents support from below.

Grounded is currently a normal-direction heuristic, not a persistent support-contact model.

## Multiple colliders per GameObject

Although a `GameObject` can contain different concrete collider component types, physics currently retrieves only `getComponent<Collider2D>()`. Therefore, only the first collider returned through the base type participates in proxy creation, collision flags, contacts, and response. Additional colliders are ignored by `PhysicsWorld2D`.

This limitation is explicitly protected by the existing `testOnlyFirstBaseColliderParticipates` regression test. Multi-collider support is not part of this phase.

## Broad phase

The default broad phase is a uniform grid with a cell size of `128` and a maximum of `256` cells per proxy. Proxies that cannot be represented safely in the configured grid fall back to conservative pairing. Candidate pairs are deduplicated before narrow phase.

`BruteForce` remains an available reference mode. The regression suite checks contact and event equivalence between uniform-grid and brute-force execution, including a multi-tick deterministic scenario.

## Known limitations preserved by this baseline

- collision detection is discrete and occurs after full body integration, so sufficiently fast bodies can tunnel through thin geometry;
- no continuous collision detection or swept tests;
- no angular velocity, torque, inertia, or rotational collision response;
- boxes are axis-aligned and rotated boxes are unsupported;
- manifolds contain only one contact point;
- solver impulses accumulate only within the current step; there are no persistent contacts or cross-step warm starting;
- no damping, sleeping, or axis constraints;
- grounded state is derived from the current frame's contact normal rather than persistent support contacts;
- only the first base collider per object participates;
- contact identity has no collider ID;
- the physics world is stepped manually and is not yet owned or orchestrated by `Scene`.

## Regression coverage map

The pre-existing `Lorenzo2DPhysicsTests` suite already protects body modes and free integration, force lifetime, materials, filters, sensors, manifold orientation, deterministic contact ordering, `Begin`/`Stay`/`End`, grounded behavior, first-collider participation, collider deactivation, object destruction, short stack stability, invalid delta times, and single-step uniform-grid/brute-force equivalence.

`Lorenzo2DPhysicsStabilityTests` adds the missing long-horizon baselines:

- long-duration resting contact;
- an eight-box dynamic stack with per-tick finite-value checks;
- deterministic repeated simulations over 1,200 ticks;
- multi-tick uniform-grid/brute-force equivalence;
- explicit discrete tunnelling at high speed;
- inactive rigid-body contact termination and reactivation.

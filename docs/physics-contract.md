# Lorenzo2D physics contract

This document describes the observable Lorenzo2D 0.6 physics behavior. It is
the compatibility contract for the compound-collider and constraint solver.

## Units and coordinate system

Lorenzo2D uses screen-space coordinates: positive X points right, positive Y
points down, and the default gravity is `(0, 980)`. Position, collider geometry,
contact points, and distance-joint lengths use engine/SFML coordinate units.
Linear velocity is units per second and angular velocity is radians per second;
transform rotation remains degrees.

## Fixed-step contract

Call `Scene::fixedUpdate(deltaTime)` before
`PhysicsWorld2D::step(scene, deltaTime)`. A valid step:

1. resets transient collider and grounded flags;
2. selects a bounded adaptive CCD substep count;
3. integrates active rigid bodies for each substep;
4. collects one proxy for every active collider;
5. creates deterministic contact and distance-joint constraints;
6. warm-starts persistent contacts and iterates velocity constraints;
7. iterates contact and joint position constraints;
8. clears forces once after the complete outer step;
9. updates sleeping bodies and `Begin`, `Stay`, and `End` events.

A non-positive or non-finite delta time is a true no-op. It does not consume
forces or replace contacts, events, or statistics.

## Rigid bodies and angular dynamics

Dynamic bodies integrate acceleration, accumulated force, optional gravity,
velocity, accumulated torque, and angular velocity using semi-implicit Euler.
Kinematic bodies move with prescribed linear and angular velocity but have zero
solver inverse mass. Static bodies do not move. Collider-only objects are
treated as static.

`fixedRotation` defaults to `true` for compatibility. Set it to `false` before
using angular velocity, torque, inertia, angular impulse, or an off-center
linear impulse. Angular collision response uses the configured inertia. The
engine does not yet derive compound mass or inertia from attached shapes.

Dynamic bodies may sleep after remaining under the configured linear and
angular thresholds for `timeToSleep`. Velocity, force, impulse, body-mode, and
constraint changes wake affected bodies. Sleeping can be disabled per body or
per world.

## Collider geometry and local origins

Every collider offset is a local center point. It is scaled and rotated by the
owner transform before being added to the owner position.

- A box size is centered on that world position. It inherits absolute X/Y
  transform scale and owner rotation, producing an oriented box.
- A circle is centered on that world position and scales by the greater
  absolute owner-scale axis so it remains circular.
- A capsule is vertical in local space. Its spine uses absolute Y scale, its
  circular radius uses the greater absolute scale axis, and owner rotation rotates the result.
- A convex polygon transforms each of its 3 through 16 validated local vertices.
- Circle construction and `setRadius` do not modify the offset.

For a renderer whose transform is the top-left of its local bounds, migrate by
setting a box offset to `size * 0.5f` or a circle offset to
`{radius, radius}`. Negative transform scale mirrors local offsets while shape
dimensions remain positive.

All active colliders on a game object participate in physics as one compound
body. Collider pairs on the same owner never self-collide. `GameObject` exposes
`getComponents<T>()` for retrieving every matching concrete or base component.

## Collision detection and response

The narrow phase supports every pair of circles, oriented boxes, capsules, and
convex polygons. Exact tangency is a contact. Manifolds contain
one representative point, one normal, and one penetration depth. Degenerate
tie axes are deterministic.

The default signed uniform-grid broad phase deduplicates multi-cell pairs and
uses conservative fallback pairing for shapes that exceed the configured cell
budget. `BruteForce` remains a reference mode. At least one object in a
reported pair must contain an active, moving Dynamic or Kinematic body.

Velocity solving applies accumulated normal, restitution, and friction
impulses, including angular leverage. Cached normal and tangent impulses are
reused on the next step when `warmStarting` is enabled. Position correction is
iterative and separately configurable.

Adaptive CCD divides the outer step according to the fastest active body and
smallest active collider extent. `maximumCcdSubsteps` bounds the cost and
`ccdMotionThreshold` tunes the permitted translation per substep. This prevents
ordinary thin-wall tunnelling but is not an unbounded swept-shape guarantee.

## Materials, filters, and sensors

Restitution and friction are clamped to `[0, 1]`; dynamic friction cannot exceed
static friction. A pair uses maximum restitution and the geometric mean of its
friction coefficients. Restitution is applied only above the configured
closing-speed threshold.

Both category/mask bit tests must pass. Sensors still create manifolds,
collider flags, queries, and events, but never apply impulses, correction, or
grounded state.

## Stable contacts and events

Every collider has a process-stable nonzero `ColliderId`. A contact contains
both object IDs, both collider IDs, both collider types, its manifold, and its
sensor flag. Contact identity is the ordered collider-ID pair, so multiple
same-type shapes on one owner remain distinct.

`contacts()` is deterministic. `isTouching` queries an object pair and
`isColliderTouching` queries an exact collider pair. Events are:

- `Begin` when a collider pair was absent from the previous valid outer step;
- `Stay` when it persists;
- `End` when it disappears.

Consume contacts and events in `onFixedPostSimulation`. `reset()` clears world
history; `reset(scene)` also clears every collider flag and grounded body flag
in that scene.

## Grounded state

Only dynamic bodies become grounded. The state is derived from non-sensor
contact normals using `groundedNormalThreshold` and is reset before each valid
step. It remains a normal-direction heuristic rather than a separate support
graph.

## Distance joints

`DistanceJoint2D` connects two object IDs at transform-aware local anchors. Its
rest length, normalized stiffness, nonnegative damping, and collision behavior
are configurable. Connected objects do not collide by default; enable
`collideConnected` to opt back in. Destroyed, inactive, self-referential, or
missing connected objects make the joint inert for that step.

## Statistics and limits

`broadPhaseStats()` reports proxy/grid/pair counters for the most recent valid
step. `stepStats()` reports CCD substeps, active and sleeping bodies, maximum
contact and joint constraints in a substep, and reused warm-start contacts.

Current deliberate limits are strictly convex polygons only, one contact point
per manifold, distance joints only, no automatic compound mass-property calculation,
bounded substep CCD, and explicit world stepping rather than scene ownership. Read-only ray,
point, overlap, and shape-cast contracts are documented in
[`physics-queries.md`](physics-queries.md).

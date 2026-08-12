# Physics queries and character collider foundations

Lorenzo2D 0.6 exposes deterministic world queries for picking, line of sight, movement prediction,
placement previews, and later navigation work. Queries are read-only: they do not advance physics,
allocate contact IDs, change `isColliding`, or alter the world's contact/event lists.

## Query context

`PhysicsQueryContext2D` copies the active colliders' geometry, filters, IDs, sensor state, and
lifetime-aware object handles. Build one snapshot after gameplay has established the transforms for
a fixed tick, then reuse it for all queries in that tick:

```cpp
l2d::PhysicsQueryContext2D queries(scene);

if (const auto hit = queries.raycast(playerPosition, cursorWorldPosition))
{
    // hit->point and hit->normal describe the hit surface.
}

l2d::PhysicsQueryFilter2D filter;
filter.ignoredObject = player.id();
if (const auto obstacle = queries.castCircle(playerPosition, desiredPosition, playerRadius, filter))
{
    desiredPosition = obstacle->point + obstacle->normal * playerRadius;
}
```

`PhysicsWorld2D::createQueryContext(scene)` is the equivalent factory. Convenience methods with the
same names are also available directly on `PhysicsWorld2D`; they create a fresh context per call.
Use those for occasional queries and a context for batches.

A context remains a stable snapshot when objects move, deactivate, or are destroyed. Its
`GameObjectHandle` results become invalid normally when their objects disappear. Create a new
context to observe structural or transform changes.

## Public operations

| Operation | Result | Typical use |
| --- | --- | --- |
| `raycast` | nearest optional hit | line of sight, weapon trace |
| `raycastAll` | all sorted hits | penetration trace, editor inspection |
| `pointQuery` | all colliders containing a point | mouse/touch picking |
| `overlapCircle` | all intersecting colliders | radial trigger, clearance |
| `overlapBox` | all intersecting colliders | placement preview, area selection |
| `castCircle` | earliest optional swept hit | top-down character prediction |
| `castBox` | earliest optional swept hit | platform or box movement prediction |

Coordinates are world coordinates. Box sizes must be finite and positive. Circle radii must be
finite and non-negative. Invalid input fails closed with no hits. A zero-length ray acts as a point
test: it reports fraction and distance zero when its point is inside a collider.

`PhysicsQueryHit2D::normal` points out of the hit collider toward the query shape. A ray that starts
inside a collider has a zero normal because no unique entry surface exists. Hits are sorted by
distance and then stable `ColliderId`, including equal-distance compound colliders.

## Filtering

`PhysicsQueryFilter2D` has three independent controls:

- `categoryMask` accepts a collider when it shares at least one category bit;
- `includeSensors` is false by default;
- `ignoredObject` excludes every collider owned by one object, which is useful for character casts.

Query filtering intentionally tests collider categories, not their simulation mask. A query is not
a collider and therefore has no reciprocal collision mask.

## Collider shapes

The simulation, broad phase, queries, and debug renderer share geometry for:

- oriented boxes;
- circles;
- vertical-local `CapsuleCollider2D` shapes;
- strictly convex `ConvexPolygonCollider2D` shapes with 3 through 16 vertices.

Capsule height is total tip-to-tip height and cannot be smaller than twice its radius. Capsules are
the primary character shape: their local spine follows Y and inherits owner rotation. With
non-uniform owner scale, the spine uses absolute Y scale and the circular radius uses the larger
absolute scale component, keeping the physics shape conservative.

Polygon construction rejects non-finite, collinear, concave, or oversized input. `setVertices` is
transactional and normalizes accepted winding to counter-clockwise order. Convex polygons are the
official 0.6 slope representation; there is no separate edge collider.

All shape pairs generate manifolds. Circle casts against every shape and box casts against circles,
boxes, and polygons are continuous. Box-versus-capsule casts use a conservative 12-segment-per-cap
polygonal envelope: they can report a hit slightly early, by at most the cap envelope error, but do
not skip a thin collider because of time stepping. With the fixed subdivision that radial envelope
error is below 0.9% of the capsule radius.

## Level format

Legacy level format version 3 added optional `capsule_collider` and
`convex_polygon_collider` records. Text versions 1 through 3 remain readable; JSON version 4 is
the current save format. Prefab validation and loading reject malformed capsule dimensions and
invalid polygon vertex sets before mutating a destination scene or document.

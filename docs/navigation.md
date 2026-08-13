# Navigation and point-and-click movement

Lorenzo2D 0.12 adds a Cartesian navigation layer that is shared by orthogonal and isometric
presentation. It combines `NavigationGrid2D`, deterministic `AStarPathfinder2D`,
`PathFollower2D`, and bounded local avoidance. All movement still passes through
`TopDownController2D` and `CharacterMotor2D`, so a navigating agent keeps the same acceleration,
collision, filtering, and fixed-step behavior as a directly controlled top-down character.

## Build a grid

A grid stores one positive traversal cost and one walkability flag per cell. World coordinates use
the top-left grid origin; `cellCenter()` and `worldToCell()` are exact inverse operations inside a
cell. Mutations advance `revision()`, which lets agents detect stale paths.

```cpp
l2d::NavigationGridConfig2D config;
config.size = {64u, 48u};
config.cellSize = {32.f, 32.f};
config.connectivity = l2d::NavigationConnectivity2D::EightWay;
l2d::NavigationGrid2D grid(config);

grid.setWalkable({12, 8}, false);
grid.setTraversalCost({20, 10}, 2.5f);
```

`navigationGridFromTileMap()` combines every visible layer in stable layer order. A non-navigable
tile blocks its cell; otherwise the greatest `movementCost` wins. Empty tiles have no effect.
Hidden layers are ignored unless requested. This preserves Tiled/ASCII navigation metadata without
creating another map format.

`bakeObstacles()` conservatively overlaps the full cell area expanded by the agent radius against
an immutable `PhysicsQueryContext2D`. It only blocks additional cells and preserves tile-defined
walls and costs. Ignore the player and other dynamic agents in the bake filter; local avoidance
handles those at runtime.

## Deterministic A*

```cpp
l2d::AStarPathfinder2D pathfinder;
const l2d::NavigationPath2D path =
    pathfinder.findWorldPath(grid, player.transform.position(), clickedWorldPosition);
if (path.succeeded()) follower.setPath(path);
```

Four-way search uses Manhattan distance; eight-way search uses octile distance. Both scale the
admissible heuristic by the grid's lowest walkable cost. Candidate neighbors have a fixed order,
and equal scores break on heuristic then row-major index. The result and visited-node limit are
therefore replay-stable on one supported build. Diagonal corner cutting is disabled by default.
Collinear world waypoints collapse by default while the complete cell path remains available.

Invalid/outside or blocked endpoints are reported separately from `NoPath` and
`SearchLimitReached`. Failed searches never return a partial movement command.

## Follow a click destination

Attach `BoxCollider2D`, `CharacterMotor2D`, `TopDownController2D`, then `PathFollower2D` to the
agent. Sample pointer edges once per rendered frame, queue the resulting world destination, and
consume it during a fixed tick:

Set `PathFollowerConfig2D::positionOffset` to the collider center or sprite footpoint when the
owner transform uses a top-left anchor. Planning, avoidance, arrival, and click destinations then
all use that navigation anchor while the motor continues moving the owner transform.

```cpp
if (pendingClick)
    follower.setDestination(grid, pathfinder, *pendingClick);

const l2d::PhysicsQueryContext2D queries(scene);
const l2d::PathFollowResult2D result = follower.follow(queries, fixedDeltaTime);
if (result.requestedRepath || follower.isPathStale(grid))
    follower.setDestination(grid, pathfinder, follower.state().destination);
```

The follower advances tolerance-bounded waypoints, slows near its goal, reports `Arrived`, and
reports `Stuck` plus `requestedRepath` after a configurable lack of progress. Runtime paths are
commands, not save data. Level format version 8 persists only `PathFollowerPrefab` tuning.

## Local avoidance

The neighbor overload of `follow()` accepts nearby `NavigationAgentSnapshot2D` values. Avoidance
sorts valid neighbors by distance and stable object ID, caps the considered count, predicts closest
approach inside a finite horizon, and adds a speed-bounded separating correction. Static collision
remains motor/query work. The default algorithm is intentionally local: it does not replace path
replanning, guarantee globally deadlock-free crowds, or provide cross-compiler network lockstep.

## Fixed-step order and limits

Move static or kinematic obstacles, rebuild a shared query snapshot, process click commands and
agents, then run physics. Rebuild or patch the grid when topology changes. A path revision signals
that a replan is required; it does not silently mutate the current command.

The 0.12 baseline supports rectangular grids, positive cell costs, four/eight-way A*, point-sized
waypoints, collision-aware following, and lightweight avoidance. Navigation meshes, hierarchical
pathfinding, off-mesh links, flow fields, asynchronous search, and authoritative network lockstep
are outside this phase.

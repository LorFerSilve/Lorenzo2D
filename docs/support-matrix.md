# Feature support matrix

Lorenzo2D uses three support levels:

- **experimental**: implemented and demonstrated, but its public workflow or edge-case coverage is
  still expected to change;
- **supported**: public API, focused tests, an installed-package consumer path, a playable example,
  and end-to-end documentation are present;
- **production-tested**: supported plus measured use in a released game or an equivalent sustained
  production workload.

`Planned` is not a support level and must never be presented as an available feature.

## Current matrix for 0.9.0

| Development path | Level | Evidence | Missing before supported |
| --- | --- | --- | --- |
| General engine foundation | Experimental | fixed-step application, scenes/ECS, rendering, assets, serialization, physics, tests, and CMake package | stable 1.0 compatibility policy, complete game-facing services, external usage evidence |
| Orthogonal platformer | Experimental | sandbox movement plus shared sweep-and-slide motor with slopes, grounded/wall/ceiling state, ground snap, overlap recovery, and moving-platform translation | platformer controller, gravity/jump policy, one-way platforms, step handling, focused template and tutorial |
| Orthogonal top-down | Planned | configurable 2D actions, shared collision-aware motor, layered orthogonal tile data, tile/object properties, collision/trigger/navigation roles, Tiled import, sprites, animation, sensors, render layers and automatic world-Y sorting | top-down/grid controllers, runtime trigger dispatcher and focused template |
| Point-and-click movement | Planned | unified pointer state, camera-aware world coordinates, deterministic queries, convex obstacles, and shared collision-aware motor | navigation grid, A*, path following, avoidance and focused template |
| Isometric | Planned | projection/inverse-projection contract, projected-Y depth, sprite footpoints, isometric Tiled data import, projected tile submission, and orthographic camera primitives | production isometric projection, projected-bounds culling, navigation, placement grid and focused template |

## Platform matrix

| Platform/toolchain | Current CI status | Support statement |
| --- | --- | --- |
| Windows MSVC | Built and tested in Release | Experimental desktop support |
| Linux GCC | Built and tested in Release | Experimental desktop support |
| Linux Clang | Built and tested in Debug and ASan/UBSan | Experimental desktop support |
| macOS | Not in CI | Not currently supported |
| Web | No target | Not currently supported |
| Android/iOS | No target or touch lifecycle | Not currently supported |

## Promotion checklist

A development path can move from experimental to supported only when:

1. its public engine API is complete for the documented baseline use case;
2. it has a focused example or project template using only installed public API;
3. regression tests cover normal, boundary, invalid-input, lifetime, and deterministic cases;
4. CI builds the template on every supported desktop toolchain;
5. a tutorial starts from an empty consumer project and reaches a playable result;
6. known exclusions are explicit and do not contradict the advertised baseline;
7. relevant performance scenarios are measured and published.

Production-tested is intentionally evidence-based and cannot be assigned only because a demo or
microbenchmark succeeds.

## Licensing status

The repository does not yet contain a license. Until the owner selects and adds one, external users
do not receive a documented permission grant to copy, modify, or distribute Lorenzo2D. This status
must be resolved before calling any development path supported for external consumers.

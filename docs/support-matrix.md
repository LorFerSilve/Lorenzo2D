# Feature support matrix

Lorenzo2D uses three support levels:

- **experimental**: implemented and demonstrated, but its public workflow or edge-case coverage is
  still expected to change;
- **supported**: public API, focused tests, an installed-package consumer path, a playable example,
  and end-to-end documentation are present;
- **production-tested**: supported plus measured use in a released game or an equivalent sustained
  production workload.

`Planned` is not a support level and must never be presented as an available feature.

## Current matrix for 0.13.0

| Development path | Level | Evidence | Missing before supported |
| --- | --- | --- | --- |
| General engine foundation | Experimental | fixed-step application, scenes/ECS, rendering, assets, serialization, physics, tests, and CMake package | stable 1.0 compatibility policy, complete game-facing services, external usage evidence |
| Orthogonal platformer | Experimental | public fixed-step controller with acceleration, gravity, variable jumps, coyote/buffer policy and events; slope-aware motor, transactional steps, category-based one-way drop-through, moving-platform translation, focused tests/example/template/tutorial and benchmark | CI template matrix, external usage evidence, platform rotation support |
| Orthogonal top-down | Experimental | configurable 2D actions, free analog top-down controller, transactional grid-step controller, shared collision-aware motor, layered orthogonal tile data, Tiled import, sprites, animation, sensors, focused example and installed-package template | runtime trigger dispatcher, CI template matrix, external usage evidence |
| Point-and-click movement | Experimental | camera-aware pointers, tile/physics navigation grid, weighted deterministic A*, collision-aware path following, bounded local avoidance, focused tests/example/installed-package template, CI template build and benchmark | external usage evidence, large-crowd/deadlock evidence, navigation-mesh or hierarchical search if required by a game |
| Isometric | Experimental | production diamond projection and inverse, conservative projected bounds, bounded tile picking and placement grid, projected-Y foot-point ordering, Tiled isometric import, projected tile submission, Cartesian navigation reuse, focused tests/example/installed-package template, CI template build and diagnostic benchmark | external usage evidence, larger real-game isometric scene evidence, tutorial-level usability feedback |

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

Lorenzo2D is licensed under the MIT License. The repository-level [LICENSE](../LICENSE) file
documents the permission grant and warranty disclaimer for external users.

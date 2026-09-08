# Feature support matrix

Lorenzo2D uses three support levels:

- **experimental**: implemented and demonstrated, but its public workflow or edge-case coverage is
  still expected to change;
- **supported**: public API, focused tests, an installed-package consumer path, a playable example,
  end-to-end documentation, and the evidence required by the promotion checklist are present;
- **production-tested**: supported plus measured use in a released game or an equivalent sustained
  production workload.

`Planned` is not a support level and must never be presented as an available feature.

## Current matrix for 1.0.0

| Development path | Level | Evidence | Missing before supported |
| --- | --- | --- | --- |
| General engine foundation | Experimental | stable 1.x source/package policy; fixed-step application, scenes/ECS, rendering, assets, serialization, physics, UI, audio, versioned saves, tests, diagnostics, installed-package consumers, and four starter projects | external sustained usage evidence |
| Orthogonal platformer | Experimental | public fixed-step controller; slope-aware motor; one-way/moving-platform behavior; focused tests/example/template/tutorial; package consumption and benchmark coverage | external usage evidence, cross-toolchain starter validation, platform rotation support |
| Orthogonal top-down | Experimental | configurable 2D actions; free analog and grid-step controllers; shared motor; layered tile data; Tiled import; focused example/template/tutorial; package consumption | runtime trigger dispatcher, external usage evidence, cross-toolchain starter validation |
| Point-and-click movement | Experimental | camera-aware pointers; tile/physics navigation grid; deterministic A*; collision-aware following; local avoidance; tests/example/template; package consumption and benchmark coverage | external usage evidence, large-crowd/deadlock evidence, hierarchical/navmesh search if a production game requires it |
| Isometric | Experimental | reversible diamond projection; conservative bounds; bounded picking/placement; projected-Y ordering; Tiled import; Cartesian navigation reuse; tests/example/template/tutorial; package consumption and diagnostics | external usage evidence, larger real-game isometric scene evidence, tutorial-level usability feedback |

The 1.0 version number defines compatibility; it does not manufacture production evidence. Support
levels remain evidence-based.

## Platform matrix

| Platform/toolchain | Current CI status | Support statement |
| --- | --- | --- |
| Windows MSVC | Built and tested in Release | Experimental desktop support |
| Linux GCC | Built and tested in Release; installed package and four starters validated | Experimental desktop support |
| Linux Clang | Built and tested in Debug and ASan/UBSan | Experimental desktop support |
| macOS | Not in CI | Not currently supported |
| Web | No target | Not currently supported |
| Android/iOS | No target or touch lifecycle | Not currently supported |

## Promotion checklist

A development path can move from experimental to supported only when:

1. its public engine API is complete for the documented baseline use case;
2. it has a focused example or project template using only installed public API;
3. regression tests cover normal, boundary, invalid-input, lifetime, and deterministic cases;
4. CI builds the relevant starter workflow on every platform claimed as supported;
5. a tutorial starts from an empty consumer project and reaches a playable result;
6. known exclusions are explicit and do not contradict the advertised baseline;
7. relevant performance scenarios are measured and published;
8. external usage or equivalent sustained validation demonstrates the workflow outside the
   repository's own synthetic examples.

Production-tested is intentionally evidence-based and cannot be assigned only because a demo or
microbenchmark succeeds.

## Licensing status

Lorenzo2D is licensed under the MIT License. The repository-level [LICENSE](../LICENSE) file
documents the permission grant and warranty disclaimer for external users.

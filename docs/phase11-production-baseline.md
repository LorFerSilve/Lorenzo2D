# Phase 11 production baseline and limits

This document records the measured hardening evidence and bounded workload envelopes available when
Phase 11 was completed. It is a **validation baseline**, not a claim that Lorenzo2D is
production-tested or that every listed maximum is a recommended game-scale budget.

The support level remains evidence-based; see [support-matrix.md](support-matrix.md).

## Validation provenance

The Phase 11.7 heavy workflow was executed end-to-end before merge on GitHub Actions run
`34289125697` using Ubuntu 24.04 hosted runners and Lorenzo2D 1.1.0. The validation head was
`a918f0db9b8a2785cc2cb3a3e4329df9d8c4f5ec`; the only later branch change before merge restored
the normal policy that skips heavy nightly jobs on pull requests.

The run produced and uploaded all three 21-day artifact groups:

- Release standard/soak stress plus benchmark JSON/CSV/log;
- ASan/UBSan standard stress report/log;
- four fixed-seed extended-fuzz logs.

Wall-clock measurements below are one hosted-runner sample. They are diagnostic data, not
performance thresholds.

## Stress evidence

### Release standard

All nine scenarios passed:

| Scenario | Operations | Wall clock |
| --- | ---: | ---: |
| tilemap-streaming | 12,352 | 2.587 ms |
| renderables-handles | 32,829 | 3.158 ms |
| physics-queries-contacts | 12,672 | 55.332 ms |
| navigation-replans | 1,024 | 60.701 ms |
| fixed-step-replay | 320,000 | 23.970 ms |
| asset-lifecycle | 4,000 | 0.212 ms |
| ui-churn | 10,000 | 2.543 ms |
| audio-voices | 1,024 | 165.457 ms |
| save-persistence | 80 | 3,754.865 ms |

The standard profile covers the representative Phase 11 scale of 10k+ tile cells, 1k+
renderables, 500+ colliders, and 250+ logical navigation clients.

### Release soak

All nine scenarios passed:

| Scenario | Operations | Wall clock |
| --- | ---: | ---: |
| tilemap-streaming | 13,288 | 2.771 ms |
| renderables-handles | 1,024,121 | 81.663 ms |
| physics-queries-contacts | 130,560 | 696.851 ms |
| navigation-replans | 4,096 | 241.199 ms |
| fixed-step-replay | 27,648,000 | 1,916.648 ms |
| asset-lifecycle | 40,000 | 1.878 ms |
| ui-churn | 100,000 | 45.936 ms |
| audio-voices | 8,192 | 660.827 ms |
| save-persistence | 320 | 13,878.111 ms |

The soak fixed-step scenario executes 432,000 ticks per replay, twice, representing two simulated
hours at 60 Hz per trace and requiring full deterministic equivalence.

## Sanitizer evidence

The full `standard` stress profile passed all nine scenarios under Clang AddressSanitizer and
UndefinedBehaviorSanitizer with:

- leak detection enabled;
- halt-on-first ASan error;
- initialization-order checks;
- UBSan stack traces and halt-on-first error.

No unresolved sanitizer finding was present in the validated run.

Sanitizer timing is intentionally not compared with Release timing. Instrumentation materially
changes allocation, memory, and execution costs.

## Fuzz evidence

Every registered property scenario passed **2,048 cases for each of four fixed seeds**:

- `0x4c324446555a5a31`;
- `0x9e3779b97f4a7c15`;
- `0xd1b54a32d192ed03`;
- `0x94d049bb133111eb`.

Covered scenarios:

- save JSON;
- level JSON/prefabs;
- Tiled JSON;
- physics queries;
- navigation-grid configuration/costs;
- UI input/bounds;
- resource lookup/root confinement.

That is 8,192 deterministic mutation/property cases per scenario in the validated extended run.
Failures remain reproducible from seed, case count, and scenario.

## Determinism evidence

Phase 11 provides two complementary replay levels:

- the stress runner's long fixed-step deterministic soak;
- the 8,192-tick subsystem-integrated replay across Scene/ECS, physics/query snapshots, navigation,
  UI, and fixed-step scheduling.

The integrated suite proves equality across different presentation-frame cadences and verifies exact
first input/state divergence reporting. Floating-point state uses the documented 1/4096-unit
quantized hash contract rather than raw object memory.

Cross-platform bitwise floating-point physics lockstep is **not** claimed.

## Diagnostic benchmark snapshot

The Release nightly run recorded:

| Benchmark | Iterations | Wall clock |
| --- | ---: | ---: |
| input action sampling | 120 | 37.384 ms |
| scene fixed update | 120 | 7.551 ms |
| render queue build | 120 | 37.258 ms |
| projected depth full sort | 120 | 55.436 ms |
| physics uniform grid | 30 | 20.821 ms |
| physics brute force | 30 | 916.467 ms |
| physics query snapshot | 50 | 9.379 ms |
| physics query ray batch | 100 | 384.145 ms |
| character motor corridor | 1,000 | 3.936 ms |
| top-down controller batch | 200 | 2,177.786 ms |
| platformer controller batch | 200 | 3,324.245 ms |
| navigation A-star batch | 10 | 486.464 ms |
| tile-map full build | 5 | 11.182 ms |
| layered tile-map build | 5 | 4.723 ms |
| tile-map view culling | 1,000 | 0.179 ms |

These values establish trend provenance only. No hosted-runner millisecond value is a merge/nightly
pass-fail threshold.

## Hard workload envelopes

The following are default/hard guards in the public 1.1.0 API.

| Boundary | Envelope |
| --- | --- |
| save entries | 4,096 default maximum |
| save key bytes | 128 default maximum |
| save schema bytes | 128 default maximum |
| save string bytes | 1 MiB default maximum |
| serialized save bytes | 4 MiB default maximum |
| level objects | 100,000 hard maximum |
| level parser input | 64 MiB default byte envelope |
| Tiled JSON parser input | 64 MiB default byte envelope |
| tile/navigation grid cells | 16,777,216 hard maximum |
| tile-map layers | 1,024 hard maximum |
| tile-map objects | 1,000,000 hard maximum |
| convex polygon vertices | 16 hard maximum |
| local-avoidance neighbors | 1,024 hard maximum; default considered count 8 |
| A* visited nodes | 100,000 default search limit |
| particle emitter capacity | 100,000 hard maximum; default 1,000 |
| replay ticks | 1,000,000 default maximum |
| replay input bytes/tick | 4,096 default maximum |
| replay total input bytes | 64 MiB default maximum |
| profiler sample window | 120 default samples |
| profiler named scopes | 256 default maximum |
| profiler scope-name bytes | 96 default maximum |
| physics velocity/position iterations | values above 64 are clamped |
| malformed-input cases/invocation | 4,096 hard maximum |
| fuzz mutation buffer | 64 KiB hard maximum |

Parser/save limits are configurable where their public limit structures expose a value. A smaller
application budget is recommended for untrusted content when the game does not need the default
envelope.

These guards prevent accidental unbounded work. They do **not** imply that allocating the hard
maximum simultaneously in every subsystem is a supported production target.

## Known production boundaries

Phase 11 deliberately does not claim:

- production-tested support based only on repository-owned synthetic workloads;
- stable millisecond budgets on shared GitHub-hosted hardware;
- macOS, web, Android, or iOS support;
- cross-machine lockstep floating-point physics;
- general concurrent mutation of Scene/ECS/renderer/audio/input state;
- unlimited asset, navigation, save, tile, replay, or parser workloads;
- that legacy mutable escape hatches preserve invariants automatically.

The largest remaining confidence gap is sustained usage in an external/released game or an
equivalent independent workload. Until that evidence exists, the support matrix remains
Experimental.

## Ongoing evidence

The scheduled `Nightly Extended Validation` workflow continues collecting:

- Release standard + soak correctness reports;
- ASan/UBSan standard stress;
- extended deterministic fuzz;
- benchmark trend artifacts.

A later support promotion should cite accumulated evidence and real usage rather than merely the
completion date of Phase 11.

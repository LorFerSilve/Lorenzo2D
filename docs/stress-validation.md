# Stress and soak validation

Phase 11.4 adds a repeatable, bounded stress runner that validates long-running engine behavior and
diagnostic invariants without turning unstable wall-clock measurements into merge gates.

The executable is `Lorenzo2DStressValidation`. It is built with the regression tests and uses only
public Lorenzo2D APIs. Pull-request CI runs the default `smoke` profile in the headless partition,
including ASan/UBSan. The scheduled Phase 11.7 workflow keeps pull requests bounded while running
the full `standard` and `soak` profiles nightly; `standard` also runs under ASan/UBSan.

## What makes a scenario fail

Stress scenarios fail on correctness or observability regressions, including:

- crashes or exceptions;
- invalid or unexpectedly surviving handles;
- entity/component/collider/query counter drift;
- tile streaming/culling accounting drift;
- failed or stale navigation replans;
- deterministic fixed-step replay divergence;
- asset snapshot/live-binding lifetime violations;
- UI interaction-state drift;
- leaked audio voices;
- save/load byte-counter drift;
- nondeterministic save serialization;
- failed transactional save replacement or leaked `.tmp` / `.bak` files.

Scenario duration is reported for diagnosis only. There is deliberately no milliseconds threshold in
Phase 11.4. Performance budgets should only become blocking after enough stable machine-specific
history exists.

## Fixed profiles

Arbitrary numeric workload arguments are not accepted. This keeps validation bounded and prevents a
mistyped CI command from creating unbounded work.

| Workload | `smoke` | `standard` | `soak` |
| --- | ---: | ---: | ---: |
| tilemap size | 64 x 64 = 4,096 | 128 x 96 = 12,288 | 128 x 96 = 12,288 |
| stream-region changes | 8 | 64 | 1,000 |
| renderable scene objects | 256 | 1,024 | 2,048 |
| dynamic colliders | 96 | 512 | 512 |
| physics ticks | 8 | 24 | 240 |
| physics queries per tick | 4 | 16 | 32 |
| logical navigation clients | 32 | 256 | 256 |
| path plans per client | 2 | 4 | 16 |
| navigation grid | 48 x 48 | 128 x 128 | 128 x 128 |
| fixed-step ticks per replay | 600 | 10,000 | 432,000 |
| fixed-step objects | 8 | 16 | 32 |
| asset replace/unload cycles | 64 | 1,000 | 10,000 |
| UI buttons | 64 | 256 | 512 |
| UI interactions | 256 | 5,000 | 50,000 |
| audio voices cycled | 64 | 512 | 4,096 |
| save entries | 128 | 4,000 | 4,000 |
| bytes per save string value | 64 | 768 | 768 |
| in-memory save/load cycles | 4 | 32 | 128 |
| transactional file cycles | 2 | 8 | 32 |

At 60 Hz, 432,000 fixed ticks represent two hours of simulated time. The soak scenario runs those
ticks twice as fast as the machine permits and compares the complete bounded replay traces.

The standard profile deliberately meets or exceeds the roadmap's representative scale for 10k+
tiles, 1k+ renderables, 500+ colliders, and 250+ navigation clients. Lorenzo2D currently exposes
navigation as deterministic pathfinding/following primitives rather than one global
`NavigationAgent` manager, so the runner models 256 independent logical clients issuing replans.

## Scenarios

### `tilemap-streaming`

Loads a fully rendered tilemap, verifies a full-map view submits every tile, then repeatedly moves a
bounded stream region. It checks resident/non-resident and visible/culled accounting plus draw-call
and rendered-item diagnostics.

### `renderables-handles`

Builds and repeatedly sorts a large render queue, destroys a deterministic subset, verifies their
`GameObjectHandle` values expire, and checks scene/render diagnostic counters against the remaining
objects.

### `physics-queries-contacts`

Runs hundreds of real `Dynamic` rigid bodies with gravity disabled. Each tick resets them to a
known state and deterministically introduces/removes overlaps, producing contact churn while repeated
raycasts exercise immutable query snapshots. Proxy and query diagnostics must remain exact.

### `navigation-replans`

Runs independent path requests across a bounded grid while changing a valid traversal cost between
replan waves. Every path must succeed at the current grid revision; expansion and explicit replan
counters are checked.

### `fixed-step-replay`

Runs a Scene through `FixedStepScheduler` with deterministic integer-only component state, records
one canonical state hash per tick, repeats the simulation from a fresh scene, and requires complete
trace equivalence. Presentation state, pointers, allocator addresses, and game-object IDs are not
hashed.

### `asset-lifecycle`

Repeatedly replaces and unloads a live sound-buffer slot plus animation snapshots. Live generations
must advance exactly, registry counts must return to zero, and external snapshots must survive
registry removal.

### `ui-churn`

Exercises press/release activation, label mutation, enable/disable, visibility changes, and
interaction cancellation across a large button set. Button count and capture state must remain
stable.

### `audio-voices`

Uses SFML's null playback device, starts looping voices in bounded batches, validates the
active-voice diagnostic count, and stops every voice. Each batch must return to zero live voices.

### `save-persistence`

Builds a large valid document, requires deterministic repeated serialization/round trips, validates
save/read byte counters, and repeatedly replaces an on-disk save through the transactional
temporary/backup policy. The standard and soak documents must occupy at least 60% of the configured
4 MiB total-size envelope.

## Running the profiles

Build tests normally:

```sh
cmake -S . -B build -DL2D_BUILD_TESTS=ON
cmake --build build --config Release --target Lorenzo2DStressValidation
```

Run the bounded PR profile:

```sh
./build/bin/Lorenzo2DStressValidation --profile smoke
```

Run the representative standard profile:

```sh
./build/bin/Lorenzo2DStressValidation --profile standard
```

Run the accelerated soak:

```sh
./build/bin/Lorenzo2DStressValidation --profile soak
```

Multi-config generators may place the executable in a configuration-specific directory.

A single scenario can be isolated:

```sh
./build/bin/Lorenzo2DStressValidation \
  --profile standard \
  --scenario physics-queries-contacts
```

## Machine-readable reports

Pass `--json <path>` to write report format version 1:

```sh
./build/bin/Lorenzo2DStressValidation \
  --profile standard \
  --json build/stress-standard.json
```

Each scenario records pass/fail status, operation count, a deterministic/checking checksum where
applicable, wall-clock milliseconds, and a failure message. Timing values are intentionally
diagnostic and therefore are not expected to be deterministic.

## Sanitizer use

For leak/lifetime validation, build the runner with the existing sanitizer configuration and execute
the desired profile:

```sh
cmake --preset sanitize
cmake --build --preset sanitize --target Lorenzo2DStressValidation
./build/sanitize/bin/Lorenzo2DStressValidation --profile standard
```

The PR `smoke` profile already participates in the repository's ASan/UBSan matrix. The nightly
workflow runs Release `standard` plus `soak`, runs `standard` again under ASan/UBSan, and
retains the JSON and console reports for 21 days.

## Current boundary

Phase 11.4 establishes reproducible stress/soak workloads and an accelerated Scene/fixed-step replay
soak. Phase 11.7 now schedules the heavier profiles and retains their artifacts. Full-engine
cross-platform replay equivalence for floating-point physics or presentation services is still not
claimed; the remaining Phase 11 work is the public API/failure-path audit.

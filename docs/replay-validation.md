# Integrated deterministic replay validation

Phase 11.6 extends the replay foundation from synthetic traces and the integer-only fixed-step soak to
an engine-integrated regression that runs canonical input through multiple public subsystems for
8,192 fixed ticks.

The registered target is `Lorenzo2DIntegratedReplayTests`. It is headless and bounded; normal pull
requests execute it as part of the existing CTest partitions.

## Integrated scenario

Each replay run owns fresh engine state and drives the same canonical input sequence through:

- `FixedStepScheduler`;
- `Scene::fixedUpdate()` and component lifecycle dispatch;
- `PhysicsWorld2D` rigid-body integration and immutable query snapshots;
- `NavigationGrid2D` mutations plus deterministic `AStarPathfinder2D` replans;
- `UiCanvas2D` pointer press/release transitions.

The fixed step is 1/64 second, which is exactly representable in binary floating point. The reference
run advances one fixed tick per frame. The comparison run uses a repeating exact-binary frame pattern
that produces the same fixed-tick sequence through zero-, one-, and multi-tick scheduler frames.
Full trace equivalence is required, proving that the tested simulation state depends on fixed ticks
rather than presentation-frame cadence.

## Canonical input and state

Each tick records three opaque input bytes:

1. horizontal movement command;
2. UI action;
3. navigation-cost action.

State hashes use `DeterministicHasher64` and append fields in a fixed documented order. The
integrated scenario includes component state, transform/body state, navigation revision/path data,
UI transition state, physics counters, and deterministic query geometry.

Floating-point values are not hashed from raw memory. Finite values are quantized to signed integers
at 1/4096-unit resolution before hashing. This avoids padding, endianness, NaN-payload, and raw
floating-representation dependencies while still making material simulation drift visible.

## Divergence guarantees

The suite verifies three properties:

- identical canonical input produces an equivalent 8,192-tick trace across the two frame cadences;
- changing one captured input at tick 3,072 reports `ReplayDivergence::Input` at exactly tick 3,072;
- perturbing simulation state without changing captured input at tick 6,144 reports
  `ReplayDivergence::State` at exactly tick 6,144.

These checks exercise first-divergence reporting against real subsystem state rather than only
synthetic hash values.

## Determinism contract

The Phase 11 replay contract is intentionally narrower than claiming that every engine output is
bitwise identical on every machine.

Expected deterministic inputs/state when callers follow stable ordering and canonical encoding:

- fixed-step tick scheduling for the same bounded frame-delta sequence;
- Scene/ECS fixed-update dispatch and component state under deterministic game logic;
- navigation-grid revisions and A* results for the same grid and options;
- UI state transitions for the same canonical pointer sequence;
- physics evolution and query results within the documented canonical quantization contract for the
  same engine build/runtime and fixed-tick inputs;
- replay input bytes, canonical state hashing, and first-divergence comparison.

Explicitly outside the deterministic simulation contract:

- GPU rasterization, driver behavior, render presentation/interpolation, and wall-clock frame timing;
- audio-device scheduling, mixer/device latency, and hardware playback timing;
- profiler wall-clock measurements;
- filesystem or operating-system scheduling and asynchronous external I/O timing;
- raw platform event order before an application converts it into canonical fixed-tick input;
- pointer values, allocator addresses, unordered-container iteration, or raw object memory.

Cross-compiler or cross-platform bitwise floating-point physics equivalence is not claimed by this
suite. A project that requires network lockstep across heterogeneous machines should define a
stronger numeric contract, such as fixed-point simulation, before treating those machines as one
deterministic domain.

## Running the suite

Run only the integrated replay regression:

```sh
ctest --test-dir build -C Debug --output-on-failure -R Lorenzo2DIntegratedReplayTests
```

Or run the complete deterministic partition selection used by normal development:

```sh
ctest --test-dir build -C Debug --output-on-failure -L determinism
```

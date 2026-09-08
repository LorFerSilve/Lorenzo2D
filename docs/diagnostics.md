# Diagnostics and profiling

Phase 11 introduces a bounded, opt-in diagnostics foundation for production investigation and
stress validation.

The first slice provides:

- named timing scopes;
- per-frame aggregation;
- rolling timing windows;
- current, rolling-average, maximum, and total-sample statistics;
- standard engine counters;
- deterministic JSON diagnostic snapshots.

The API is intentionally main-thread-owned. It does not create global state or implicit worker
threads.

## Profiler configuration

`ProfilerConfig` bounds diagnostic work:

- `sampleWindow`: retained samples per scope;
- `maxScopes`: maximum unique scope names;
- `maxNameBytes`: maximum scope-name length.

Invalid configurations are rejected by the constructor. The implementation also imposes hard upper
bounds so diagnostics cannot accidentally become an unbounded telemetry store.

```cpp
l2d::ProfilerConfig config;
config.sampleWindow = 240u;
config.maxScopes = 128u;

l2d::Profiler profiler(config);
```

## Frame aggregation

Call `beginFrame()` once before frame instrumentation and `endFrame()` after it. Multiple records
using the same name within the frame are summed and committed as one rolling sample.

```cpp
profiler.beginFrame();

profiler.record("physics", 0.8);
profiler.record("physics", 0.4);
profiler.record("render", 3.1);

profiler.endFrame();
```

The committed `physics` sample for that frame is 1.2 ms.

Records made while no diagnostic frame is open are committed immediately. This is useful for
standalone operations such as asset imports or save/load measurements.

## Scoped timing

`Profiler::scope()` measures wall-clock duration with `std::chrono::steady_clock`.

```cpp
{
    auto scope = profiler.scope("navigation");
    runNavigation();
}
```

Destroying the scope submits the elapsed duration. When the profiler is disabled the scope does not
take a timestamp or publish a sample.

Profiler scopes are presentation/diagnostic measurements and are not part of Lorenzo2D's
deterministic simulation state.

## Timing statistics

`statistics(name)` exposes:

- current sample;
- rolling average;
- all-time maximum;
- all-time sample count;
- retained sample window.

The raw rolling window is intentionally exposed so tools can calculate percentiles without adding a
fixed percentile policy to the core engine.

## Standard counters

`DiagnosticCounters` currently defines counters for:

- active entities/components;
- colliders and physics queries;
- navigation expansions/replans;
- draw calls/rendered items;
- loaded/live assets;
- active audio voices;
- save bytes written/read.

Counter additions saturate at `uint64_t` maximum instead of wrapping.

This initial slice supplies the shared counter contract. Later Phase 11 slices wire these counters
into the individual engine subsystems and stress scenarios.

## Diagnostic reports

`captureDiagnosticSnapshot()` combines timing statistics, counters, engine version, and diagnostic
frame index.

`DiagnosticReport::toJson()` emits deterministic JSON suitable for logs and bug reports. The
report deliberately excludes wall-clock timestamps so identical diagnostic state serializes
identically.

```cpp
const auto snapshot = l2d::captureDiagnosticSnapshot(profiler, counters);
std::cerr << l2d::DiagnosticReport::toJson(snapshot);
```

The report format has its own `DiagnosticReport::FormatVersion`.

## Deterministic replay foundation

Phase 11 also provides a bounded replay trace for finding the first simulation divergence.

`ReplayTrace` stores strictly increasing tick identifiers, opaque input bytes, and one compact
64-bit state hash per tick. `ReplayTraceConfig` bounds:

- total retained ticks;
- input bytes per tick;
- total retained input bytes.

Failed records leave the existing trace unchanged. Input bytes are intentionally opaque: a game
defines its own canonical encoding and can feed the captured bytes back into its simulation during
replay.

`DeterministicHasher64` provides a stable FNV-1a-based 64-bit hash builder for canonical state
fields. Integer values use an explicit byte order and strings are length-delimited, avoiding
platform-memory-layout and string-concatenation ambiguity.

Do not hash raw object memory, padding, pointer values, container addresses, or unordered iteration.
Append stable IDs and canonical scalar fields in a documented order. For floating-point state,
choose an explicit project contract such as fixed-point/quantized integers or a documented bitwise
representation before hashing.

`compareReplayTraces(expected, actual)` reports whether traces are equivalent and, on failure, the
first divergent tick plus the divergence category: tick sequence, input payload, state hash, or
trace length.

```cpp
l2d::DeterministicHasher64 state;
state.appendString("player");
state.appendInt64(playerXFixed);
state.appendInt64(playerYFixed);

l2d::ReplayTrace trace;
trace.record(tick, encodedInput, state.value());

const auto comparison = l2d::compareReplayTraces(reference, trace);
if (!comparison.equivalent())
{
    // comparison.firstDivergenceTick identifies the earliest mismatch.
}
```

This is the replay primitive, not yet the complete Phase 11 soak harness. Later slices still need to
wire real engine input/state capture into long-running deterministic simulations.

## Threading and ownership

`Profiler` and `DiagnosticCounters` are not internally synchronized. A game or tool should give
each instance one owning thread, normally the main engine thread. Cross-thread aggregation can be
added later with an explicit queue/merge contract rather than imposing locking overhead on every
scope.

## Phase 11 progression

The diagnostics and replay foundations are only the first vertical slices. Phase 11 still requires
subsystem wiring, stress/soak workloads, fuzz/property tests, subsystem-integrated long deterministic
replay, nightly CI, branch protection, and a public-API/failure-path audit before the phase can be
marked implemented.

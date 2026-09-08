# Benchmarking policy and output

Lorenzo2D benchmarks are diagnostic measurements, not unit tests. They make algorithmic changes and
performance trends visible without encoding timing-dependent correctness claims.

## Scenarios

The main `Lorenzo2DBenchmarks` executable measures:

- typed input-action sampling over a representative digital 2D binding;
- scene fixed updates over a populated component set;
- render queue construction and stable legacy z-order sorting;
- a complete 4,096-object projected-depth sort, including footpoint evaluation;
- uniform-grid and brute-force physics steps;
- a batch of free top-down controllers sharing an immutable physics-query snapshot;
- a batch of platformer controllers sharing an immutable physics-query snapshot;
- 32 deterministic A* requests across a weighted 128-by-128 obstacle grid;
- physics-query snapshot construction over 1,024 colliders;
- reusable-context ray batches across the same 1,024 colliders;
- repeated shared-character sweeps and slides through a bounded corridor;
- full legacy and layered tile-map construction;
- tile-map view-culling telemetry.

Phase 9 additionally builds `Lorenzo2DIsometricBenchmarks`. It performs repeated production
isometric projection/inverse-projection, bounded cell picking, placement lookups, and render-view to
visible-tile-region calculations over a 512-by-512 logical map. Its checksum keeps the projection
and culling work observable. This is a focused diagnostic executable rather than an extension of the
persisted JSON/CSV report schema, so existing benchmark consumers remain unchanged.

Phase 10 additionally builds `Lorenzo2DPhase10Benchmarks`. It measures a 128-button screen-space
UI hit-testing/update workload and repeated typed save serialization/deserialization over a
512-entry document. Audio playback timing is intentionally excluded because device scheduling is
not a stable blocking microbenchmark.

Phase 11 diagnostics foundation additionally builds `Lorenzo2DDiagnosticsBenchmarks`. It records
the cost of disabled scoped profiling and bounded per-frame aggregation across the standard
subsystem timing names. These numbers are trend diagnostics only and are not pass/fail thresholds.

## Building and running

```sh
cmake --preset benchmarks
cmake --build --preset benchmarks
./build/benchmarks/benchmarks/Lorenzo2DBenchmarks
./build/benchmarks/benchmarks/Lorenzo2DIsometricBenchmarks
./build/benchmarks/benchmarks/Lorenzo2DPhase10Benchmarks
./build/benchmarks/benchmarks/Lorenzo2DDiagnosticsBenchmarks
```

The main executable always writes its human-readable table to standard output. Optional
machine-readable reports can be written in the same run:

```sh
./build/benchmarks/benchmarks/Lorenzo2DBenchmarks \
  --json build/benchmarks/results.json \
  --csv build/benchmarks/results.csv
```

Parent directories must already exist. An unknown option, a missing path, or an unwritable report is
an error and produces a non-zero exit code.

## Report contract

JSON reports contain:

- `schema_version`;
- `engine_version`;
- one `benchmarks` entry per scenario;
- scenario `name`, `iterations`, `milliseconds`, and `checksum`.

CSV reports contain the same scenario values with `schema_version` and `engine_version` columns on
every row so that standalone rows retain provenance.

Checksums keep measured work observable and make accidental dead-code elimination or empty scenarios
easier to notice. They are diagnostic and are not stable persisted-data identifiers.

## Establishing budgets

Do not choose pass/fail timing thresholds from one machine or one run. To make a scenario blocking:

1. define the game-relevant workload and why it matters;
2. record compiler, build type, hardware class, and repeated-run variance;
3. keep correctness/equivalence tests separate from timing;
4. select a tolerance that accommodates observed CI noise;
5. provide an override or reporting path for new CI hardware;
6. review whether the threshold tests latency, throughput, allocations, or an algorithmic counter.

Algorithmic telemetry such as candidate-pair counts or draw calls is often more stable than wall
clock time and should be preferred when it represents the intended contract.

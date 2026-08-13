# Benchmarking policy and output

Lorenzo2D benchmarks are diagnostic measurements, not unit tests. They make algorithmic changes and
performance trends visible without encoding timing-dependent correctness claims.

## Scenarios

The current executable measures:

- typed input-action sampling over a representative digital 2D binding;
- scene fixed updates over a populated component set;
- render queue construction and stable legacy z-order sorting;
- a complete 4,096-object projected-depth sort, including footpoint evaluation;
- uniform-grid and brute-force physics steps;
- a batch of free top-down controllers sharing an immutable physics-query snapshot;
- a batch of platformer controllers sharing an immutable physics-query snapshot;
- physics-query snapshot construction over 1,024 colliders;
- reusable-context ray batches across the same 1,024 colliders;
- repeated shared-character sweeps and slides through a bounded corridor;
- full legacy and layered tile-map construction;
- tile-map view-culling telemetry.

Navigation and complete isometric scene scenarios must be
added alongside the systems that implement them. The report schema accepts
those scenarios without a format change.

## Building and running

```sh
cmake --preset benchmarks
cmake --build --preset benchmarks
./build/benchmarks/benchmarks/Lorenzo2DBenchmarks
```

The executable always writes its human-readable table to standard output. Optional machine-readable
reports can be written in the same run:

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

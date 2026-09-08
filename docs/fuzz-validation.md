# Fuzz and malformed-input validation

Phase 11.5 adds a deterministic, bounded property/mutation runner for public failure boundaries.
It is not an open-ended random fuzzer in pull-request CI: every run is reproducible from a seed,
scenario name, and case count.

## Pull-request profile

`Lorenzo2DFuzzValidation` is registered as a headless CTest target. Pull requests run:

```sh
Lorenzo2DFuzzValidation --seed 0x4c324446555a5a31 --cases 96
```

The runner caps `--cases` at 4096 per scenario and caps each mutated text buffer at 64 KiB.
There is no unseeded randomness, wall-clock stopping rule, or timing threshold.

## Scenarios

| Scenario | Properties |
| --- | --- |
| `save-json` | malformed/oversized JSON fails transactionally; accepted input survives deterministic save/reload |
| `level-json` | malformed/oversized level JSON and auto-detection preserve the destination; accepted prefabs validate and round-trip structurally |
| `tiled-json` | malformed/oversized Tiled JSON preserves existing tile data; accepted output stays within cell/layer/object bounds |
| `physics-queries` | non-finite coordinates, invalid dimensions, rotations, and capsule geometry fail closed |
| `navigation-grid` | invalid grid dimensions/configuration and traversal costs are rejected without changing the live grid |
| `ui-input` | invalid button bounds/styles are rejected; bounded pointer-event sequences cannot corrupt button identity/state |
| `resource-lookup` | malformed relative names do not mutate roots and relative `..` traversal cannot resolve outside a configured root |

A scenario may accept a mutated document if it is still valid. The property is therefore not
"every mutation must fail"; instead, rejection must be safe and transactional, while acceptance
must satisfy the subsystem's normal validity/round-trip contract.

## Parser workload envelopes

Save JSON already uses `SaveGameLimits::maxTotalBytes`. Phase 11.5 extends the same policy to
the other JSON import boundaries:

- `LevelLoadLimits::maxInputBytes`: 64 MiB by default;
- `TiledJsonImportLimits::maxInputBytes`: 64 MiB by default.

The existing two-argument load APIs remain source-compatible and use these defaults. Additive
overloads accept the limit structures so games can choose smaller budgets. A zero-byte envelope is
invalid. The full input is bounded before JSON construction or legacy level parsing begins.

These byte limits complement, rather than replace, existing semantic limits such as maximum level
object count and `TileMapData` cell/layer/object counts.

## Reproduction and heavier runs

Reproduce a failure exactly with the seed, case count, and scenario printed by the runner:

```sh
./build/tests/Lorenzo2DFuzzValidation \
  --seed 0x4c324446555a5a31 \
  --cases 96 \
  --scenario tiled-json
```

A heavier deterministic pass can be run manually without changing the executable:

```sh
./build/tests/Lorenzo2DFuzzValidation --seed 0x4c324446555a5a31 --cases 2048
```

Phase 11.7 nightly CI reuses this runner with 2,048 cases per scenario across four fixed seeds.
Normal pull requests remain at 96 cases with the default seed, while nightly failures retain
per-seed logs for exact reproduction.

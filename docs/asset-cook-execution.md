# Asset cook cache and bounded execution

Phase 14.4 adds the execution boundary on top of the Phase 14.2 manifest identities and Phase 14.3 dependency graph. It remains a CPU-only authoring/build contract and does not change runtime `AssetManager` ownership.

## `AssetCookCache`

`AssetCookCache` records the last successfully produced build key for each `AssetId`. The build key is derived by `AssetCookExecutor` from the manifest's canonical direct `cookKey`, the normalized cooked-output path, and the build keys of declared dependencies in dependency-first order. A dependency change therefore changes the build keys of its dependents, and an output-path change changes the affected asset's build key even when its direct `cookKey` is unchanged.

This build key is an execution/cache fingerprint, not a second asset identity or dependency system. Source metadata, dependency edges, importer settings, direct cook identity, and cooked paths remain authoritative in `AssetManifest` and `AssetBuildGraph`.

The cache is deterministic, versioned, bounded, and transactionally deserialized: malformed input never replaces the previously published cache state. `record()` accepts only valid bounded `AssetId` values and enforces the project asset-count limit. The 16 MiB persistence envelope is large enough for the maximum valid cache admitted by those limits. Entries for assets no longer present in the current manifest are pruned before execution so project churn cannot accumulate stale cache state indefinitely. A cache record is written only after the cooker callback reports success.

## `AssetCookExecutor`

`AssetCookExecutor::execute` accepts the previous and current manifests, a mutable cook cache, a maximum number of jobs, and a host-provided cooker callback. It obtains dependency-first invalidation from `AssetBuildGraph::computeRebuildOrder`, derives current build keys from the validated graph, skips only invalidated entries whose complete current build key is already recorded as successful, and invokes at most `maxJobs` callbacks per call.

Successful jobs are recorded immediately, so a later invocation resumes from durable successful state. Jobs beyond the bound are returned in `remaining`. A failed callback stops execution and reports the asset-specific error; the failed asset is not recorded as successful and the caller's result object is not replaced. Successful jobs completed before that failure remain cached for a subsequent retry.

The host owns filesystem I/O and importer/cooker implementation. The callback receives the canonical `AssetCookRequest` plus the manifest's project-relative cooked path. This keeps the engine contract testable without introducing hidden worker threads, process launching, or a second asset identity system.

## Incremental workflow

1. Build and validate the current `AssetManifest` and `AssetBuildGraph`.
2. Load the previous successful manifest and `AssetCookCache`.
3. Call `AssetCookExecutor::execute` with a bounded job budget.
4. Persist the cache after successful work.
5. Repeat while `remaining` is non-empty.
6. Publish the new manifest as the previous successful manifest only when the desired cook transaction has completed.

Installed-package regression coverage exercises dependency-first bounded execution, resume behavior, transitive dependency invalidation against an old cache, cooked-output-path invalidation, stale-cache pruning, bounded/validated cache mutation, deterministic cache round-tripping, transactional cache rejection, callback failure semantics, and reuse of canonical manifest cook identities.

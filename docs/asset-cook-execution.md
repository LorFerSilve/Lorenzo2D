# Asset cook cache and bounded execution

Phase 14.4 adds the execution boundary on top of the Phase 14.2 manifest identities and Phase 14.3 dependency graph. It remains a CPU-only authoring/build contract and does not change runtime `AssetManager` ownership.

## `AssetCookCache`

`AssetCookCache` records only the last successfully produced `cookKey` for each `AssetId`. The cache is deterministic, versioned, bounded, and transactionally deserialized: malformed input never replaces the previously published cache state. A cache record is written only after the cooker callback reports success.

The cache deliberately does not duplicate source metadata, dependency edges, importer settings, or cooked paths. Those remain authoritative in `AssetManifest`.

## `AssetCookExecutor`

`AssetCookExecutor::execute` accepts the previous and current manifests, a mutable cook cache, a maximum number of jobs, and a host-provided cooker callback. It obtains dependency-first invalidation from `AssetBuildGraph::computeRebuildOrder`, skips entries whose exact cook identity is already recorded as successful, and invokes at most `maxJobs` callbacks per call.

Successful jobs are recorded immediately, so a later invocation resumes from durable successful state. Jobs beyond the bound are returned in `remaining`. A failed callback stops execution and reports the asset-specific error; the failed asset is not recorded as successful and the caller's result object is not replaced.

The host owns filesystem I/O and importer/cooker implementation. The callback receives the canonical `AssetCookRequest` plus the manifest's project-relative cooked path. This keeps the engine contract testable without introducing hidden worker threads, process launching, or a second asset identity system.

## Incremental workflow

1. Build and validate the current `AssetManifest` and `AssetBuildGraph`.
2. Load the previous successful manifest and `AssetCookCache`.
3. Call `AssetCookExecutor::execute` with a bounded job budget.
4. Persist the cache after successful work.
5. Repeat while `remaining` is non-empty.
6. Publish the new manifest as the previous successful manifest only when the desired cook transaction has completed.

Installed-package regression coverage exercises dependency-first bounded execution, resume behavior, deterministic cache round-tripping, transactional cache rejection, callback failure semantics, and reuse of canonical manifest cook identities.

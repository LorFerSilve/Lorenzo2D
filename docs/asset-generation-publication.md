# Transactional content-generation publication

Phase 14.8 adds a publication boundary between authoring/cooking and runtime/editor consumption.

Earlier Phase 14 slices already provide deterministic manifests, dependency-aware rebuild planning,
incremental cook caching, concrete file/atlas cookers, and manifest-backed runtime lookup. The
remaining failure mode was publication: a background rebuild that writes directly into the active
content root can leave the manifest and its artifacts temporarily out of sync.

`AssetGenerationPublisher` solves that without replacing the existing cook pipeline.

## Generation model

Each published generation is an immutable directory under one publication root:

```text
content/
├── generation-0123456789abcdef/
│   ├── .l2d-generation-manifest
│   └── cooked/...
└── generation-fedcba9876543210/
    ├── .l2d-generation-manifest
    └── cooked/...
```

A generation ID is derived deterministically from the canonical serialized `AssetManifest`.
Different manifest identities therefore publish under different names, while a generation that has
already been published is never overwritten in place.

Work in progress uses a private sibling directory:

```text
content/.l2d-stage-generation-0123456789abcdef/
```

The staging directory is not a runtime root. Runtime/editor consumers continue to use the previous
published generation until the next generation is complete and explicitly selected.

## Preparing an incremental rebuild

`prepare(current, previous, staging)` optionally seeds the new staging tree from a previous
published generation.

The previous generation is copied first. `AssetBuildGraph::computeRebuildOrder()` is then used to
remove the primary cooked output of every changed asset and every transitively invalidated
dependent. Unchanged artifacts remain available in staging, so the existing `AssetCookCache` and
`AssetCookExecutor` can continue to skip cache-stable work.

Typical flow:

```cpp
l2d::AssetGenerationPublisher publisher("build/content-generations");

l2d::AssetGenerationStaging staging;
if (!publisher.prepare(nextManifest, &currentGeneration, staging, &error))
{
    return;
}

l2d::AssetFileCooker fileCooker(staging.root);
l2d::AssetCookExecutionResult result;
if (!l2d::AssetCookExecutor::execute(
        currentGeneration.manifest,
        staging.manifest,
        cache,
        maxJobs,
        fileCooker.cookFunction(),
        result,
        &error))
{
    return;
}
```

Generated cookers use the same staging root. For example, an atlas cooker can publish its image and
`.regions` sidecar inside the staging generation while the previous published generation remains
untouched.

## Publication gate

`publish()` is transactional with respect to visibility.

Before the staging directory is exposed it:

1. revalidates the manifest and dependency graph;
2. verifies that the staging identity matches the manifest;
3. verifies that the staging directory belongs to the publisher;
4. resolves every manifest `cookedPath`;
5. requires every required artifact to be a regular file confined to the staging root;
6. rewrites the canonical generation manifest metadata; and
7. renames the complete staging directory to its immutable generation name.

If any required artifact is missing, publication fails and the staging tree remains private.
The caller-provided `AssetContentGeneration` output is unchanged.

The final rename happens inside the same publication directory. Existing generations are not renamed,
deleted, or modified as part of publishing the new one.

## Runtime/editor handoff

Publication does not implicitly mutate runtime state. This separation is deliberate.

`AssetContentGeneration::configure()` builds a candidate `AssetManifestResourceLocator` from the
published manifest and generation root, then replaces the caller's locator only after configuration
succeeds:

```cpp
l2d::AssetManifestResourceLocator runtimeResources;

// Old generation remains active while background cooking runs.

l2d::AssetContentGeneration nextGeneration;
if (publisher.publish(staging, nextGeneration, &error))
{
    nextGeneration.configure(runtimeResources, &error);
}
```

Already loaded `AssetManager` resources retain the Phase 14.7 snapshot/live-handle behavior. The
locator swap affects future lookups; coordinated reload policy remains an application/editor
decision.

## Failure and safety properties

- Invalid or cyclic manifests are rejected before a staging root is created.
- Missing dependencies use the existing `AssetBuildGraph` fail-closed behavior.
- The publisher reserves `.l2d-generation-manifest` for generation metadata.
- Required cooked paths must resolve inside the generation root.
- A previous generation must belong to the same publisher before it can seed a rebuild.
- Symlinks in a copied previous generation are rejected rather than reproduced into staging.
- An incomplete generation is never added as a runtime resource root.
- A successfully published generation is immutable from the publisher's point of view.

The publisher intentionally does not delete old generations. Retention/garbage collection needs
policy about editor rollback, live handles, packaging and deployment, so it remains outside this
slice rather than guessing a lifetime rule.

## Existing workflows remain valid

Projects that do not need generated content sets can continue using `ResourceLocator`,
`AssetManifestResourceLocator`, or direct `AssetManager` paths exactly as before.

Phase 14.8 is an optional orchestration layer around the existing Phase 14 contracts, not a
replacement content system.

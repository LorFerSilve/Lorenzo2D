# Asset build graph and rebuild invalidation

Phase 14.3 adds a CPU-only whole-project dependency graph for authoring and cook workflows. It builds on the stable `AssetId`, `AssetManifest`, and cook-key contracts introduced in Phases 14.1 and 14.2. It does not alter runtime `AssetManager` ownership, live resource handles, or the older polling-oriented `AssetPipeline::AssetDependencyGraph` used by runtime hot reload.

## Whole-project validation

`AssetBuildGraph::build()` consumes an `AssetManifest` transactionally. A candidate graph is accepted only when every declared dependency resolves to another manifest entry and the complete project graph is acyclic. Failed builds preserve the previously published graph.

The graph stores deterministic dependency and dependent lists keyed by canonical `AssetId`. `topologicalOrder()` publishes a stable dependency-first order, using lexical `AssetId` ordering whenever multiple nodes are ready at the same time. `dependenciesOf()` and `dependentsOf()` provide direct or transitive graph queries without introducing a second asset identity system.

```cpp
l2d::AssetBuildGraph graph;
std::string error;
if (!graph.build(manifest, &error))
{
    // Missing dependency or cycle: keep the previous graph/build state.
}
```

## Rebuild invalidation

`AssetBuildGraph::computeRebuildOrder(previous, current, output)` compares two complete manifests. A current asset is initially invalidated when it is new, its canonical cook key changed, or its cooked output path changed. Every current transitive dependent is then invalidated as well because a dependency's cooked result can change without changing the dependent's own direct cook inputs.

The resulting vector is deterministic and dependency-first, so callers can execute or enqueue cooks without rebuilding a dependent before an invalidated prerequisite. Removed assets are not returned because they no longer have a current cook target; any surviving asset that removes or changes a dependency receives a different cook key and is therefore invalidated normally.

Both manifests must form complete acyclic project graphs. Failure leaves the caller-provided output unchanged.

```cpp
std::vector<l2d::AssetId> rebuildOrder;
if (!l2d::AssetBuildGraph::computeRebuildOrder(lastSuccessfulManifest,
                                               currentManifest,
                                               rebuildOrder,
                                               &error))
{
    // Do not schedule a partial build from an invalid project graph.
}
```

## Determinism and limits

The graph inherits the manifest's bounded asset/dependency limits. Construction and rebuild planning use ordered containers, producing stable validation and rebuild ordering independent of insertion order. Rebuild invalidation is intentionally based on the Phase 14.2 cook identity rather than timestamps or machine-specific absolute paths.

## Scope boundary

Phase 14.3 decides **what is structurally valid and what must be rebuilt**. It does not execute importers, persist a rebuild cache, hash source files, watch the filesystem, generate atlases, preprocess audio, or mutate editor documents. Those remain later Phase 14 responsibilities.

Installed-package validation exercises graph construction, completeness rejection, cycle rejection, transactional preservation, transitive dependent discovery, deterministic topological ordering, and transitive rebuild invalidation through `find_package(Lorenzo2D)`.

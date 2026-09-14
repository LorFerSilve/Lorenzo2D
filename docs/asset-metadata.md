# Asset metadata foundation

Phase 14.1 introduces the authoring-side identity and source-description contract that later content cooking, caching, manifests, and editor integration build on.

## Stable identity

`AssetId` remains the existing serialized string type so Phase 14 does not break runtime or scene serialization contracts. `AssetMetadataRegistry::isValidAssetId()` narrows IDs used by the new metadata layer to portable ASCII identifiers containing letters, digits, `_`, `-`, `.`, `/`, or `:`. IDs are logical identities and must not be derived from absolute machine paths.

Runtime `AssetManager` names and live handles remain separate concerns. The metadata layer does not load GPU/audio resources and does not make runtime systems depend on authoring state.

## Source descriptors

`AssetSourceDescriptor` records:

- stable `AssetId`;
- explicit `AssetSourceKind`;
- project-relative source path;
- importer identifier;
- deterministic key/value import settings;
- declared asset dependencies.

Source paths must be relative to a project/content root and may not escape it through `..`. Import settings use `std::map` so iteration is deterministic. Dependencies are validated for syntax, duplicates, and self-dependencies, then stored in sorted order.

The registry intentionally does not require dependencies to already exist. That permits metadata discovery/import in arbitrary order; graph completeness and cycle validation belong to the later Phase 14 dependency/build-graph slice.

## Transactional registry updates

`AssetMetadataRegistry::upsert()` validates a complete descriptor before modifying registry state. Invalid replacements therefore leave the previous descriptor intact. Successful writes normalize the source path and dependency order. `descriptors()` returns assets in stable `AssetId` order, providing deterministic input for the later manifest/cache stages.

Hard limits bound identifier length, importer length, settings, dependencies, and registry size. These limits keep malformed or generated authoring input from creating unbounded metadata structures.

## Architecture boundary

Phase 14.1 is CPU-only and header-only. It deliberately does **not** add cooked output, persistent manifests, cache keys, atlas generation, audio preprocessing, file watching, or editor mutations. Existing `AssetPipeline` hot reload and `AssetDependencyGraph` behavior remains unchanged. Later Phase 14 slices should consume the metadata contract instead of introducing a second identity system.

## Validation

The registered headless resource regression covers stable-ID/path validation, deterministic ordering/canonicalization, dependency validation, and transactional rejection of invalid replacements. A separate installed-package consumer compiles and executes the public metadata API through `find_package(Lorenzo2D)`, proving that the contract is exported with the normal SDK headers.

# Deterministic asset manifests and cook identity

Phase 14.2 builds deterministic persistence and import/cook identity on the Phase 14.1 `AssetSourceDescriptor` model. It remains a CPU-only authoring/content-build contract and does not change runtime `AssetManager` ownership, live handles, or resource-loading behavior.

## Cook request contract

`AssetManifest::makeCookRequest()` validates and canonicalizes a source descriptor, records a caller-supplied source-content hash and importer version, and derives a stable 64-bit cook key. The key covers the stable `AssetId`, source kind, normalized project-relative path, importer identifier/version, source-content hash, sorted import settings, and sorted declared dependencies.

Equivalent descriptors therefore produce the same cook key regardless of dependency insertion order or lexical `.` path components. Changing source bytes, importer version, settings, identity, or another canonical cook input changes the key. The key is a deterministic rebuild/cache identity, not a cryptographic authenticity primitive.

## Manifest contract

`AssetManifestEntry` records the canonical source descriptor, source-content hash, importer version, cook key, and project-relative cooked output path. Entries are stored in stable `AssetId` order. `upsert()` validates the entire replacement before publishing it, including recomputing the canonical cook key, so rejected replacements preserve prior state.

The persisted manifest uses a versioned length-prefixed format rather than exposing Lorenzo2D's private JSON dependency through an installed public header. Length prefixes make arbitrary setting values unambiguous while deterministic map/asset ordering keeps byte output stable. Deserialization is transactional: malformed input, duplicate IDs/settings, invalid paths, tampered cook keys, unsupported versions, limit violations, and trailing data leave the previous manifest unchanged.

## Bounds and portability

Manifest input is capped at 16 MiB and the Phase 14.1 registry limits continue to bound assets, settings, and dependencies. Source and cooked paths are project-relative, reject `..`, and are additionally capped at 1024 portable characters. Importer versions are bounded to 64 characters.

Absolute machine paths and runtime handles are deliberately excluded from persisted identity. A build tool resolves source/cooked paths against its own project/build roots.

## Architecture boundary

This slice defines deterministic manifest persistence and the handoff from source metadata to an importer/cooker. It does **not** execute importers, validate whole-project dependency completeness/cycles, schedule rebuilds, generate atlases, preprocess audio, watch files, or mutate editor documents. Those remain later Phase 14 slices.

The existing runtime `AssetPipeline` and `AssetDependencyGraph` remain unchanged. Later dependency-graph and rebuild-cache work should consume `AssetManifest` and its cook keys instead of creating parallel content identities.

## Validation

Headless resource regressions cover canonical cook identity, sensitivity to cook inputs, stable manifest ordering, byte-for-byte round trips, tamper rejection, malformed/trailing-data rejection, and transactional preservation. The installed-package asset-metadata consumer also compiles and executes `AssetManifest` through `find_package(Lorenzo2D)`.

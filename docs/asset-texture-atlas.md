# Deterministic texture-atlas generation

Phase 14.6 adds deterministic texture-atlas generation on top of the Phase 14 manifest, dependency
graph, cook cache, and transactional artifact-publication contracts.

The public entry point is `AssetTextureAtlasCooker`. Atlas assets remain ordinary
`AssetSourceKind::Texture` manifest entries and use importer ID `l2d.texture.atlas`; no parallel
asset identity or cache system is introduced.

## Source contract

An atlas is generated from existing texture entries in an `AssetManifest`.

`makeManifestEntry()`:

1. validates and canonicalizes the atlas `AssetId`;
2. sorts and de-duplicates source texture IDs;
3. requires every dependency to exist in the supplied source manifest and be a texture;
4. folds dependency cook keys and cooked paths into a canonical aggregate source hash;
5. records `max_dimension` and `padding` as import settings;
6. creates the standard `AssetCookRequest` / `AssetManifestEntry` identity; and
7. requires the atlas output to be a project-relative `.png` path.

Because atlas dependencies are also recorded in `AssetSourceDescriptor::dependencies`, the existing
`AssetBuildGraph` and `AssetCookExecutor` provide transitive invalidation and cache stability.

## Deterministic packing

Packing is intentionally simple and reproducible:

- source IDs are processed in lexicographic order;
- images are never rotated;
- a bounded shelf layout is used;
- padding and maximum atlas dimension are explicit cook settings;
- atlas dimensions are the tight bounds of placed source rectangles; and
- transparent pixels occupy padding/unwritten space.

The first implementation prioritizes stable output and inspectable behavior over packing density.
Alternative packing algorithms can be introduced later only by changing the importer version or
other canonical cook inputs.

## Region metadata

Every published atlas PNG has a sidecar at:

```
<atlas>.png.regions
```

The sidecar is deterministic text with a versioned signature, atlas size, and one region per source
asset. `TextureAtlasMetadata` validates and round-trips this format and provides lookup by source
`AssetId`.

Region records contain:

- source asset ID;
- x/y atlas origin;
- width; and
- height.

The public metadata format is bounded to 256 regions and 1 MiB of serialized text.

## Transactional publication

The atlas image and region sidecar are treated as one generation.

Before publication, the cooker:

1. decodes every cooked texture dependency;
2. generates the atlas pixels and deterministic region metadata;
3. stages the PNG using a temporary filename that preserves the `.png` extension;
4. stages the region sidecar;
5. reloads and validates both staged artifacts; and
6. publishes both files with backup/restore handling.

If publication fails, previously published image/metadata files are restored where present.
Interrupted backups are recovered on the next cook before new work begins. The cook cache is updated
only after the complete callback succeeds.

## Bounds and filesystem safety

The current limits are:

- 256 source textures per atlas;
- 8,192 pixels maximum configured atlas dimension;
- 32 pixels maximum padding; and
- project-relative output paths only.

Dependency paths are resolved beneath the configured project root. Output directory components may
not be symlinks or non-directories, and existing final destinations must be regular files.

## Integration example

```cpp
l2d::AssetTextureAtlasCooker atlasCooker(projectRoot, sourceManifest, 4096u, 1u);

l2d::AssetManifestEntry atlasEntry;
atlasCooker.makeManifestEntry(
    "atlases/characters",
    {"textures/player", "textures/enemy"},
    "cooked/characters.png",
    atlasEntry,
    &error);

l2d::AssetManifest next = sourceManifest;
next.upsert(atlasEntry, &error);

l2d::AssetCookExecutionResult result;
l2d::AssetCookExecutor::execute(
    sourceManifest,
    next,
    cache,
    1u,
    atlasCooker.cookFunction(),
    result,
    &error);
```

## Scope boundary

Phase 14.6 does not change runtime `AssetManager` lookup or require games to consume atlases.
Runtime manifest-backed lookup/editor integration and optional preprocessing remain later Phase 14
work. The existing simple resource-root workflow remains unchanged.

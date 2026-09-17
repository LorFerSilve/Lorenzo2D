# Concrete file cooking and cooked-artifact publication

Phase 14.5 connects real source files to the Phase 14.4 bounded execution contract through the public
`AssetFileCooker`. It remains an authoring/build API: runtime `AssetManager` ownership and the existing
simple resource-root workflow are unchanged.

## Built-in importers

The initial built-in importers are deterministic passthrough cookers. They preserve runtime-native
bytes and therefore do not introduce a second cooked container format.

| Importer ID | Required kind | Supported source/output extensions |
| --- | --- | --- |
| `l2d.texture.copy` | `Texture` | `.png`, `.jpg`, `.jpeg`, `.bmp` |
| `l2d.font.copy` | `Font` | `.ttf`, `.otf` |
| `l2d.sound.copy` | `Sound` | `.wav`, `.ogg`, `.flac` |
| `l2d.shader.copy` | `Shader` | `.vert`, `.frag`, `.glsl` |

All built-in importers currently report importer version `1`. The source and cooked extensions must
match because the produced artifact is intentionally the validated runtime-native file.

Format validation is bounded and deterministic. Binary formats are checked against their declared
container signature/header, while shader sources reject NUL/control bytes and whitespace-only input.
The 64 MiB per-source byte envelope prevents malformed metadata from causing unbounded reads.

## Manifest integration

`AssetFileCooker::makeManifestEntry` is the preferred entry point for built-in file importers. It:

1. validates the `AssetSourceDescriptor` and importer/kind/extension combination;
2. resolves the source beneath the configured project root;
3. reads and validates the source bytes;
4. computes the canonical FNV-1a source-content hash;
5. uses `AssetManifest::makeCookRequest` with the built-in importer version; and
6. produces a validated `AssetManifestEntry` for the requested project-relative cooked path.

The cooker does not create a parallel identity system. `AssetId`, dependencies, settings, source
paths, importer names, importer versions, and direct cook identity remain authoritative in
`AssetMetadataRegistry` and `AssetManifest`.

## Bounded execution

`AssetFileCooker::cookFunction()` returns an `AssetCookExecutor::CookFunction`, so concrete file I/O
is scheduled exclusively by the Phase 14.4 rebuild/cache layer:

```cpp
l2d::AssetFileCooker fileCooker(projectRoot);

l2d::AssetManifestEntry entry;
fileCooker.makeManifestEntry(source, "cooked/player.png", entry, &error);
current.upsert(entry, &error);

l2d::AssetCookExecutionResult result;
l2d::AssetCookExecutor::execute(previous, current, cache, 8u,
                                fileCooker.cookFunction(), result, &error);
```

Before cooking, the callback rebuilds the canonical request and verifies that the current source
bytes still hash to `AssetCookRequest::sourceContentHash`. A source modified after manifest creation
therefore fails the cook instead of publishing bytes under a stale cache identity.

## Artifact publication

A successful cook follows a staged publication sequence:

1. write to `<cooked-path>.l2d-tmp`;
2. reread the staged artifact;
3. re-run format validation and verify its content hash;
4. move an existing published artifact to `<cooked-path>.l2d-backup`;
5. rename the validated staged artifact into place; and
6. restore the backup if publication fails.

An interrupted replacement is recovered on the next cook: if a backup exists without the published
artifact, the backup is restored before new work begins. Stale temporary files are discarded.
Successful cache state is still recorded only by `AssetCookExecutor` after the callback returns
success.

## Path and filesystem safety

Source and cooked paths remain project-relative manifest paths. Source resolution canonicalizes the
configured project root and rejects files that resolve outside it. Cooked publication creates path
components one at a time and rejects existing symlink components or a symlink destination, avoiding
accidental publication outside the project tree through manifest-controlled paths.

This API is synchronous by design. Job budgeting, dependency ordering, resume behavior, and future
background scheduling remain responsibilities of `AssetCookExecutor` and later Phase 14 slices.

## Scope boundary

Phase 14.5 deliberately does not generate atlases, transcode audio, publish multiple assets as one
generation, or change runtime resource lookup. Those concerns build on this concrete artifact
boundary rather than being folded into importer identity or the cook cache.

# Manifest-backed runtime resource lookup

Phase 14.7 connects the deterministic cooked-content manifest to runtime resource loading without
making runtime ownership depend on editor or cooker state.

The public boundary is `AssetManifestResourceLocator`. It owns an `AssetManifest` snapshot and
uses the existing `ResourceLocator` containment/ordered-root rules to map stable `AssetId` values
to cooked files.

## Runtime lookup contract

A successful lookup:

1. validates the requested `AssetId`;
2. finds the corresponding manifest entry;
3. revalidates its canonical manifest identity;
4. optionally requires an exact `AssetSourceKind`;
5. resolves the entry's project-relative `cookedPath` through configured runtime roots;
6. requires the resolved artifact to be a regular file; and
7. publishes `AssetManifestResource` only after all checks succeed.

Failure is transactional: the caller-provided output object is not modified.

The resolved snapshot exposes the stable asset ID, source kind, cooked path, and canonical cook key.
The resolver does not expose source-file loading as a fallback. A manifest entry always resolves to
its cooked artifact.

## Root policy and filesystem safety

Runtime roots retain the existing `ResourceLocator` semantics:

- roots are normalized absolute paths;
- duplicate roots are ignored;
- lookup uses insertion-order precedence;
- manifest paths cannot escape a configured root with `..`;
- canonicalized symlink targets outside a root are rejected; and
- missing artifacts fail explicitly.

The manifest adapter additionally caps configured roots at 64 so manifest-driven lookup cannot grow
an unbounded search list.

Multiple roots allow packaged content, an override directory, or another deployment layout to share
one logical manifest without embedding machine-specific absolute paths.

## AssetManager integration

`AssetManager` keeps all existing path- and `ResourceLocator`-based overloads unchanged. Phase
14.7 adds manifest overloads for the runtime types already owned by the manager:

```cpp
l2d::AssetManifestResourceLocator resources(manifest);
resources.addRoot(contentRoot, &error);

l2d::AssetManager assets;
assets.loadTexture(resources, "textures/player");
assets.loadFont(resources, "fonts/ui");
assets.loadSoundBuffer(resources, "sounds/jump");
```

These overloads require the manifest kind to match the requested runtime type and store the loaded
resource under the same stable `AssetId`. Existing snapshot/live-handle semantics are unchanged.
A failed resolve or decode does not replace an already loaded generation.

Other manifest kinds, such as shaders or levels, can still be resolved with
`AssetManifestResourceLocator::resolve()` and consumed by their owning subsystem.

## Manifest lifetime and replacement

The locator owns its manifest by value. Callers may therefore deserialize/build a candidate manifest
and then publish it with `setManifest()` without creating a borrowed-lifetime dependency.

Replacing the manifest affects future lookup only. It does not unload, mutate, or silently reload
resources already owned by `AssetManager`. Background generation swaps and coordinated reload are
separate orchestration concerns.

## Existing simple workflow

The original workflow remains valid and source-compatible:

```cpp
l2d::ResourceLocator resources;
resources.addRoot("assets");
assets.loadTexture("player", resources, "textures/player.png");
```

Games are not required to adopt the Phase 14 content pipeline.

## Scope boundary

Phase 14.7 does not watch manifests, schedule background cooks, atomically publish a complete
multi-asset generation, or make the editor depend on runtime loading state. The next content-pipeline
slice should establish a transactional generation-publication boundary so background rebuilds can
be prepared without exposing a partially updated manifest/artifact set.

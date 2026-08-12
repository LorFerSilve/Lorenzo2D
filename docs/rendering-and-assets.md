# Rendering and asset pipeline

Lorenzo2D 0.7 separates logical world coordinates from presentation. The
render-context and ordering APIs are additive: the previous interpolation-alpha
and z-order calls remain source-compatible adapters.

## Render contexts and passes

`RenderContext2D` travels through `SceneManager`, `Scene`, `GameObject`, and
`Component`. It carries the interpolation alpha, an optional
`CoordinateProjection2D`, and one `RenderPass2D`. The built-in passes are
`World`, `PhysicsDebug`, and `UI`. Integer layers are deliberately separate and
belong to ordering inside a pass.

World renderers call `worldToRender()` without changing `Transform`. Shape and
sprite transforms project their anchor positions; particle, tilemap, and
physics-debug geometry projects each submitted vertex. UI contexts ignore the
projection so screen-space positions cannot accidentally be skewed. Input or
picking code can use `renderToWorld()` on the same context.
`OrthogonalProjection2D` is the identity adapter used by existing games.

```cpp
l2d::OrthogonalProjection2D projection;
l2d::RenderContext2D context{
    interpolationAlpha,
    &projection,
    l2d::RenderPass2D::World
};

scene.render(window, context);
```

A custom projection implements `worldToRender`, `renderToWorld`, and
`depthFor`. Physics, navigation, and gameplay continue to read Cartesian world
coordinates. Non-identity tilemap projections transform submitted vertices and
currently disable chunk culling conservatively; projected-bounds caching is a
later optimization.

## Layers and deterministic depth

`RenderOrder2D` selects a pass, signed integer layer, depth mode, and optional
fine order. `RenderQueue2D` compares the complete `RenderSortKey2D`
lexicographically:

1. `layer`;
2. `depth`;
3. `order`;
4. scene `insertionOrder`.

The available depth modes are `Fixed`, `Explicit`, `WorldY`, and `ProjectedY`.
Fixed and explicit modes use the configured explicit depth; the two names keep
the intent visible for fixed scenery versus author-controlled ordering.
World/projected Y modes use the sprite's interpolated bottom-centre footpoint;
non-sprite objects fall back to the interpolated Transform position. A local
footpoint override is available for shape renderers or unusual art.

Nonfinite explicit depths are rejected transactionally. Nonfinite values that
reach a sort key are treated as zero, keeping the comparator total and
deterministic.

```cpp
auto& order = actor.addComponent<l2d::RenderOrder2D>(
    l2d::RenderDepthMode2D::ProjectedY);
order.setLayer(10);
order.setLocalFootPoint({16.f, 32.f});
```

## z-order compatibility

`GameObject::setZOrder()` controls scene presentation. Smaller values render
first; equal values retain scene insertion order. Without an active
`RenderOrder2D`, it maps to the new key's `order` with layer and depth zero, so
old scenes render identically. With `RenderOrder2D`, z-order remains the fine
order unless `setOrder()` overrides it. Level format versions 2 and 3 persist
the value as `z_order`; version 1 levels still load with z-order zero.

## Sprite anchors and flips

`SpriteRenderer` supports custom origins and the `TopLeft`, `Center`, and
`BottomCenter` presets. Presets are reapplied when a texture rectangle changes.
`setFlippedX()` and `setFlippedY()` mirror presentation independently from the
owner Transform scale, so colliders and gameplay transforms are unaffected.
The default remains top-left to preserve existing visuals.

## Incremental and streamed tilemaps

`TileMap::setTile(row, column, value)` edits an existing cell. Its vertex data
is rebuilt only for the containing render chunk. If the edit changes whether
the cell equals the solid character, collision rectangles are recomputed and
published transactionally so exact merged collision coverage remains intact.
`lastUpdateStats()` makes that cost visible.

`setStreamRegion()` accepts a tile-coordinate rectangle. Chunks outside it are
non-resident and skipped before camera culling. `TileMapRenderStats` separates
resident and non-resident chunks. `clearStreamRegion()` restores the complete
map.

Animated atlas entries are configured with `TileSet::setAnimatedTile()`. Every
frame has an atlas rectangle and a positive duration. Tilemap updates change
only the six texture coordinates for each animated tile; vertex positions and
collision geometry stay untouched.

## Live assets and background loading

Ordinary `TextureHandle` and `FontHandle` values remain immutable leases.
`AssetManager::liveTexture()` and `liveFont()` opt into a named registry slot;
`snapshot()` returns a lifetime-safe lease to its current generation.
`SpriteRenderer::setLiveTexture()` and `DebugOverlay::setLiveFont()` refresh
those bindings during rendering and keep their last valid resource if a slot
is temporarily empty.

`AssetPipeline::requestTexture()` decodes an image on a worker thread. Call
`poll()` from the thread that owns the SFML graphics context to construct and
publish completed GPU textures. `watchTexture()` plus `scanForChanges()` adds
polling-based hot reload. The dependency graph rejects cycles and each reload
event reports transitively invalidated dependents.

## Particles and post-processing

`ParticleEmitter2D` is a deterministic component with continuous emission and
bursts, bounded capacity, lifetime/speed ranges, direction spread, gravity,
and color/size interpolation. A fixed seed makes effects reproducible in tests.

`PostProcessStack2D` applies ordered alpha, additive, or multiply color passes
in screen coordinates after scene rendering and restores the previous view.
It is intentionally a lightweight color-grading/fade layer, not an off-screen
shader graph.

See `Lorenzo2DPhase4Example` for the systems working together.

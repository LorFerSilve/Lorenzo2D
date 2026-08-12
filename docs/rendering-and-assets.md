# Rendering and asset pipeline

Phase 4 adds explicit presentation ordering, editable/streamed tilemaps, live
asset bindings, particles, and lightweight screen-space effects while keeping
the previous APIs source-compatible.

## Stable z-order

`GameObject::setZOrder()` controls scene presentation. Smaller values render
first; equal values retain scene insertion order. `RenderQueue2D::build()`
creates a stable per-pass snapshot and excludes inactive or destroy-queued
objects. Level format versions 2 and 3 persist the value as `z_order`; version 1
levels still load with z-order zero.

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

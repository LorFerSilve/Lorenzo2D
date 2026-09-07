# Isometric projection, picking, placement, and culling

Lorenzo2D 0.13 adds the Phase 9 isometric presentation layer while preserving a Cartesian logical
world. Physics, navigation, transforms, collision geometry, and tile coordinates stay in ordinary
world space; only presentation, pointer picking, placement previews, depth ordering, and projected
visibility use the isometric transform.

## Projection contract

`IsometricProjection2D` implements the existing `CoordinateProjection2D` interface. It accepts a
logical world-cell size, a projected diamond-cell size, and an optional render-space origin. For a
logical cell coordinate `(column, row)`, the projection is:

```text
renderX = originX + (column - row) * renderCellWidth  / 2
renderY = originY + (column + row) * renderCellHeight / 2
```

The inverse transform is analytical, so `renderToWorld(worldToRender(p))` round-trips finite
positions within normal floating-point precision. `depthFor()` returns projected Y, which means
`RenderDepthMode2D::ProjectedY` sorts actors by their projected foot point without changing their
world transforms.

`CoordinateProjection2D::projectBounds()` is a conservative optional contract. The isometric
implementation projects all four corners of an axis-aligned world rectangle and returns the
smallest render-space axis-aligned rectangle containing them. Projections that cannot provide a safe
bound may return `false`; callers must then avoid aggressive culling.

## Tile picking and placement

`IsometricTileGrid2D` bridges the projected view back to the Cartesian `TileMapData` grid. Construct
it from validated isometric `TileMapData` plus the desired projected tile size:

```cpp
l2d::IsometricTileGrid2D grid(data, {64.f, 32.f}, {640.f, 64.f});
```

Use `pick(renderPosition)` to map a render-space pointer position to a bounded `TileMapCell`.
`worldPosition(cell)` and `renderPosition(cell)` provide matching placement anchors, either centered
in the cell or at the cell origin. Picking outside the grid returns `std::nullopt` instead of
clamping to an unrelated cell.

Because the grid uses the same logical cell size as `TileMapData`, navigation can continue to use
`NavigationGrid2D` and `AStarPathfinder2D` without an isometric-specific pathfinding implementation.

## Projected-view culling

A non-identity projection can rotate or shear a rectangular world-space chunk, so applying the old
orthogonal world-space camera test to projected geometry would be incorrect. Phase 9 therefore uses
an explicit projected-view workflow:

1. `IsometricTileGrid2D::visibleRegionForView()` inverse-projects the four corners of the current
   `sf::View` and derives a conservative Cartesian tile region.
2. Optional cell padding accounts for sprites or decorations that extend beyond their logical foot
   cells.
3. Pass the returned `TileMapRegion` to `TileMap::setStreamRegion()`.
4. The existing tile renderer rejects non-resident chunks before submission while still projecting
   the vertices of resident chunks through `RenderContext2D`.

This keeps culling conservative and deterministic. The tile renderer intentionally continues to
avoid its orthogonal per-chunk camera test for arbitrary non-identity projections; the Phase 9
visible-region calculation is the supported projected culling path.

## Depth ordering

Attach `RenderOrder2D` with `RenderDepthMode2D::ProjectedY` to actors that must interleave by their
feet in an isometric scene. `RenderQueue2D` evaluates the configured foot point through the active
projection and sorts by the resulting render-space Y value. Explicit layers and order values still
resolve larger structural ordering requirements such as ground, actors, roofs, and UI.

## Tiled and navigation

`TiledJsonImporter` already imports finite `orientation: "isometric"` maps into `TileMapData`.
Phase 9 consumes that metadata rather than inventing a second map model. Tile definitions, collision
roles, navigation roles, flip flags, properties, and animation remain shared with orthogonal maps.

The navigation grid remains Cartesian by design. Convert pointer input to a tile or world target
with `IsometricTileGrid2D`, then send that target through the same deterministic A* and path-following
pipeline used by point-and-click games.

## Validation and diagnostics

`Lorenzo2DIsometricTests` covers projection round-trips, conservative bounds, picking, placement,
isometric-data validation, visible-region derivation, and projected-Y ordering. The installed CMake
package is checked for both Phase 9 public headers, and CI builds the standalone
`templates/isometric` consumer.

`Lorenzo2DIsometricBenchmarks` exercises repeated projection/inverse-projection, cell picking, and
view-to-visible-region calculation. It is diagnostic rather than a pass/fail performance gate.

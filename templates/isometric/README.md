# Lorenzo2D isometric starter

This standalone Lorenzo2D 0.13 project demonstrates the Phase 9 isometric API using only installed
public headers and the exported `Lorenzo2D::Lorenzo2D` CMake target.

The logical world remains Cartesian. `IsometricProjection2D` converts logical world positions to a
diamond projection, `IsometricTileGrid2D` handles picking and placement, and
`visibleRegionForView()` produces a conservative Cartesian region that can be supplied to
`TileMap::setStreamRegion()` for projected-view chunk culling. `RenderDepthMode2D::ProjectedY`
uses the projection's render-space Y coordinate for actor ordering.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d/install
cmake --build build
```

For an interactive game, keep physics and navigation in world coordinates, pass the isometric
projection through `RenderContext2D`, and update the tile map stream region when the camera moves.

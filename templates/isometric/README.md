# Lorenzo2D isometric starter

This standalone Lorenzo2D 0.13 project uses only the installed public API. Logical gameplay stays
Cartesian while `IsometricProjection2D` handles presentation. Left-clicking a projected tile uses
inverse projection to pick its Cartesian placement cell, then reserves that cell in
`IsometricPlacementGrid2D`.

Build Lorenzo2D into an install prefix, then configure this folder with that prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d/install
cmake --build build
```

The starter deliberately keeps collision/navigation coordinates unprojected. Use the same
`TileMapData` to build navigation or physics data, and use `RenderDepthMode2D::ProjectedY` for
actors that must sort by their projected footpoint.

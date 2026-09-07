# Layered tile content

`TileMapData` is the scene-independent content model. It stores dimensions, tile size, orthogonal
or isometric orientation, `TileId` definitions, ordered layers, objects, and typed properties.
Properties support booleans, signed integers, finite doubles, and strings.

## Definitions and layers

A `TileDefinition` may reference a texture `AssetId`, atlas rectangle, animation frames, collision
kind, navigation flag, movement cost, and arbitrary properties. `EmptyTile` (`0`) is reserved.

Layers have one of these roles:

- `Ground` and `Decoration`: visible content;
- `Collision`: contributes solid cells without rendering;
- `Trigger`: gameplay metadata without rendering or collision;
- `Navigation`: pathfinding metadata without rendering;
- `Object`: marker for imported object groups.

`TileMapColliderBuilder2D` unions collision-role cells and definitions marked `Solid`, then performs
deterministic greedy rectangle merging. `TileMap::loadFromData` snapshots validated data and creates
its render and collision objects transactionally. Pass an `AssetManager` when any visible tile
definition references a texture; unresolved assets reject the load and preserve the previous map.

```cpp
l2d::AsciiTileMapImporter importer;
importer.mapCharacter('G', 1);
importer.mapCharacter('#', 2);

l2d::TileMapData data;
if (!importer.import({"G#", "GG"}, {32.f, 32.f}, data)) return false;

l2d::TileDefinition wall = *data.definition(2);
wall.collision = l2d::TileCollisionKind::Solid;
wall.navigable = false;
data.setDefinition(wall);

l2d::TileMap runtime;
if (!runtime.loadFromData(scene, data, assets)) return false;
```

The existing character-based `TileMap::loadFromLayout` API remains supported through
`AsciiTileMapImporter::importLegacy`. Its layout lookup and incremental edit methods remain for
existing games; new layered games should edit/import `TileMapData` and reload it transactionally.

## Tiled JSON import

`TiledJsonImporter` accepts finite orthogonal and isometric JSON maps with inline atlas tilesets,
uncompressed array tile layers, object layers, properties, animation frames, and horizontal,
vertical, or diagonal tile flips. It rejects infinite maps, external tilesets, encoded/compressed
layer payloads, unsupported layer types, unknown tile IDs, invalid numbers, and allocations above
the `TileMapData` limits.

Tiled global IDs remain the runtime `TileId`. Atlas image paths become texture `AssetId` strings,
so load those exact IDs into `AssetManager` or remap them in game content before runtime loading.
Layer role defaults to `Ground`; set a string property named `role` to `decoration`, `collision`,
`trigger`, or `navigation` to select another role. Tile properties named `solid`, `navigable`, and
`movementCost` populate their strongly typed definition fields while remaining available as normal
metadata where applicable.

Isometric orientation remains content metadata: logical tile coordinates stay Cartesian and are
shared by physics and navigation. In 0.13, `IsometricTileGrid2D` consumes validated isometric
`TileMapData` to provide bounded render-space picking, world/render placement anchors, conservative
projected region bounds, and camera-derived `TileMapRegion` values for `TileMap::setStreamRegion()`.
See [`isometric.md`](isometric.md) for the complete Phase 9 workflow.

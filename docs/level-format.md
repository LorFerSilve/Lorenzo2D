# Lorenzo2D level format

`LevelSerializer::save` writes deterministic JSON version 4. The serializer validates the entire
`LevelDocument` before writing, and `load` replaces its destination only after the complete input
has parsed and passed validation. The object limit is 100,000.

## Version 4 schema

The root record contains `format`, `version`, `name`, and `objects`. Each object contains its base
state plus a component array:

```json
{
  "format": "Lorenzo2DLevel",
  "version": 4,
  "name": "Village",
  "objects": [
    {
      "name": "Player",
      "tag": "player",
      "active": true,
      "zOrder": 10,
      "transform": {
        "position": [100.0, 200.0],
        "rotation": 0.0,
        "scale": [1.0, 1.0]
      },
      "components": [
        {
          "type": "SpriteRenderer",
          "version": 1,
          "required": true,
          "data": {
            "texture": "characters/player.png",
            "rect": [0, 0, 32, 48],
            "size": [32.0, 48.0],
            "color": [255, 255, 255, 255],
            "origin": [16.0, 48.0],
            "flipX": false,
            "flipY": false,
            "renderOrder": {
              "layer": 10,
              "depth": 0.0,
              "order": 0,
              "mode": 1
            }
          }
        }
      ]
    }
  ]
}
```

Built-in version-1 component records cover rectangle/circle renderers, sprite renderers, animators,
rigid bodies, and box/circle/capsule/convex-polygon colliders. Sprite textures and animation clips
are stable `AssetId` strings. Instantiate asset-backed documents with an `AssetManager`; a missing
required asset rejects and rolls back the complete operation.

## Custom component codecs

`ComponentCodecRegistry` maps `(type, version)` pairs to an encoder and decoder. `data` may contain
any JSON value and is delivered to codecs as compact JSON text. Unknown optional records are
skipped, while an unknown required record or a failing decoder rejects instantiation. This keeps
saved component versions explicit and prevents silent interpretation of a newer schema.

```cpp
l2d::ComponentCodecRegistry codecs;
codecs.registerCodec(
    "game.Team", 1,
    [](const l2d::GameObject& object) -> std::optional<std::string> {
        const auto* team = object.getComponent<Team>();
        return team ? std::optional<std::string>("{\"id\":1}") : std::nullopt;
    },
    [](l2d::GameObject& object, const std::string& json) {
        return parseAndAttachTeam(object, json);
    });

auto handles = l2d::LevelSerializer::instantiate(scene, level, assets, &codecs);
```

## Legacy migration

`load` auto-detects the former line-oriented `LORENZO2D_LEVEL` format and continues to read
versions 1, 2, and 3. Saving a loaded legacy document writes JSON version 4. The checked-in
`assets/levels/phase2-showcase.l2dlevel` remains a version-1 compatibility fixture.

The old formats cannot encode sprite assets, animator state, repeated/custom components, angular
body state, sleeping, or joints. Existing fields retain their former defaults during migration;
unknown trailing data and a text header claiming version 4 are rejected.

# Lorenzo2D level format

`.l2dlevel` is a deterministic line-oriented representation of `LevelDocument`.
Version 1 deliberately uses a fixed record order so malformed or incompatible
data can be rejected before the destination document changes. See
`assets/levels/phase2-showcase.l2dlevel` for a complete example.

## Header

```text
LORENZO2D_LEVEL 1
level "Level name"
objects 2
```

Quoted strings use the C++ `std::quoted` representation. Names and tags may be
empty but cannot contain line breaks. `objects` cannot exceed 100,000.

## Object record

Every object uses this exact field order:

```text
object
name "Player"
tag "player"
active 1
transform 100 200 0 1 1
rectangle 1 32 48 20 80 220 255
circle 0
rigid_body 1 2 0 0 0 0 1 1 1
box_collider 1 30 44 1 2 0 0.6 0.4 2 5 0
circle_collider 0
end
```

Each optional component starts with `0` or `1`. A zero ends that line. A one is
followed by:

| Component | Values after presence flag |
| --- | --- |
| `rectangle` | size x/y, RGBA bytes |
| `circle` | radius, RGBA bytes |
| `rigid_body` | body type, velocity x/y, acceleration x/y, mass, use-gravity, gravity scale |
| `box_collider` | size x/y, then common collider properties |
| `circle_collider` | radius, then common collider properties |

Body types are `0` static, `1` kinematic, and `2` dynamic. Common collider
properties are offset x/y, restitution, static friction, dynamic friction,
category bits, mask bits, and the sensor flag. Boolean fields must be `0` or
`1`. Colors use integer channels from 0 through 255. Floating-point values must
be finite, sizes and radii cannot be negative, mass must be positive, and the
material must already satisfy the engine's normalized `[0, 1]` contract.

Version 1 supports at most one collider per prefab because the physics world
uses the first collider on an object. Shape renderers may be combined. Sprite
asset references, animation state, custom component codecs, and migrations are
reserved for later versions.

# Rendering 2.0

Phase 12 evolves Lorenzo2D's deterministic 2D presentation path without replacing the simple
SpriteRenderer/TileMap workflows or introducing a general-purpose render graph.

## 12.1 Shader and material foundation

The first Rendering 2.0 slice adds two public abstractions:

- `Shader2D`: an owned, transactionally compiled shader program with an explicit uniform layout;
- `Material2D`: typed per-draw uniform, texture, shader, and blend state.

Existing renderers remain usable without either abstraction. `SpriteRenderer` has an optional
material binding; when no material is assigned it follows the same default SFML draw path as before.

### Shader lifetime and compilation

`Shader2D` owns the underlying shader program. It is move-only and is normally shared with
materials through `Shader2DHandle` (`std::shared_ptr<Shader2D>`).

The initial public loading surface supports:

- fragment source from memory;
- vertex + fragment source from memory.

Each source is bounded to 1 MiB and embedded NUL bytes are rejected. Loading is transactional:
source validation or GLSL compilation failure leaves the previously loaded program unchanged.

Constructing `Shader2D` and defining its uniform layout are CPU-only operations; the SFML shader
backend is allocated lazily. Source compilation and loaded-shader destruction/application are
graphics-resource work and should follow the owning render thread/context discipline unless the
application deliberately establishes another compatible graphics context.

### Explicit uniform layout

Lorenzo2D does not infer a material contract from driver-specific shader reflection. Instead,
`Shader2D::setUniformLayout()` declares the names, types, and required status used by materials.

Supported Phase 12.1 types are:

| Lorenzo2D type | Material setter | GLSL intent |
| --- | --- | --- |
| `Float` | `setFloat()` | `float` |
| `Integer` | `setInteger()` | `int` |
| `Boolean` | `setBoolean()` | `bool` |
| `Vector2` | `setVector2()` | `vec2` |
| `Vector3` | `setVector3()` | `vec3` |
| `Color` | `setColor()` | normalized `vec4` |
| `Texture` | `setTexture()` / `setCurrentTexture()` | `sampler2D` |

The layout is bounded to 64 uniforms, names are bounded to 128 bytes, duplicate names are rejected,
and invalid layout replacement is transactional.

This schema is deliberately explicit: a mismatch between the declared layout and the GLSL source is
an authoring error. SFML may report a missing/optimized-out GLSL uniform through its own shader
diagnostics; Lorenzo2D validates the declared material contract before draw submission.

### Material validation

A `Material2D` may be used as a blend-only material with no shader. Once a shader is attached,
typed uniform setters accept only names whose declared type matches the shader layout.

Validation includes:

- finite floating-point scalar/vector values;
- valid texture handles for explicit sampler bindings;
- typed layout compatibility;
- required-uniform completeness;
- valid blend-mode enumeration;
- rejection of a shader rebind when existing material state is incompatible.

`isComplete()` reports whether the complete current material state is drawable.
`apply(sf::RenderStates&)` refuses incomplete state and only publishes the new RenderStates after
validation succeeds.

Every declared uniform is written on every shader-backed material application. Omitted optional
uniforms receive canonical defaults so materials sharing one shader cannot inherit state from a
previous draw:

- numeric scalars/vectors: zero;
- boolean: `false`;
- color: transparent black;
- texture: the drawable's current texture.

Games that need a different default should set that value explicitly on the material.

### Texture lifetime

`setTexture()` stores a `TextureHandle`, so the texture lease remains alive with the material.
This is important because the backend shader stores a sampler reference rather than copying the
texture.

`setCurrentTexture()` binds the drawable's current texture instead and is the normal choice for a
SpriteRenderer material that should shade the sprite's own texture.

### Blend configuration

`MaterialBlendMode2D` currently exposes:

- `Alpha`;
- `Add`;
- `Multiply`;
- `None`.

Blend state is material-owned and written into the draw's RenderStates together with the shader.

### SpriteRenderer integration

Material use is optional:

```cpp
auto shader = std::make_shared<l2d::Shader2D>();
shader->loadFragmentSource(fragmentSource);
shader->setUniformLayout({
    {"tint", l2d::ShaderUniformType2D::Color, true},
    {"texture", l2d::ShaderUniformType2D::Texture, true},
});

auto material = std::make_shared<l2d::Material2D>();
material->setShader(shader);
material->setColor("tint", sf::Color::White);
material->setCurrentTexture("texture");

spriteRenderer.setMaterial(material);
```

Calling `clearMaterial()` restores the original simple SpriteRenderer draw path.

### Mutation and concurrency

Shader compilation, material mutation, material application, and draw submission are a
single-owner render-thread/context contract. Sharing a `Shader2D` between materials is supported:
each material application writes every declared uniform—stored state or a canonical optional
default—immediately before its draw.

Applications must not concurrently mutate/apply materials sharing one shader without their own
external synchronization and graphics-context discipline.

## Failure model

| Operation | Failure behavior |
| --- | --- |
| invalid/oversized source | returns `false`; old program preserved |
| shader compilation failure | returns `false`; old program preserved |
| invalid uniform layout | returns `false`; old layout preserved |
| wrong typed setter | returns `false`; previous value preserved |
| non-finite scalar/vector | returns `false`; previous value preserved |
| invalid texture handle | returns `false`; previous value preserved |
| incompatible shader rebind | returns `false`; old shader preserved |
| incomplete material apply | returns `false`; caller RenderStates preserved |

## Deliberately not in 12.1

This slice does not yet implement:

- off-screen render targets;
- configurable pass orchestration;
- shader-based post-processing chains;
- batching/material switch statistics;
- tile/sprite batching;
- atlas generation;
- 2D lighting.

Those build on this material/state contract in later Phase 12 slices.

## Validation portability

The shader/material regression executable always validates CPU-side uniform-layout and public API
behavior. Runtime GLSL compilation, material application, and pixel readback cases execute only when
`Shader2D::isSupported()` is true. Linux/Xvfb CI provides the required shader-capable path and runs
those GPU cases fully; a runner that exposes no shader capability reports those cases as skipped
instead of treating unavailable hardware/runtime support as an engine failure.

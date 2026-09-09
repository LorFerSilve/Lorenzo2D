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

## 12.2 Render surfaces and off-screen rendering

`RenderSurface2D` is the first explicit off-screen target resource in Rendering 2.0. It owns one
SFML render texture and exposes:

- bounded, transactionally-created pixel storage;
- a borrowed `sf::RenderTarget*` for drawing;
- a borrowed output `sf::Texture*` after `display()`;
- allocation/content generations;
- surface-to-target presentation with optional `Material2D` state.

Construction and config validation are CPU-only. The graphics backend is allocated only by
`create()`.

### Bounded allocation

The initial public envelope is:

- maximum dimension: 8,192 pixels per axis;
- maximum pixel count: 33,554,432 pixels.

Both limits are validated before touching the graphics backend. Actual GPU/driver limits may be
lower; backend creation can therefore still fail. Such failure is transactional and preserves the
previous valid surface, config, texture, and generation.

A successful replacement increments `allocationGeneration()` and resets
`contentGeneration()`. `reset()` invalidates the current allocation and advances the allocation
generation. Generations saturate instead of wrapping.

### Drawing and publication

A typical frame is:

```cpp
l2d::RenderSurface2D surface;
surface.create({{1280u, 720u}, false, false});

surface.clear(sf::Color::Black);
surface.target()->draw(drawable);
surface.display();
```

`display()` is the publication boundary for the texture contents and increments
`contentGeneration()`. The target/texture pointers are borrowed and become invalid after a
successful `create()` replacement or `reset()`.

### Compositing

`present()` draws the published surface texture into any `sf::RenderTarget`, including another
`RenderSurface2D` target. Presentation supports:

- destination position;
- optional destination size/scaling;
- vertex color modulation;
- optional `Material2D`.

Presentation position and explicit size are expressed in the destination target's **current-view
coordinates**. Callers that need pixel-space composition should set/use the destination default view
for that operation.

This makes surface chaining possible before the configurable pass system exists. Presenting a
surface into its own target is rejected to avoid read/write feedback on the same texture.
Non-finite positions and non-positive/non-finite explicit sizes are also rejected.

### Camera and lightweight post-process targeting

`Camera2D::applyTo()` and `PostProcessStack2D::apply()` now have additive
`sf::RenderTarget&` overloads. Existing `sf::RenderWindow&` overloads remain source compatible
and delegate to the generic target path.

The legacy `PostProcessStack2D` is still only a color-overlay/fade stack. Shader-based full-screen
post-processing belongs to the configurable pass/post-process slices that follow 12.2.

### Scene/Component compatibility boundary

The 1.x `Component::onRender(sf::RenderWindow& ...)` virtual surface is intentionally unchanged in
12.2. Replacing that signature with `sf::RenderTarget&` would break existing custom components.

Therefore 12.2 establishes the off-screen resource/compositing contract without silently claiming
that every existing Scene component can render into it. The next pass-orchestration slice must define
an additive, explicit submission/migration boundary rather than skipping legacy custom renderers
silently.

### Failure model

| Operation | Failure behavior |
| --- | --- |
| invalid surface size/config | returns `false`; current allocation preserved |
| GPU/backend creation failure | returns `false`; current allocation preserved |
| clear/display without allocation | returns `false` |
| invalid present transform | returns `false`; destination untouched |
| incomplete presentation material | returns `false`; destination untouched |
| self-presentation feedback | returns `false` |

### Concurrency and lifetime

Surface creation, destruction, draw submission, `display()`, and presentation are
single-owner graphics-thread/context operations. Config validation/default construction are CPU-only.

Games must not retain `target()` or `texture()` pointers across successful reallocation/reset.
Use the owning `RenderSurface2D` or `RenderSurface2DHandle` as the lifetime anchor.

`RenderSurface2D` itself is intentionally neither copyable nor movable. Stable object identity keeps
borrowed target/texture pointers and allocation-generation observers tied to one lifetime anchor;
moving an allocated surface would otherwise invalidate those observers without an unambiguous
monotonic generation transition.

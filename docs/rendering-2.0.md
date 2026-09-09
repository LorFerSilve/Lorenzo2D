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

The legacy `PostProcessStack2D` remains a color-overlay/fade stack. Phase 12.4 adds the separate
`ShaderPostProcessChain2D` for full-screen shader passes, so existing lightweight fades do not
inherit off-screen workspace or shader requirements.

### Scene/Component compatibility boundary

The 1.x `Component::onRender(sf::RenderWindow& ...)` virtual surface is intentionally unchanged in
12.2. Replacing that signature with `sf::RenderTarget&` would break existing custom components.

Therefore 12.2 establishes the off-screen resource/compositing contract without silently claiming
that every existing Scene component can render into it. Phase 12.3 supplies the additive explicit
pass-orchestration/legacy bridge rather than skipping custom renderers silently.

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

## 12.3 Configurable render-pass orchestration

`RenderPipeline2D` adds a bounded ordered pass layer on top of `RenderSurface2D`. It is
intentionally **not** a general render graph: passes execute in vector order, dependencies are
declared for validation/lifetime only, and the engine never topologically reorders work.

Each `RenderPipelinePass2D` declares:

- a unique bounded name;
- one logical `RenderPass2D` context (`World`, `PhysicsDebug`, or `UI`);
- one output target: the caller backbuffer or an owned `RenderSurface2D`;
- zero or more read-only surface inputs;
- optional clear policy;
- optional automatic presentation of a surface output back to the backbuffer;
- enabled/disabled state.

The initial envelope is 64 passes, 16 declared inputs per pass, and 128 bytes per pass name.

### Deterministic ordering and mutation

Insertion order is execution order. `movePass()` performs an explicit reorder; disabled passes retain
their index but are skipped and do not contribute to `completedPasses`.

Pipeline mutation is rejected while `execute()` is active. Recursive execution on the same pipeline
returns `ReentrantExecution` instead of entering a second render traversal.

Each pass also receives isolated target-view state: the target's incoming `sf::View` is restored when
the pass exits, including callback failure and exception unwinding. A callback may apply a camera for
its own draw work without leaking that view into later passes.

### Frame context and projection lifetime

`RenderPipelineFrame2D` supplies interpolation alpha and an optional coordinate projection once per
execution. The interpolation alpha must be finite and within `[0, 1]`.

The projection pointer is borrowed **only for the duration of `execute()`** and is never stored in the
pipeline. Each pass receives a normal `RenderContext2D` built from that frame state plus the pass's
logical `RenderPass2D`.

### Inputs and outputs

Surface inputs use `RenderSurface2DConstHandle`, preventing pass callbacks from mutating the
declared input through the pipeline API. A declared input must:

- still be allocated;
- have a valid target/texture;
- contain published content from an earlier `display()`, or be the output of an earlier enabled
  surface pass in the same pipeline run;
- not alias the current output target.

A surface output is automatically published with `display()` after its callback succeeds. A pass
may then automatically `present()` that surface to the backbuffer using the existing
`RenderSurfacePresent2D` position/size/color/material contract.

Self/read-write feedback is rejected during preflight.

### Preflight and side effects

Before pass 0 clears or draws anything, the pipeline validates the complete enabled pass sequence:

- frame validity;
- structural pass validity;
- legacy Scene requirements;
- output availability;
- input availability/publication;
- input/output feedback.

If preflight fails, `completedPasses == 0` and no pass callback or clear operation has run.

Once execution begins, GPU draw submission is not transactionally reversible. If a callback returns
`false`, publishing fails, or presentation fails, the result identifies the failing pass and counts
only passes that fully completed before it. Exceptions from user callbacks/legacy Scene rendering
still propagate after the pipeline resets its internal executing guard.

### Legacy Scene compatibility bridge

The existing 1.x virtual rendering contract remains:

`Component::onRender(sf::RenderWindow&, ...)`

Changing it to `sf::RenderTarget&` would break existing custom components. 12.3 therefore exposes an
explicit compatibility path:

- `addLegacyScenePass()` marks a backbuffer pass that dispatches the existing Scene renderer;
- generic `execute(sf::RenderTarget&)` rejects pipelines containing such a pass with
  `LegacySceneRequired` during preflight;
- `execute(sf::RenderWindow&, Scene&)` supplies the required legacy window and Scene only for that
  call; neither is retained by the pipeline.

This avoids silently skipping old components while allowing new callback passes to target off-screen
surfaces immediately.

### Failure reporting

`RenderPipelineResult2D` reports a stable failure enum, optional pass index, and the number of fully
completed enabled passes. `renderPipelineFailureName()` exposes stable diagnostic names.

Important failures include:

| Failure | Meaning |
| --- | --- |
| `InvalidFrame` | interpolation alpha is outside the supported finite `[0,1]` range |
| `InvalidPass` | an enabled pass no longer satisfies its structural contract |
| `LegacySceneRequired` | generic target execution encountered a legacy Scene pass |
| `SurfaceUnavailable` | an output surface is not allocated |
| `InputUnavailable` | a declared input is not allocated/published |
| `FeedbackLoop` | declared input/output or surface/backbuffer feedback was detected |
| `CallbackFailed` | a generic pass callback returned `false` |
| `SurfacePublishFailed` | output `display()` failed |
| `SurfacePresentFailed` | automatic presentation failed |

### Concurrency and callback lifetime

Pipeline configuration and execution are a single-owner render-thread contract. Callbacks are owned
by the pipeline and may capture game state, so captured references must outlive every execution that
can invoke them.

Surface handles retain input/output lifetimes, but callbacks must not reallocate/reset pipeline
surfaces or mutate shared materials concurrently while execution is active.

The caller still owns final backbuffer publication: `RenderPipeline2D` never calls
`RenderWindow::display()` or an equivalent backbuffer display operation.

## 12.4 Shader-based post-processing

`ShaderPostProcessChain2D` adds a bounded, ordered full-screen shader path over the 12.1–12.3
foundations. It consumes one already-published `RenderSurface2D` and writes the final result into any
`sf::RenderTarget`, including the backbuffer or the output target of a `RenderPipeline2D` callback.

This is deliberately separate from `PostProcessStack2D`. The legacy stack remains the lightweight
screen-space color-overlay/fade API and does not allocate off-screen surfaces or require shader
support.

### Pass contract

Each `ShaderPostProcessPass2D` contains:

- a unique non-empty name bounded to 128 bytes;
- one non-null `Material2DHandle`;
- enabled/disabled state;
- optional output clearing and a clear color.

A chain contains at most 16 passes. Insertion order is execution order; `movePass()` is the explicit way to change that order. Structural configuration is CPU-only: a pass may be defined
before its shader is compiled, but `apply()` rejects enabled passes whose material has no loaded
shader or is incomplete.

The source texture is the full-screen sprite's current texture. Post-process materials should bind
their source sampler with `Material2D::setCurrentTexture()`. Other material texture uniforms remain
available for auxiliary inputs such as noise or lookup textures.

### Published source and no-pass behavior

The source surface must be allocated and have `contentGeneration() > 0`, which means its current
contents have been published through `RenderSurface2D::display()`. Reading directly from an
unpublished render texture is rejected with `SourceUnpublished`.

If every pass is disabled, the chain performs an exact full-screen copy using overwrite blending.
This gives callers one stable composition entry point even when an effect is toggled off.

The source surface cannot also be the destination target. That read/write feedback is rejected before
any draw submission.

### Ping-pong workspace

A single enabled shader pass renders directly into the destination and allocates no internal render
surface. Two or more enabled passes lazily allocate at most two `RenderSurface2D` workspaces and
alternate between them.

Workspace properties follow the source pixel size and smoothing flag. Compatible allocations are
reused across frames; a source-size/filter change causes transactional replacement before drawing.
`resetWorkspace()` explicitly releases retained GPU resources.

Intermediate passes are published with `display()` before their texture becomes the next pass's
input. The final destination is not published by the chain: when that destination belongs to a
`RenderSurface2D` pipeline pass, `RenderPipeline2D` remains responsible for its normal publication
step.

### Coordinate and view contract

Full-screen work runs in the destination target's default view and scales the source texture to the
destination pixel extent. The caller's incoming view is restored after every destination/workspace
draw scope, including exception unwinding.

Post-processing therefore operates in presentation pixels rather than world/projected coordinates.
World camera/projection state belongs to the pass that produced the source surface.

### Failure reporting

`ShaderPostProcessResult2D` reports a stable failure enum, optional original pass index, and the
number of fully completed enabled passes. `shaderPostProcessFailureName()` exposes stable diagnostic
names.

Important failures include:

| Failure | Meaning |
| --- | --- |
| `InvalidPass` | an enabled pass no longer satisfies its structural contract |
| `SourceUnavailable` | the input surface is not allocated |
| `SourceUnpublished` | the input surface has no published generation |
| `DestinationUnavailable` | the destination has zero pixel extent |
| `FeedbackLoop` | source and destination are the same render target |
| `ShaderUnavailable` | an enabled material has no loaded shader |
| `MaterialIncomplete` | required/typed material state is incomplete |
| `WorkspaceAllocationFailed` | a required ping-pong surface could not be allocated |
| `WorkspaceClearFailed` | an intermediate target could not be cleared |
| `MaterialApplyFailed` | material state could not be applied for a full-screen draw |
| `WorkspacePublishFailed` | an intermediate render surface could not be published |

Preflight validates source, destination, feedback, shader availability, and material completeness
before workspace allocation or destination drawing. Once GPU draw submission starts, partial output
is not transactionally reversible.

### RenderPipeline2D integration

A typical pipeline first renders new target-capable content into a `RenderSurface2D`, then declares
that published surface as a read-only input of a backbuffer callback pass. The callback invokes
`ShaderPostProcessChain2D::apply()` with the declared input and its supplied target.

`Lorenzo2DPhase12PostProcessExample` demonstrates this exact public API path. The chain performs no
dependency scheduling of its own; surface ordering and publication remain explicit in
`RenderPipeline2D`.

### Lifetime and concurrency

Passes retain their `Material2DHandle` values. Materials in turn retain shader and texture leases
according to the 12.1 contract. Internal workspace surfaces are owned by the chain until
`resetWorkspace()` or destruction.

Configuration, material mutation, workspace allocation, and `apply()` are a single-owner
render-thread/context contract. Concurrent mutation/application is unsupported.

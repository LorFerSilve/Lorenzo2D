# Render statistics 2D

Phase 12.7 completes the Rendering 2.0 diagnostic surface with one ordered, frame-local telemetry
contract. `RenderStatisticsRecorder2D` observes successful draw submissions in the same order in
which they reach the rendering backend and exposes the resulting `RenderStatistics2D` snapshot.

The recorder is optional. Passing no recorder preserves every existing rendering path and does not
add hidden global state.

## Statistics

`RenderStatistics2D` exposes seven counters:

| Counter | Meaning |
| --- | --- |
| `drawCallCount` | successful observed backend draw submissions |
| `submittedPrimitiveCount` | submitted triangles |
| `batchCount` | submitted state-compatible draw batches |
| `materialSwitchCount` | transitions between consecutive observed `Material2D` identities |
| `shaderSwitchCount` | transitions between consecutive observed `Shader2D` identities |
| `renderedItemCount` | logical rendered items represented by the observed draw work |
| `culledItemCount` | explicit culling decisions reported by participating subsystems |

All counters saturate at `std::numeric_limits<std::size_t>::max()` instead of wrapping.

### Primitive and batch units

The primitive unit is always a **triangle**. A normal sprite, render-surface presentation, or
full-screen post-process submission contributes two primitives. `SpriteBatch2D` contributes two
triangles per successfully drawn sprite. Tilemap telemetry derives primitives from the existing
triangle vertex count (`submittedVertexCount / 3`).

A batch is a draw group that already shares the state required by the owning renderer. A simple
sprite/full-screen draw therefore contributes one batch. Each issued `SpriteBatch2D` batch
contributes one. The current tilemap bridge treats each existing tilemap draw call as one batch
because its chunk vertex arrays are already grouped by drawable state.

## Ordered material and shader switches

State-switch counts are intentionally not reconstructed from unordered totals. The recorder keeps
the material and shader identity of the previous observed draw and compares the next draw against
it.

Rules:

- the first observed draw creates the baseline and contributes zero switches;
- the same `Material2D` object reused by adjacent draws contributes no material switch;
- two different `Material2D` objects count as a material switch even if their current values happen
  to be equal;
- two different materials sharing the same `Shader2D` count as a material switch but not a shader
  switch;
- switching from shader-backed state to the default/no-shader state (or back) counts as a shader
  switch;
- the null material is the default material identity and transitions to/from it count as material
  switches after the first observed draw.

Each draw record carries a `Material2DHandle` only for the duration of the recording call. The
recorder retains `weak_ptr` ownership identities for the previous material and shader. Those weak
identities do not keep rendering resources alive, but their control blocks remain distinguishable
even after the underlying object is destroyed. A later material or shader that happens to reuse the
same memory address is therefore still counted as a different render-state identity.

## Integration points

### `RenderContext2D` and `SpriteRenderer`

`RenderContext2D` has an optional `RenderStatisticsRecorder2D*`. `SpriteRenderer` automatically
records a successful sprite draw when that pointer is non-null. A draw rejected by numeric safety or
material application is not counted.

### `SpriteBatch2D`

The existing `SpriteBatch2D::draw(target)` overload remains unchanged for 1.x source compatibility.
Pass a recorder to the additional `SpriteBatch2D::draw(target, recorder)` overload when telemetry is
required. Each successfully issued batch is recorded after the backend draw call. If a later
material application fails, statistics retain only the batches that were actually submitted before
the failure.

### `RenderSurface2D`

The existing `RenderSurface2D::present(target, presentation)` member-function signature remains
available. The recorder-aware overload accepts a third argument. A successful surface presentation
records one draw, two triangles, one batch, and one rendered item using the presentation material
identity. Clear and `display()` operations are publication operations, not draw calls, and therefore
do not increment render statistics.

### `ShaderPostProcessChain2D`

The existing two-argument `ShaderPostProcessChain2D::apply(source, target)` overload remains
available. The recorder-aware overload accepts a third argument and records every successful
full-screen draw, including internal ping-pong workspace passes. A no-pass copy is still a real
full-screen backend draw and is therefore recorded.

### `RenderPipeline2D`

`RenderPipelineFrame2D::statistics` is borrowed only for one `execute()` call. It is propagated into
each pass `RenderContext2D`, so callback renderers can use `execution.context.statistics`.
Pipeline-owned surface presentation also receives the same recorder automatically.

The pipeline does not guess what arbitrary callback code draws. A custom callback that issues raw
SFML draws is responsible for recording those draws itself after successful submission.

### `RenderComposition2D`

`RenderCompositionFrame2D::statistics` is likewise borrowed only during execution. Every generated
composition context receives the same recorder, which keeps material/shader transitions ordered
across cameras, viewports, passes, and layer ranges.

### Tilemaps

Existing `TileMapRenderStats` remains the authoritative tilemap render result. Call
`recordTileMapRenderStatistics(stats, recorder)` to append that result to the ordered stream.

Tilemap culling is currently reported at **chunk granularity**, because `TileMap` already computes
`visibleChunkCount`/`culledChunkCount` as its actual view/streaming culling unit. Phase 12.7 does not
invent a per-tile cull count after the fact.

## DiagnosticCounters bridge

The Phase 11 `DiagnosticCounter` enum gains five appended counters without changing the numeric
values of existing counters:

- `SubmittedPrimitives` -> `submitted_primitives`;
- `RenderBatches` -> `render_batches`;
- `MaterialSwitches` -> `material_switches`;
- `ShaderSwitches` -> `shader_switches`;
- `CulledItems` -> `culled_items`.

`accumulateRenderStatisticsDiagnostics()` copies a `RenderStatistics2D` snapshot into the standard
saturating `DiagnosticCounters`. Those counters therefore flow through the existing deterministic
`DiagnosticSnapshot` and JSON bug-report format.

The older `accumulateTileMapRenderDiagnostics()` and `accumulateSpriteBatchDiagnostics()` adapters
remain for compatibility and now fill the new counters where their legacy result structs contain
exact information. Do **not** also accumulate those legacy adapters for draw work already represented
by a complete `RenderStatisticsRecorder2D` snapshot in the same reporting interval; doing so would
double count that work.

## Typical frame

```cpp
l2d::RenderStatisticsRecorder2D renderStats;
renderStats.reset();

l2d::RenderPipelineFrame2D frame;
frame.statistics = &renderStats;

const auto result = pipeline.execute(window, scene, frame);

l2d::DiagnosticCounters counters;
l2d::accumulateRenderStatisticsDiagnostics(renderStats.statistics(), counters);
```

When a tilemap is rendered outside a path that already records its result, append its existing stats
before exporting the frame:

```cpp
l2d::recordTileMapRenderStatistics(tileMap.lastRenderStats(), renderStats);
```

## Failure and lifetime semantics

Render statistics describe **work that was actually observed**, not intended work. Configuration or
preflight failures contribute nothing. If execution fails after earlier draws completed, those draws
remain in the recorder; telemetry is not transactionally rolled back because the GPU work itself
cannot be rolled back.

The recorder owns no rendering resources. `RenderDrawStatistics2D` accepts shared handles so an
identity can be captured safely during the call, while the recorder itself stores only weak ownership
identity afterward. Frame/context recorder pointers are borrowed for their documented execution
scope. Callers normally own one recorder per render thread/frame and call `reset()` before starting
the next measurement interval.

The recorder is not a concurrent aggregation primitive. Recording and reset must not race with each
other or with rendering. Separate render threads require separate recorders or external
synchronization.

## Validation

`Lorenzo2DRenderStatisticsTests` covers ordered state transitions, saturation, shader identity,
identity reuse after destruction, preservation of the three legacy renderer member-function
signatures, `SpriteRenderer`, sprite batching, render-surface presentation, no-pass post-processing,
pipeline and composition propagation, tilemap bridging, and export into `DiagnosticCounters`.

The installed-package and `add_subdirectory` consumers also take member pointers to the legacy
`RenderSurface2D::present`, `SpriteBatch2D::draw`, and `ShaderPostProcessChain2D::apply` overloads, so
those signatures are protected by downstream integration CI as well.

The existing Phase 12 sprite-batching example now also records its frame and verifies that 200 atlas
sprites collapse to one draw call, 400 submitted triangles, one batch, and 200 rendered items.

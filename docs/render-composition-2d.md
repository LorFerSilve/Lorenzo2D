# Render composition 2D

Phase 12.6 introduces Lorenzo2D's explicit camera/layer composition path. Phase 12.6.1 established
the bounded CPU-side configuration contract; Phase 12.6.2 adds deterministic execution against a
generic SFML render target and a compatibility bridge for the existing `RenderWindow`-based Scene
renderer.

The composition layer deliberately remains smaller than `RenderPipeline2D`. It selects **which
camera, render pass, layer range, and viewport** are active and in which order. Render-target
allocation, clear policy, surface dependencies, publication, post-processing, and final presentation
remain pipeline/application concerns.

## Render layer ranges

`RenderContext2D` carries an inclusive `RenderLayerRange2D`. The default range spans the complete
`std::int32_t` domain, so existing rendering remains unchanged unless a caller explicitly narrows the
range.

`RenderQueue2D::build()` preserves the existing render-pass filter and deterministic sort-key
construction, then rejects entries whose `RenderSortKey2D::layer` falls outside the context range.
Objects without an active `RenderOrder2D` remain in the World pass at layer `0` and are therefore
included by the default range.

An invalid range (`minimum > maximum`) contains no layers. Invalid ranges cannot be stored in a
`RenderCompositionEntry2D`, while direct `RenderQueue2D` use with such a context deterministically
produces an empty layer selection.

## Composition entries

`RenderComposition2D` stores an explicitly ordered, bounded list of `RenderCompositionEntry2D`
records. Every entry defines:

- a unique non-empty name of at most 128 bytes;
- a borrowed `Camera2D` pointer;
- one `RenderPass2D`;
- one inclusive `RenderLayerRange2D`;
- one normalized SFML viewport;
- an enabled flag.

A composition contains at most 16 entries. Invalid entries, duplicate names, invalid indices, and
capacity overflow are rejected before the existing configuration is changed.

Entry order is insertion order. No state-based sorting or implicit camera grouping occurs.
`moveEntry()` is the only explicit operation that changes an existing entry's relative position.
Disabled entries retain their configured index but are skipped during execution and do not contribute
to `completedEntries`.

## Camera ownership and viewports

Composition entries do not own cameras. The caller must keep every referenced `Camera2D` alive for
as long as its entry can be queried or executed. `RenderComposition2D` never mutates a borrowed
camera itself.

`makeView()` copies the camera's current `sf::View` and changes only the copy's normalized viewport.
Viewport coordinates follow SFML's normalized target convention: position must be non-negative,
size must be positive, all values must be finite, and the rectangle must remain inside normalized
`[0, 1] x [0, 1]` target space.

## Frame context

`RenderCompositionFrame2D` supplies interpolation alpha and an optional coordinate projection.
Alpha must be finite and in `[0, 1]`.

`makeContext()` combines that frame state with the selected entry's pass and layer range. Disabled
entries and invalid frame state return `std::nullopt`.

`RenderContext2D` retains an explicit constructor whose first three parameters match the historical
brace-initialization order (`interpolationAlpha`, `projection`, `pass`), with the layer range as an
optional fourth parameter. Existing 1.x source call sites therefore retain their previous default
all-layers behavior.

## Ordered execution

The generic execution API is:

```cpp
const l2d::RenderCompositionResult2D result = composition.execute(
    target,
    [](const l2d::RenderCompositionExecution2D& execution)
    {
        // Draw using execution.target and execution.context.
        return true;
    },
    frame);
```

Each enabled entry executes once in configured order. `RenderCompositionExecution2D` exposes:

- the caller's `sf::RenderTarget&`;
- the current immutable entry configuration;
- the generated `RenderContext2D`;
- the entry's original configured index.

The composition applies the entry's prepared camera view before invoking the callback. A callback
returning `false` stops the sequence immediately and reports `CallbackFailed`; only earlier enabled
entries count as completed. A composition with zero enabled entries is a successful zero-work
execution when the frame and callback are valid.

The callback object and all references inside `RenderCompositionExecution2D` are borrowed for the
callback invocation only and must not be retained beyond it.

## Preflight and camera snapshots

Before the first callback runs, execution validates the frame and every enabled entry and snapshots
each generated `RenderContext2D` and camera `sf::View`.

This preflight has two important consequences:

1. a structural failure in a later enabled entry is reported before any earlier callback draws;
2. changing a referenced `Camera2D` from an earlier callback affects the **next composition
   execution**, not a later entry already prepared for the current execution.

The entry configuration itself also cannot be mutated through the composition while execution is
active. This prevents callback-driven vector invalidation, index changes, or ordering changes in the
middle of a traversal.

## Target-view restoration

`execute()` captures the target's incoming `sf::View` once and restores it before returning. The
restore also happens when a callback reports failure or throws an exception.

A callback may temporarily change the target view for its own work, but that change does not leak to
the next composition entry: the next prepared camera view is applied explicitly. It also does not
leak back to the caller after the composition finishes.

The executor does **not** clear the target and does not call `display()`. Multiple viewports therefore
compose onto the existing target contents according to normal draw ordering and blending semantics.

## Reentrancy and mutation

`RenderComposition2D` follows a single-owner render-thread execution contract.

While `execute()` is active:

- recursive execution of the same composition returns `ReentrantExecution`;
- `addEntry()`, `setEntry()`, `setEntryEnabled()`, `moveEntry()`, `removeEntry()`, and `clear()`
  return `false` without modifying the configured plan;
- `executing()` reports `true`.

The guard is reset on normal completion, callback failure, and exception unwinding.

## Legacy Scene compatibility bridge

The existing 1.x custom-component contract remains `Component::onRender(sf::RenderWindow&, ...)`.
Phase 12.6.2 therefore provides:

```cpp
composition.execute(window, scene, frame);
```

For every enabled entry, this overload applies the prepared view and invokes
`Scene::render(window, context)`. The existing `RenderQueue2D` then enforces that entry's pass and
layer range. This supports split-screen, minimap-style repeated world rendering, and explicit UI/world
composition without breaking custom `RenderWindow` components.

The bridge does **not** freeze Scene/ECS state for the complete multi-entry run. Each entry invokes
the normal Scene rendering path, so Scene mutations caused by render callbacks follow the existing
Scene dispatch/deferred-destruction rules and can affect later entries where those rules permit it.
Only the composition's entry order, generated contexts, and camera-view snapshots are fixed for one
execution.

## Relationship with RenderPipeline2D

`RenderComposition2D` and `RenderPipeline2D` solve different ordering problems:

- `RenderComposition2D`: camera + viewport + logical pass + layer selection;
- `RenderPipeline2D`: render-target selection + clear + surface inputs/outputs + publication +
  presentation.

A new target-capable renderer can invoke generic composition execution from a pipeline callback. The
legacy Scene composition overload remains backbuffer/`RenderWindow`-bound because changing the
existing Component virtual signature would be a 1.x breaking change.

Composition execution never automatically creates surfaces, clears targets, publishes render
textures, presents windows, or calls `RenderWindow::display()`.

## Failure reporting

`RenderCompositionResult2D` reports a stable failure enum, the number of fully completed enabled
entries, and an optional original entry index.
`renderCompositionFailureName()` exposes stable diagnostic names.

| Failure | Meaning |
| --- | --- |
| `ReentrantExecution` | the same composition is already executing |
| `InvalidFrame` | interpolation alpha is outside finite `[0, 1]` |
| `InvalidCallback` | generic execution received an empty callback |
| `InvalidEntry` | an enabled entry fails structural validation during preflight |
| `CallbackFailed` | a generic callback returned `false` |

Exceptions thrown by a user callback or legacy Scene renderer are not converted into a failure enum;
they propagate after the target-view and execution-state guards restore their incoming state.

## Lifetime and concurrency

The composition borrows cameras and never extends their lifetime. Generic execution also borrows the
target and callback-captured state for the duration of the call. The legacy overload borrows the
window and Scene for the call only.

Configuration, camera mutation, Scene rendering, and composition execution are a single-owner
render-thread/context contract unless an application supplies its own stronger external
synchronization and graphics-context discipline. Concurrent mutation/execution is unsupported.

## Validation

`Lorenzo2DRenderCompositionTests` remains the headless foundation suite covering:

- full-domain, bounded, and invalid layer ranges;
- entry validation and duplicate-name rejection;
- deterministic insertion/reorder behavior;
- bounded capacity and transactional overflow rejection;
- generated frame contexts and copied camera views;
- disabled entries and normalized viewport validation;
- RenderQueue pass preservation and layer-range filtering.

`Lorenzo2DRenderCompositionExecutionTests` runs in the graphics/Xvfb partition and adds coverage for:

- deterministic generic multi-camera execution and original-index reporting;
- disabled-entry skipping;
- camera-view preflight snapshots;
- incoming target-view restoration;
- callback failure and bounded recursive execution;
- mutation rejection while executing;
- exception unwinding;
- invalid frame/callback side-effect isolation;
- the legacy Scene bridge with separate layer ranges and viewports;
- stable execution failure names.

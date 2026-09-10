# Render composition 2D

Phase 12.6.1 introduces the CPU-side contract used to compose cameras, render passes, integer
render layers, and normalized viewports. It deliberately stops before multi-camera execution; the
execution layer is the next Phase 12 slice.

## Render layer ranges

`RenderContext2D` now carries an inclusive `RenderLayerRange2D`. The default range spans the complete
`std::int32_t` domain, so existing rendering remains unchanged unless a caller explicitly narrows the
range.

`RenderQueue2D::build()` first preserves the existing render-pass filter and deterministic sort-key
construction, then rejects entries whose `RenderSortKey2D::layer` falls outside the context range.
Objects without an active `RenderOrder2D` remain in the World pass at layer `0` and are therefore
included by the default range.

An invalid range (`minimum > maximum`) contains no layers. This gives callers a deterministic empty
selection without special-case queue behavior.

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
`moveEntry()` is the only operation that changes an existing entry's relative position.

## Camera ownership and views

Composition entries do not own cameras. The caller must keep every referenced `Camera2D` alive while
its entry can be queried. `RenderComposition2D` never mutates the borrowed camera.

`makeView()` copies the camera's current `sf::View` and changes only the copy's normalized viewport.
This lets one camera remain reusable in another composition entry or in the legacy direct rendering
path.

Viewport coordinates follow SFML's normalized target convention. Position must be non-negative,
size must be positive, all values must be finite, and the resulting rectangle must remain inside the
normalized `[0, 1] x [0, 1]` target area.

## Frame context

`RenderCompositionFrame2D` supplies interpolation alpha and the optional coordinate projection for a
composition query. Alpha must be finite and in `[0, 1]`.

`makeContext()` combines that frame state with the selected entry's pass and layer range. Disabled
entries and invalid frame state return `std::nullopt`.

`RenderContext2D` now has an explicit constructor whose first three parameters match the historical
brace-initialization order (`interpolationAlpha`, `projection`, `pass`), with the layer range as an
optional fourth parameter. This preserves existing 1.x source call sites while avoiding partial
aggregate-initializer warnings under warnings-as-errors builds.

## What Phase 12.6.1 does not do

This slice does not iterate a composition and render it automatically. It also does not own render
targets, clear targets, present windows, inject cameras into `RenderPipeline2D`, or perform automatic
layer/pass scheduling.

Those execution concerns belong to Phase 12.6.2. Keeping them separate makes the configuration
contract independently testable and prevents the first multi-camera implementation from silently
changing existing Scene, RenderQueue, or RenderPipeline ordering semantics.

## Validation

`Lorenzo2DRenderCompositionTests` is a headless regression suite covering:

- full-domain, bounded, and invalid layer ranges;
- entry validation and duplicate-name rejection;
- deterministic insertion/reorder behavior;
- bounded capacity and transactional overflow rejection;
- generated frame contexts and copied camera views;
- disabled entries;
- normalized viewport validation;
- RenderQueue pass preservation and layer-range filtering.

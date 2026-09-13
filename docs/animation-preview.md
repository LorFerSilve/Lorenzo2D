# Animation preview foundation

Phase 13.7 adds a bounded editor-only animation preview model without adding editor dependencies to Lorenzo2D runtime modules.

## Boundary

`AnimationPreviewModel` lives in the standalone `tools/editor` project and consumes only installed public engine contracts: `AnimationClip`, `AssetId`, serialized `AnimatorPrefab` state, and `EditorDocument` selection/identity. The engine does not depend on this model.

The model intentionally copies a bound `AnimationClip`. Preview lifetime therefore does not borrow runtime asset-manager storage and cannot outlive an asset-manager-owned clip by accident. Binding is accepted only when the currently selected object has an `AnimatorPrefab` that references the supplied clip asset ID.

## Preview contract

The model provides:

- deterministic clip enumeration in serialized animator order;
- binding of a referenced clip or the animator's initial clip;
- play, pause, stop, seek, and signed frame stepping;
- configurable playback speed from `0x` through `16x`;
- looping and finite non-looping completion behavior;
- a value-only snapshot containing object/clip identity, source rectangle, frame index/count, elapsed time, duration, speed, and playback state;
- automatic invalidation when selection changes, the selected object identity changes, the Animator component disappears, or the bound clip is no longer referenced.

Preview clips are capped at 4,096 frames. Cursor resolution is therefore bounded even for a large time jump, and looping playback uses modulo time rather than repeatedly advancing frame-by-frame through every elapsed loop.

## Deliberate non-goals

Phase 13.7 does not introduce a second animation asset format, editor-owned asset registry, animation graph/state machine, keyframe editor, sprite-texture loader, or runtime simulation mode. Texture resolution remains the application's/content pipeline's responsibility, consistent with the Phase 13 asset-browser boundary. Play/test workflow is the next separate editor dependency.

## Validation

`Lorenzo2DEditorAnimationPreviewTests` is built and executed against the installed Lorenzo2D package. Regressions cover serialized clip ordering, initial-clip binding, playback timing and speed, seek/frame stepping, non-looping completion, stale selection/binding invalidation, and the preview workload limits.

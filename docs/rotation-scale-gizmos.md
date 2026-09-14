# Rotation and scale gizmos

Phase 13.10 extends the editor-only viewport transform foundation with rotation and scale gestures while preserving the transaction and dependency boundaries established by Phase 13.3.

## Architecture

`ViewportTransformModel` remains part of the standalone editor package. It consumes public Lorenzo2D transform/prefab contracts through `EditorDocument`; engine modules do not depend on editor code.

Translation, rotation, and scale all use the same exclusive `EditorCommandHistory` coalesced-command mechanism. A drag may publish multiple live transform updates, but a successful gesture commits exactly one undo entry. Cancelling a gesture restores the complete document snapshot captured when the gesture began, including selection and pre-existing redo history.

`EditorDocument::setObjectTransform()` remains the publication boundary, so runtime `Prefab` validation is still authoritative for the final transform state.

## Handles

A selected object exposes five transform interaction points in viewport coordinates:

- center: translation;
- upper rotation handle: rotation;
- positive-X handle: X-only scale;
- positive-Y handle: Y-only scale;
- diagonal handle: uniform scale.

Handle geometry is expressed in viewport pixels rather than world units. It therefore remains usable at different editor zoom levels while the transformed values continue to use the runtime world-space `TransformState` contract.

## Rotation semantics

Rotation is derived from the signed angle between successive pointer vectors around the object's transform origin. Per-update angular deltas are wrapped across the `-pi`/`pi` discontinuity and accumulated for the lifetime of the gesture. This permits continuous drags beyond one complete revolution without a 360-degree jump.

Position and scale remain unchanged by a rotation gesture.

## Scale semantics

Scale gestures are multiplicative relative to the pointer projection captured when the drag starts:

- X scale projects along the viewport X axis;
- Y scale projects along the viewport Y axis;
- uniform scale projects along the positive viewport diagonal and applies the same factor to both scale components.

This prevents the first pointer update from snapping merely because the drag began near the edge of a handle rather than at its exact center. Near-zero and excessively large interactive scale values are rejected before publication; runtime prefab validation still validates every accepted update.

Position and rotation remain unchanged by scale gestures. Uniform scaling preserves the existing X:Y scale ratio.

## Gesture safety

Only one viewport transform gesture can be open at a time. While it is open:

- another translation, rotation, or scale gesture cannot begin;
- ordinary history execute/undo/redo operations remain blocked by the existing coalesced-command contract;
- viewport bounds and camera view cannot change;
- changing the selected object invalidates the gesture and rolls the document back to its start snapshot;
- Escape-style cancellation can use `cancelActiveDrag()` without needing to know the active transform mode.

A gesture that returns to its exact start transform is treated as a no-op and does not enter history.

## Validation

The installed-package editor regression suite covers:

- deterministic handle placement and hit testing;
- continuous multi-turn rotation;
- X-only, Y-only, and uniform scaling;
- transform-component isolation;
- one undo command per completed gesture;
- undo/redo restoration;
- cancellation and no-op suppression;
- redo preservation;
- selection-identity invalidation;
- cross-mode gesture exclusivity;
- rejection of unsafe near-zero interactive scale updates.

## Next slice

With translation, rotation, and scale authoring sharing one deterministic viewport/history contract, the remaining Phase 13 dependency is **Phase 13.11 — end-to-end authoring tutorial and Phase 13 completion evidence**. It should demonstrate creating, editing, saving, and play-testing a small level without hand-editing JSON while preserving runtime-only package consumption.

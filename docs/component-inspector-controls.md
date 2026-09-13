# Component-specific inspector controls

Phase 13.9 extends the standalone editor's `ComponentInspectorModel` with typed property controls for the serialized built-in components that previously required callers to use the generic `editSelected()` escape hatch.

## Boundary

The controls remain editor-only. Lorenzo2D runtime modules do not depend on editor code, and the editor does not introduce a second component schema. Every property edit resolves the current stable editor selection, mutates a copy of the selected public `Prefab`, and publishes it through `EditorDocument::replaceObject()` and `EditorCommandHistory`.

Consequently `l2d::isValidPrefab()` remains authoritative. Invalid values or dependency-breaking edits are rejected transactionally, restore the exact prior document state, and do not enter undo history. Reapplying the current value is a no-op and likewise creates no history entry.

## Typed controls

The Phase 13.9 surface covers:

- `RectangleRenderer`: size and color;
- `CircleRenderer`: radius and color;
- `SpriteRenderer`: size, color, origin, horizontal/vertical flip, and render-order state;
- `Animator`: playback speed, playing state, clip removal, and clearing the initial clip;
- `RigidBody2D`: body type, velocity, acceleration, mass, gravity enablement, and gravity scale.

Asset selection continues to use the Phase 13.4 asset-picking boundary. Collider/navigation editing continues to use the dedicated Phase 13.6 authoring model. This avoids duplicating those validation and visualization responsibilities in the generic inspector.

## Invariants

Component-specific setters only edit components already present on the selected `Prefab`. Component creation/removal remains explicit through `addComponent()` / `removeComponent()` or asset picking where configuration is required.

Animator clip removal preserves runtime validity: removing the current initial clip clears `initialClip`; removing the final remaining clip is rejected by runtime validation because an `AnimatorPrefab` must contain at least one clip.

Rigid-body edits are also checked against dependent gameplay components. For example, changing a body used by `CharacterMotor2D` away from `Kinematic` is rejected by the existing `Prefab` contract.

## Validation

Installed-package component-inspector regressions exercise successful typed mutations, undo/redo, no-op suppression, invalid numeric rollback, animator clip invariants, and rigid-body dependency rejection. The editor core still links only against the installed public `Lorenzo2D::Lorenzo2D` package.

## Next slice

With component-specific property mutation no longer dependent on untyped callbacks, the next planned Phase 13 dependency is **Phase 13.10 — rotation/scale gizmos**. It should extend the existing coalesced viewport gesture contract rather than introducing a second history mechanism.

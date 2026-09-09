# Public API and failure-path audit

Phase 11.8 audits the installed Lorenzo2D 1.x surface for ownership, lifetime, mutability, validation,
transactionality, workload bounds, units, and concurrency assumptions.

## Scope

The audit covers all **77 public headers** under `include/Lorenzo2D/` that are installed by the
package, plus the generated public version header. Private implementation headers, test helpers,
benchmark internals, and sandbox-only code are outside the compatibility surface.

The review categories are the Phase 11 roadmap requirements:

- ownership and borrowed lifetimes;
- accidental mutability;
- silent fallback or rejection;
- validation consistency;
- units and coordinate spaces;
- exception safety and transactionality;
- workload limits;
- concurrency assumptions.

No public symbol was removed or incompatibly renamed. Corrections use additive APIs, stronger
compile-time rejection of invalid temporary lifetimes, internal rollback hardening, and explicit
contract documentation.

## Findings and dispositions

### Borrowed input snapshots

`InputMap` and `InputContextStack` store a non-owning pointer to an `InputSnapshot`. Their
previous `const InputSnapshot&` constructors/setters allowed a temporary to bind, leaving a dangling
pointer immediately after the full expression.

Phase 11.8:

- deletes rvalue snapshot constructor overloads;
- deletes rvalue `setSnapshot()` overloads;
- adds `hasSnapshot()`;
- adds `clearSnapshot()`;
- propagates detach through every map owned by `InputContextStack`.

The snapshot source must outlive the map/stack while attached. Application-owned
`Input::snapshot()` remains the normal long-lived source.

### Legacy ActionMap rejection

`ActionMap` is a compatibility adapter whose original `void bindAction()` and
`void clearAction()` discarded the checked `InputMap` result.

Phase 11.8 adds:

- `tryBindAction()`;
- `tryClearAction()`.

The legacy void methods remain source compatible and delegate to the checked variants. New code
should normally use `InputMap` directly; code that must retain the adapter can now observe invalid
names/keys.

### Projection fallback

`RenderContext2D::worldToRender()`, `renderToWorld()`, and `depthFor()` intentionally retained
safe presentation fallbacks when a custom projection returned a non-finite value. That behavior is
useful for compatibility but previously made projection failure indistinguishable from a legitimate
identity/zero result.

Phase 11.8 adds:

- `tryWorldToRender()`;
- `tryRenderToWorld()`;
- `tryDepthFor()`.

The checked variants return `std::nullopt` for non-finite input/output. Existing methods preserve
their previous fallback behavior.

### Camera rejection

Several `Camera2D` methods rejected unsafe coordinates or time deltas while returning `void`.

Phase 11.8 adds checked variants:

- `trySetCenter()`;
- `tryMove()`;
- `tryFollow()`;
- `trySetBounds()`.

Existing setters remain compatibility adapters. Size and zoom continue to use their documented
sanitizing/clamping policy rather than rejection.

### Level instantiation transactionality

Level parsing was already transactional, but scene instantiation had two failure-path weaknesses:

1. the asset-free batch overload could leave objects created before a later prefab failed;
2. the asset-aware rollback called `Scene::destroyQueuedGameObjects()`, which could also sweep
   unrelated objects that had been queued before instantiation.

Phase 11.8 records the pre-call set of stable `GameObjectId` values and uses an internal
identity-based rollback:

- every Scene object created after the snapshot is removed/queued if the level call fails;
- rollback remains correct if a custom decoder destroys/sweeps pre-existing objects and compacts the
  Scene container;
- objects that existed before the call are not swept merely because level instantiation failed;
- both asset-free and asset-aware batch overloads use the same policy.

During an active Scene dispatch, physical destruction remains deferred until the normal dispatch
boundary. This preserves the Scene mutation rules.

A custom component decoder can still cause arbitrary side effects outside Scene object creation
(for example by modifying external state captured by the decoder, or explicitly destroying an
existing Scene object). Lorenzo2D cannot generically undo or resurrect such external/pre-existing
effects. Decoder authors are responsible for making those side effects transactional when required.

### Borrowed Scene and component references

`Scene::createGameObject()`, Scene lookup functions, and GameObject component lookup functions
return borrowed references/pointers. They are not owning handles.

The public headers now state that:

- Scene object pointers/references become invalid when the object is destroyed or the Scene clears;
- component pointers/references become invalid when their owning GameObject is destroyed;
- `GameObjectHandle` is the intended cross-frame/deferred identity mechanism.

### Asset pipeline lifetime and threading

`AssetPipeline` borrows the `AssetManager` passed to its constructor. The manager must outlive
the pipeline.

Background image decoding may execute on worker threads. Pipeline state, watching, scanning, event
access, dependency mutation, and `poll()` remain caller-thread operations. `poll()` must execute
on the thread/context allowed to publish SFML GPU resources.

The engine does not claim that arbitrary public objects are safe for concurrent mutation unless a
specific API explicitly documents that property.

### TileMapData mutable escape hatches

`TileMapData::layer()` and mutable `layers()` predate the stricter checked mutation surface.
They can bypass `addLayer()`, `replaceLayer()`, and per-layer dimensional validation.

Removing them would be a breaking 1.x change, so they remain available with an explicit contract:

- prefer checked mutators for new code;
- direct mutable access makes the caller responsible for restoring invariants;
- call `isValid()` before publishing/consuming externally-mutated data.

This is a candidate cleanup for a future major version rather than a 1.x removal.

## Validation and failure model

The public surface deliberately uses several failure styles. They are not interchangeable:

| Style | Intended use |
| --- | --- |
| `bool` | validation/mutation that can reject normal caller input without throwing |
| `std::optional<T>` | lookup/query where absence or checked conversion failure is expected |
| status/result struct | operations needing both success state and diagnostic/output data |
| exception | constructor/configuration or batch-instantiation failure where no useful result can be returned |
| sanitizing setter | legacy/presentation APIs with an explicitly documented finite bounded replacement |
| compatibility fallback | retained only where changing old observable behavior would break 1.x callers; checked alternative is provided when ambiguity matters |

Parsers and importers that write into a destination object preserve the previous destination on
malformed/oversized input. File save replacement uses the documented temporary/backup policy.

## Units and coordinate spaces

The audit found no unresolved unit ambiguity in the documented major subsystems:

- engine time values are seconds unless a field explicitly says milliseconds;
- physics rotations exposed in game-facing APIs are degrees where named `rotationDegrees`; Transform
  rotation follows the existing degree-based SFML-facing contract;
- navigation and physics operate in Cartesian world coordinates;
- UI pointer positions are screen/pixel coordinates;
- projection APIs explicitly convert world <-> render coordinates;
- tile stream regions use tile coordinates;
- profiler report values are milliseconds.

New APIs should continue naming units in identifiers whenever the type alone is ambiguous.

## Concurrency contract

Unless a type explicitly states otherwise, Lorenzo2D public objects use **single-owner-thread
mutation**. In particular:

- Scene/ECS mutation and rendering are not concurrently mutable;
- physics worlds/query-context construction are coordinated by the game loop;
- input snapshots are sampled/published by the application flow;
- UI/audio service mutation is not a general concurrent API;
- AssetPipeline background work is limited to its documented decode stage;
- immutable snapshots/leases may be copied where their owning type documents that behavior.

A future async content/save subsystem must define publication and shutdown lifetime explicitly rather
than relying on incidental container/thread safety.

## Compatibility items intentionally retained

The audit does not remove the following 1.x behavior:

- legacy `ActionMap`, `Input`, and `Mouse` adapters;
- mutable `TileMapData` layer escape hatches;
- raw borrowed Scene/GameObject query pointers;
- sanitizing legacy numeric setters;
- compatibility projection fallbacks.

These are documented compatibility debt, not newly-endorsed design patterns. Phase 22 may reassess
them with migration guidance.

## Regression evidence

`Lorenzo2DApiFailureAuditTests` locks down:

- temporary-snapshot construction rejection;
- snapshot attach/detach propagation;
- checked ActionMap failure reporting;
- checked RenderContext projection failures;
- checked Camera2D rejection;
- asset-free batch instantiation rollback;
- preservation of an unrelated pre-existing Scene destroy queue during codec failure;
- identity-stable rollback when a decoder sweeps that queue, compacts the Scene, creates another
  Scene object, and then rejects the component.

The installed-package and add-subdirectory consumers also compile and execute the additive checked
APIs.

See [phase11-production-baseline.md](phase11-production-baseline.md) for workload limits and the
measured Phase 11 validation evidence.

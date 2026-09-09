# Input actions, contexts, controllers, and pointers

Lorenzo2D samples input once per rendered frame into an `InputSnapshot`. Gameplay code binds named
actions to that snapshot instead of polling SFML directly. Continuous state remains visible to all
fixed ticks in the frame; a pressed or released edge can be consumed only once by each `InputMap`.

## Input codes

`InputCode` identifies a device, device index, control code, and control type. Factory functions
cover physical keyboard scancodes, logical keyboard labels, mouse buttons, gamepad buttons and
axes, and touch contacts.

Physical scancodes are recommended for position-based controls. A saved binding for the physical
movement cluster remains in the same place when the operating-system layout changes between QWERTY
and AZERTY. Logical keys are appropriate when the printed character itself is the intended control
and are also retained by the compatibility adapters.

```cpp
const l2d::InputCode physicalW =
    l2d::Input::physicalKey(sf::Keyboard::Scancode::W);
const l2d::InputCode logicalZ =
    l2d::Input::logicalKey(sf::Keyboard::Key::Z);
const l2d::InputCode gamepadX =
    l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::X);
```

Invalid device indices, control kinds, non-finite axis values, and out-of-range values are rejected
or clamped before publication. Disconnecting a gamepad immediately clears all of its controls so a
button or axis cannot remain stuck; reconnecting starts from neutral state.

## Typed actions

Attach an `InputMap` to the application-owned snapshot and bind buttons or axes. The map borrows
the snapshot: the source object must outlive the map while attached. Temporary snapshots are
rejected at compile time. `hasSnapshot()` and `clearSnapshot()` make attachment explicit; an
`InputContextStack` applies the same borrowed lifetime to every owned map.

Bind buttons or axes:

```cpp
l2d::InputMap gameplay(l2d::Input::snapshot());

gameplay.bindButton(
    "jump", l2d::Input::physicalKey(sf::Keyboard::Scancode::Space));
gameplay.bindButton("jump", l2d::InputCode::gamepadButton(0u, 0u));

gameplay.bindAxis2D(
    "move",
    l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
    l2d::Input::physicalKey(sf::Keyboard::Scancode::D),
    l2d::Input::physicalKey(sf::Keyboard::Scancode::W),
    l2d::Input::physicalKey(sf::Keyboard::Scancode::S));

gameplay.bindAxis2D(
    "move",
    l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::X),
    l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::Y),
    0.18f);
```

Several bindings may feed one action. Button edges are calculated for the aggregate action: adding
a second held key does not retrigger an already-held action. `axis1D` and `axis2D` combine digital
and analog bindings, apply configurable analog deadzones, clamp their result, and optionally
normalize diagonals.

Use `down` for continuous state. Use `consumePressed` or `consumeReleased` for commands handled in a
fixed tick:

```cpp
const sf::Vector2f movement = gameplay.axis2D("move");
if (gameplay.consumePressed("jump"))
{
    // This branch can run in at most one fixed tick for this rendered frame.
}
```

## Context stack

`InputContextStack` owns maps named for modes such as `gameplay`, `menu`, `console`, or
`placement`. Resolution starts at the top. A blocking context prevents lower maps from observing an
action even when the top context does not bind that action. A non-blocking overlay can add shortcuts
while allowing gameplay beneath it.

```cpp
l2d::InputContextStack contexts(l2d::Input::snapshot());
l2d::InputMap& gameplay = contexts.createContext("gameplay");
l2d::InputMap& menu = contexts.createContext("menu");

contexts.pushContext("gameplay");
contexts.pushContext("menu", true); // blocks gameplay until popped
```

## Pointer model

`Pointer` unifies the primary mouse button and touch contacts as `PointerState`. Every state exposes
an ID, device, screen position, frame delta, down/pressed/released flags, vertical and horizontal
wheel deltas, and drag begin/update/end state. Dragging starts after a configurable pixel threshold;
Core does not attach selection, placement, or navigation behavior to it.

World conversion always accepts the view that owns the gameplay presentation:

```cpp
const l2d::PointerState& pointer = l2d::Pointer::primary();
const sf::Vector2f world = l2d::Pointer::worldPosition(window, camera.view());
```

The conversion accounts for camera transforms, viewports, and the current render-target size.

## Compatibility window

`Input`, `Mouse`, and `ActionMap` remain available as adapters for the 0.5 line. Their limited
`Key` and `MouseButton` enums are deprecated by policy but do not emit compiler diagnostics yet,
because the project treats warnings as errors. `ActionMap::tryBindAction()` and
`tryClearAction()` expose invalid compatibility bindings; the original void methods retain their
legacy ignore-the-result behavior. New game code should use `InputCode`, `InputMap`,
`InputContextStack`, and `Pointer`.

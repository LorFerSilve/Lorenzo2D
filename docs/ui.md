# UI

Lorenzo2D 1.0 provides a small game-facing screen-space UI layer through
`UiCanvas2D`. It is deliberately not a retained editor toolkit: it covers the
runtime menu/HUD interaction baseline required by the four project templates.

## Buttons and coordinates

`UiButton2D` stores an ID, label, screen-space position and size, visual style,
and enabled/visible state. Button IDs must be non-empty and bounds must be finite
with strictly positive dimensions.

Buttons are evaluated in insertion order and hit-tested from back to front, so a
later overlapping button is the topmost interactive control.

```cpp
l2d::UiCanvas2D ui;

l2d::UiButton2D button;
button.id = "resume";
button.label = "Resume";
button.position = {40.f, 40.f};
button.size = {180.f, 48.f};

if (!ui.addButton(button))
    throw std::runtime_error("invalid UI button");
```

## Pointer interaction

Call `update(Pointer::primary())` once per rendered frame after application
events have been sampled. A button activates only when the pointer is pressed on
that button and released over the same button. Dragging or releasing outside does
not activate it.

`wasActivated(id)` is frame-scoped: the activation list is cleared on the next
`update()`.

```cpp
void onFrameStart(float) override
{
    ui.update(l2d::Pointer::primary());

    if (ui.wasActivated("resume"))
        resumeGame();
}
```

Disabling or hiding the currently hovered/pressed button cancels the interaction.
`cancelInteraction()` is available for menu/context switches.

## Rendering

`render(window, font)` temporarily selects the window's default view, draws the
canvas in screen space, and restores the previous view before returning. Passing
an empty `FontHandle` still renders button rectangles but omits labels.

This makes UI independent from the active world camera or isometric projection.

## Scope and exclusions

The 1.0 UI baseline intentionally includes buttons, styling, hit-testing,
press/release capture, and text rendering. Layout containers, text input,
accessibility navigation, localization, gamepad focus traversal, and a visual UI
editor remain future extensions.

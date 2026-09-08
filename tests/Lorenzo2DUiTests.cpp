#include "TestSupport.hpp"

#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <limits>
#include <optional>
#include <string>

namespace
{
    using l2d::test::runTest;

    l2d::PointerState pointerAt(int x, int y)
    {
        l2d::PointerState pointer;
        pointer.screenPosition = {x, y};
        return pointer;
    }

    void testValidationAndMutation()
    {
        l2d::UiCanvas2D canvas;
        l2d::UiButton2D button;
        button.id = "play";
        button.label = "Play";
        button.position = {10.f, 20.f};
        button.size = {120.f, 40.f};

        L2D_REQUIRE(canvas.addButton(button));
        L2D_REQUIRE(!canvas.addButton(button));
        L2D_REQUIRE(canvas.buttonCount() == 1u);
        L2D_REQUIRE(canvas.hasButton("play"));
        L2D_REQUIRE(canvas.setButtonLabel("play", "Resume"));
        L2D_REQUIRE(canvas.setButtonBounds("play", {20.f, 30.f}, {140.f, 44.f}));
        L2D_REQUIRE(!canvas.setButtonBounds("play", {20.f, 30.f}, {0.f, 44.f}));

        l2d::UiButton2D invalid = button;
        invalid.id = "bad";
        invalid.position.x = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!canvas.addButton(invalid));
        L2D_REQUIRE(canvas.removeButton("play"));
        L2D_REQUIRE(canvas.buttonCount() == 0u);
    }

    void testPressReleaseActivationIsFrameScoped()
    {
        l2d::UiCanvas2D canvas;
        l2d::UiButton2D button;
        button.id = "save";
        button.position = {100.f, 100.f};
        button.size = {80.f, 30.f};
        L2D_REQUIRE(canvas.addButton(button));

        l2d::PointerState press = pointerAt(110, 110);
        press.down = true;
        press.pressed = true;
        canvas.update(press);
        L2D_REQUIRE(canvas.pressedButton() == std::optional<std::string>("save"));
        L2D_REQUIRE(!canvas.wasActivated("save"));

        l2d::PointerState release = pointerAt(120, 112);
        release.released = true;
        canvas.update(release);
        L2D_REQUIRE(canvas.wasActivated("save"));
        L2D_REQUIRE(!canvas.pressedButton().has_value());

        canvas.update(pointerAt(120, 112));
        L2D_REQUIRE(!canvas.wasActivated("save"));
    }

    void testReleaseOutsideDoesNotActivate()
    {
        l2d::UiCanvas2D canvas;
        l2d::UiButton2D button;
        button.id = "quit";
        button.position = {0.f, 0.f};
        button.size = {50.f, 50.f};
        L2D_REQUIRE(canvas.addButton(button));

        l2d::PointerState press = pointerAt(25, 25);
        press.down = true;
        press.pressed = true;
        canvas.update(press);

        l2d::PointerState release = pointerAt(100, 100);
        release.released = true;
        canvas.update(release);
        L2D_REQUIRE(!canvas.wasActivated("quit"));
    }

    void testTopmostAndDisabledButtons()
    {
        l2d::UiCanvas2D canvas;
        l2d::UiButton2D bottom;
        bottom.id = "bottom";
        bottom.position = {0.f, 0.f};
        bottom.size = {100.f, 100.f};
        l2d::UiButton2D top = bottom;
        top.id = "top";

        L2D_REQUIRE(canvas.addButton(bottom));
        L2D_REQUIRE(canvas.addButton(top));

        l2d::PointerState hover = pointerAt(50, 50);
        canvas.update(hover);
        L2D_REQUIRE(canvas.hoveredButton() == std::optional<std::string>("top"));

        L2D_REQUIRE(canvas.setButtonEnabled("top", false));
        canvas.update(hover);
        L2D_REQUIRE(canvas.hoveredButton() == std::optional<std::string>("bottom"));

        L2D_REQUIRE(canvas.setButtonVisible("bottom", false));
        canvas.update(hover);
        L2D_REQUIRE(!canvas.hoveredButton().has_value());
    }
}

int main()
{
    int failures = 0;
    runTest("ui validation and mutation", testValidationAndMutation, failures);
    runTest("ui press release activation", testPressReleaseActivationIsFrameScoped, failures);
    runTest("ui release outside", testReleaseOutsideDoesNotActivate, failures);
    runTest("ui topmost disabled handling", testTopmostAndDisabledButtons, failures);
    return failures == 0 ? 0 : 1;
}

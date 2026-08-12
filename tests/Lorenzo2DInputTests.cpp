#include "ActionState.hpp"
#include "TestSupport.hpp"

#include <Lorenzo2D/Core/InputContextStack.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/View.hpp>

#include <cmath>
#include <limits>

namespace
{
    using l2d::test::runTest;

    constexpr float EPSILON = 0.0001f;

    l2d::InputCode key(sf::Keyboard::Scancode scancode)
    {
        return l2d::InputCode::keyboard(scancode);
    }

    void testActionStateUsesAggregateTransitions()
    {
        using l2d::core_detail::ActionState;

        ActionState idle;
        idle.include(false, false);
        L2D_REQUIRE(!idle.isPressed());
        L2D_REQUIRE(!idle.wasPressed());
        L2D_REQUIRE(!idle.wasReleased());

        ActionState firstKeyPressed;
        firstKeyPressed.include(true, false);
        firstKeyPressed.include(false, false);
        L2D_REQUIRE(firstKeyPressed.isPressed());
        L2D_REQUIRE(firstKeyPressed.wasPressed());
        L2D_REQUIRE(!firstKeyPressed.wasReleased());

        ActionState secondKeyPressedWhileFirstHeld;
        secondKeyPressedWhileFirstHeld.include(true, true);
        secondKeyPressedWhileFirstHeld.include(true, false);
        L2D_REQUIRE(secondKeyPressedWhileFirstHeld.isPressed());
        L2D_REQUIRE(!secondKeyPressedWhileFirstHeld.wasPressed());
        L2D_REQUIRE(!secondKeyPressedWhileFirstHeld.wasReleased());

        ActionState finalKeyReleased;
        finalKeyReleased.include(false, true);
        finalKeyReleased.include(false, false);
        L2D_REQUIRE(!finalKeyReleased.isPressed());
        L2D_REQUIRE(!finalKeyReleased.wasPressed());
        L2D_REQUIRE(finalKeyReleased.wasReleased());
    }

    void testInputCodesCoverPhysicalLayoutsAndDevices()
    {
        const l2d::InputCode physicalW = key(sf::Keyboard::Scancode::W);
        const l2d::InputCode physicalZ = key(sf::Keyboard::Scancode::Z);
        const l2d::InputCode logicalW = l2d::InputCode::keyboardLogical(sf::Keyboard::Key::W);
        const l2d::InputCode mouse = l2d::InputCode::mouse(sf::Mouse::Button::Extra2);
        const l2d::InputCode gamepad = l2d::InputCode::gamepadAxis(3u, sf::Joystick::Axis::PovX);
        const l2d::InputCode touch = l2d::InputCode::touch(42u);

        L2D_REQUIRE(physicalW.isValid());
        L2D_REQUIRE(physicalZ.isValid());
        L2D_REQUIRE(physicalW != physicalZ);
        L2D_REQUIRE(logicalW.isValid());
        L2D_REQUIRE(physicalW != logicalW);
        L2D_REQUIRE(mouse.isValid());
        L2D_REQUIRE(gamepad.isValid());
        L2D_REQUIRE(touch.isValid());

        l2d::InputCode invalid = gamepad;
        invalid.deviceIndex = static_cast<std::uint16_t>(sf::Joystick::Count);
        L2D_REQUIRE(!invalid.isValid());
    }

    void testButtonBindingsUseAggregateTransitions()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);
        const l2d::InputCode primary = key(sf::Keyboard::Scancode::Space);
        const l2d::InputCode alternate = key(sf::Keyboard::Scancode::Up);

        L2D_REQUIRE(inputMap.bindButton("jump", primary));
        L2D_REQUIRE(inputMap.bindButton("jump", alternate));
        L2D_REQUIRE(inputMap.bindButton("jump", primary));

        snapshot.beginFrame();
        L2D_REQUIRE(snapshot.setButton(primary, true));
        L2D_REQUIRE(inputMap.down("jump"));
        L2D_REQUIRE(inputMap.pressed("jump"));

        snapshot.beginFrame();
        L2D_REQUIRE(snapshot.setButton(alternate, true));
        L2D_REQUIRE(inputMap.down("jump"));
        L2D_REQUIRE(!inputMap.pressed("jump"));

        L2D_REQUIRE(snapshot.setButton(primary, false));
        L2D_REQUIRE(inputMap.down("jump"));
        L2D_REQUIRE(!inputMap.released("jump"));

        snapshot.beginFrame();
        L2D_REQUIRE(snapshot.setButton(alternate, false));
        L2D_REQUIRE(!inputMap.down("jump"));
        L2D_REQUIRE(inputMap.released("jump"));
    }

    void testFastTapRetainsBothEdges()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);
        const l2d::InputCode touch = l2d::InputCode::touch(7u);
        inputMap.bindButton("select", touch);

        snapshot.beginFrame();
        snapshot.setButton(touch, true);
        snapshot.setButton(touch, false);

        L2D_REQUIRE(!inputMap.down("select"));
        L2D_REQUIRE(inputMap.pressed("select"));
        L2D_REQUIRE(inputMap.released("select"));
    }

    void testDigitalAxesSupportWasdAndZqsd()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);

        L2D_REQUIRE(inputMap.bindAxis2D(
            "wasd", key(sf::Keyboard::Scancode::A), key(sf::Keyboard::Scancode::D),
            key(sf::Keyboard::Scancode::W), key(sf::Keyboard::Scancode::S)));
        L2D_REQUIRE(inputMap.bindAxis2D(
            "zqsd", key(sf::Keyboard::Scancode::Q), key(sf::Keyboard::Scancode::D),
            key(sf::Keyboard::Scancode::Z), key(sf::Keyboard::Scancode::S)));

        snapshot.beginFrame();
        snapshot.setButton(key(sf::Keyboard::Scancode::D), true);
        snapshot.setButton(key(sf::Keyboard::Scancode::W), true);
        snapshot.setButton(key(sf::Keyboard::Scancode::Z), true);

        const float diagonal = 1.f / std::sqrt(2.f);
        L2D_REQUIRE_APPROX_2D(inputMap.axis2D("wasd"), sf::Vector2f(diagonal, -diagonal), EPSILON);
        L2D_REQUIRE_APPROX_2D(inputMap.axis2D("zqsd"), sf::Vector2f(diagonal, -diagonal), EPSILON);
    }

    void testAxesApplyDeadzonesAndOptionalNormalization()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);
        const l2d::InputCode horizontal = l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::X);
        const l2d::InputCode vertical = l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::Y);

        snapshot.setDeviceConnected(l2d::InputDeviceType::Gamepad, 0u, true);
        L2D_REQUIRE(inputMap.bindAxis1D("aim_x", horizontal, 0.2f));
        L2D_REQUIRE(inputMap.bindAxis2D("aim", horizontal, vertical, 0.2f, true));
        L2D_REQUIRE(!inputMap.bindButton("aim", key(sf::Keyboard::Scancode::Space)));
        L2D_REQUIRE(!inputMap.bindAxis1D("invalid", horizontal, 1.f));
        L2D_REQUIRE(
            !inputMap.bindAxis1D("nan", horizontal, std::numeric_limits<float>::quiet_NaN()));

        snapshot.beginFrame();
        snapshot.setAxis(horizontal, 0.1f);
        L2D_REQUIRE_APPROX(inputMap.axis1D("aim_x"), 0.f, EPSILON);

        snapshot.setAxis(horizontal, 0.5f);
        L2D_REQUIRE_APPROX(inputMap.axis1D("aim_x"), 0.375f, EPSILON);

        snapshot.setAxis(horizontal, 1.f);
        snapshot.setAxis(vertical, 1.f);
        L2D_REQUIRE_APPROX_2D(inputMap.axis2D("aim"),
                              sf::Vector2f(1.f / std::sqrt(2.f), 1.f / std::sqrt(2.f)), EPSILON);
    }

    void testEdgesAreConsumedOncePerFrame()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);
        const l2d::InputCode jump = key(sf::Keyboard::Scancode::Space);
        inputMap.bindButton("jump", jump);

        snapshot.beginFrame();
        snapshot.setButton(jump, true);

        L2D_REQUIRE(inputMap.consumePressed("jump"));
        L2D_REQUIRE(!inputMap.consumePressed("jump"));
        L2D_REQUIRE(inputMap.down("jump"));
        L2D_REQUIRE(inputMap.down("jump"));

        snapshot.beginFrame();
        L2D_REQUIRE(!inputMap.consumePressed("jump"));
        snapshot.setButton(jump, false);
        L2D_REQUIRE(inputMap.consumeReleased("jump"));
        L2D_REQUIRE(!inputMap.consumeReleased("jump"));
    }

    void testContextStackBlocksLowerGameplay()
    {
        l2d::InputSnapshot snapshot;
        l2d::InputContextStack contexts(snapshot);
        l2d::InputMap& gameplay = contexts.createContext("gameplay");
        l2d::InputMap& overlay = contexts.createContext("overlay");
        l2d::InputMap& menu = contexts.createContext("menu");

        gameplay.bindAxis1D("move_x", key(sf::Keyboard::Scancode::A),
                            key(sf::Keyboard::Scancode::D));
        overlay.bindButton("screenshot", key(sf::Keyboard::Scancode::F12));
        menu.bindButton("confirm", key(sf::Keyboard::Scancode::Enter));

        L2D_REQUIRE(contexts.pushContext("gameplay"));
        snapshot.beginFrame();
        snapshot.setButton(key(sf::Keyboard::Scancode::D), true);
        L2D_REQUIRE_APPROX(contexts.axis1D("move_x"), 1.f, EPSILON);

        L2D_REQUIRE(contexts.pushContext("overlay", false));
        L2D_REQUIRE_APPROX(contexts.axis1D("move_x"), 1.f, EPSILON);

        L2D_REQUIRE(contexts.pushContext("menu", true));
        L2D_REQUIRE_APPROX(contexts.axis1D("move_x"), 0.f, EPSILON);
        L2D_REQUIRE(!contexts.down("move_x"));
        L2D_REQUIRE_EQUAL(*contexts.topContextName(), std::string("menu"));

        L2D_REQUIRE(contexts.popContext("menu"));
        L2D_REQUIRE_APPROX(contexts.axis1D("move_x"), 1.f, EPSILON);
    }

    void testGamepadDisconnectClearsStatesAndReconnects()
    {
        l2d::InputSnapshot snapshot;
        const l2d::InputCode button = l2d::InputCode::gamepadButton(0u, 2u);
        const l2d::InputCode axis = l2d::InputCode::gamepadAxis(0u, sf::Joystick::Axis::X);

        snapshot.beginFrame();
        L2D_REQUIRE(snapshot.setDeviceConnected(l2d::InputDeviceType::Gamepad, 0u, true));
        L2D_REQUIRE(snapshot.setButton(button, true));
        L2D_REQUIRE(snapshot.setAxis(axis, 0.75f));
        L2D_REQUIRE(snapshot.down(button));
        L2D_REQUIRE_APPROX(snapshot.value(axis), 0.75f, EPSILON);

        snapshot.beginFrame();
        L2D_REQUIRE(snapshot.setDeviceConnected(l2d::InputDeviceType::Gamepad, 0u, false));
        L2D_REQUIRE(!snapshot.down(button));
        L2D_REQUIRE(snapshot.released(button));
        L2D_REQUIRE_APPROX(snapshot.value(axis), 0.f, EPSILON);

        snapshot.beginFrame();
        snapshot.setDeviceConnected(l2d::InputDeviceType::Gamepad, 0u, true);
        snapshot.setButton(button, true);
        L2D_REQUIRE(snapshot.pressed(button));
        L2D_REQUIRE(snapshot.down(button));
    }

    void testPointerWorldConversionHandlesCameraViewportAndResize()
    {
        l2d::PointerState pointer;
        pointer.screenPosition = {400, 200};
        sf::View view({100.f, 50.f}, {200.f, 100.f});

        L2D_REQUIRE_APPROX_2D(l2d::Pointer::worldPosition(pointer, {800u, 400u}, view),
                              sf::Vector2f(100.f, 50.f), EPSILON);

        pointer.screenPosition = {800, 400};
        L2D_REQUIRE_APPROX_2D(l2d::Pointer::worldPosition(pointer, {1600u, 800u}, view),
                              sf::Vector2f(100.f, 50.f), EPSILON);

        view.setCenter({500.f, 300.f});
        view.setSize({400.f, 200.f});
        view.setViewport(sf::FloatRect({0.25f, 0.f}, {0.5f, 1.f}));
        pointer.screenPosition = {200, 200};
        L2D_REQUIRE_APPROX_2D(l2d::Pointer::worldPosition(pointer, {800u, 400u}, view),
                              sf::Vector2f(300.f, 300.f), EPSILON);

        L2D_REQUIRE_APPROX_2D(l2d::Pointer::worldPosition(pointer, {0u, 0u}, view),
                              view.getCenter(), EPSILON);
        L2D_REQUIRE(l2d::Pointer::setDragThreshold(8.f));
        L2D_REQUIRE_APPROX(l2d::Pointer::dragThreshold(), 8.f, EPSILON);
        L2D_REQUIRE(!l2d::Pointer::setDragThreshold(-1.f));
    }
}

int main()
{
    int failures = 0;

    runTest("action state uses aggregate transitions", testActionStateUsesAggregateTransitions,
            failures);
    runTest("input codes cover physical layouts and devices",
            testInputCodesCoverPhysicalLayoutsAndDevices, failures);
    runTest("button bindings use aggregate transitions", testButtonBindingsUseAggregateTransitions,
            failures);
    runTest("fast tap retains both edges", testFastTapRetainsBothEdges, failures);
    runTest("digital axes support WASD and ZQSD", testDigitalAxesSupportWasdAndZqsd, failures);
    runTest("axes apply deadzones and optional normalization",
            testAxesApplyDeadzonesAndOptionalNormalization, failures);
    runTest("edges are consumed once per frame", testEdgesAreConsumedOncePerFrame, failures);
    runTest("context stack blocks lower gameplay", testContextStackBlocksLowerGameplay, failures);
    runTest("gamepad disconnect clears states and reconnects",
            testGamepadDisconnectClearsStatesAndReconnects, failures);
    runTest("pointer conversion handles camera viewport and resize",
            testPointerWorldConversionHandlesCameraViewportAndResize, failures);

    if (failures != 0)
    {
        std::cerr << failures << " input test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D input tests passed.\n";
    return 0;
}

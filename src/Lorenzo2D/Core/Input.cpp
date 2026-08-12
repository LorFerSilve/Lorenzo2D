#include <Lorenzo2D/Core/Input.hpp>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Mouse.hpp>

#include <limits>

namespace l2d
{
    InputSnapshot Input::s_snapshot;

    const InputSnapshot& Input::snapshot() noexcept
    {
        return s_snapshot;
    }

    InputCode Input::physicalKey(sf::Keyboard::Scancode scancode) noexcept
    {
        return InputCode::keyboard(scancode);
    }

    InputCode Input::logicalKey(sf::Keyboard::Key key) noexcept
    {
        return InputCode::keyboardLogical(key);
    }

    InputCode Input::code(Key key) noexcept
    {
        return logicalKey(toSfmlKey(key));
    }

    bool Input::isKeyPressed(Key key)
    {
        return s_snapshot.down(code(key));
    }

    bool Input::wasKeyPressed(Key key)
    {
        return s_snapshot.pressed(code(key));
    }

    bool Input::wasKeyReleased(Key key)
    {
        return s_snapshot.released(code(key));
    }

    void Input::reset()
    {
        s_snapshot.reset();
    }

    void Input::beginFrame()
    {
        s_snapshot.beginFrame();
    }

    void Input::processEvent(const sf::Event& event)
    {
        if (const auto* pressed = event.getIf<sf::Event::KeyPressed>())
        {
            s_snapshot.setButton(InputCode::keyboard(pressed->scancode), true);
            s_snapshot.setButton(InputCode::keyboardLogical(pressed->code), true);
            return;
        }

        if (const auto* released = event.getIf<sf::Event::KeyReleased>())
        {
            s_snapshot.setButton(InputCode::keyboard(released->scancode), false);
            s_snapshot.setButton(InputCode::keyboardLogical(released->code), false);
            return;
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            s_snapshot.setButton(InputCode::mouse(pressed->button), true);
            return;
        }

        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
        {
            s_snapshot.setButton(InputCode::mouse(released->button), false);
            return;
        }

        if (const auto* pressed = event.getIf<sf::Event::JoystickButtonPressed>())
        {
            if (pressed->joystickId < sf::Joystick::Count &&
                pressed->button < sf::Joystick::ButtonCount)
            {
                const auto gamepad = static_cast<std::uint16_t>(pressed->joystickId);
                s_snapshot.setDeviceConnected(InputDeviceType::Gamepad, gamepad, true);
                s_snapshot.setButton(InputCode::gamepadButton(gamepad, pressed->button), true);
            }
            return;
        }

        if (const auto* released = event.getIf<sf::Event::JoystickButtonReleased>())
        {
            if (released->joystickId < sf::Joystick::Count &&
                released->button < sf::Joystick::ButtonCount)
            {
                const auto gamepad = static_cast<std::uint16_t>(released->joystickId);
                s_snapshot.setDeviceConnected(InputDeviceType::Gamepad, gamepad, true);
                s_snapshot.setButton(InputCode::gamepadButton(gamepad, released->button), false);
            }
            return;
        }

        if (const auto* began = event.getIf<sf::Event::TouchBegan>())
        {
            if (began->finger <= std::numeric_limits<std::uint16_t>::max())
                s_snapshot.setButton(InputCode::touch(static_cast<std::uint16_t>(began->finger)),
                                     true);
            return;
        }

        if (const auto* ended = event.getIf<sf::Event::TouchEnded>())
        {
            if (ended->finger <= std::numeric_limits<std::uint16_t>::max())
                s_snapshot.setButton(InputCode::touch(static_cast<std::uint16_t>(ended->finger)),
                                     false);
            return;
        }

        if (const auto* disconnected = event.getIf<sf::Event::JoystickDisconnected>())
        {
            if (disconnected->joystickId < sf::Joystick::Count)
                s_snapshot.setDeviceConnected(InputDeviceType::Gamepad,
                                              static_cast<std::uint16_t>(disconnected->joystickId),
                                              false);
        }
    }

    void Input::update()
    {
        for (unsigned int index = 0u; index < sf::Keyboard::ScancodeCount; ++index)
        {
            const auto scancode = static_cast<sf::Keyboard::Scancode>(index);
            s_snapshot.setButton(InputCode::keyboard(scancode),
                                 sf::Keyboard::isKeyPressed(scancode));
        }

        for (unsigned int index = 0u; index < sf::Keyboard::KeyCount; ++index)
        {
            const auto key = static_cast<sf::Keyboard::Key>(index);
            s_snapshot.setButton(InputCode::keyboardLogical(key), sf::Keyboard::isKeyPressed(key));
        }

        for (unsigned int index = 0u; index < sf::Mouse::ButtonCount; ++index)
        {
            const auto button = static_cast<sf::Mouse::Button>(index);
            s_snapshot.setButton(InputCode::mouse(button), sf::Mouse::isButtonPressed(button));
        }

        sf::Joystick::update();
        for (unsigned int gamepad = 0u; gamepad < sf::Joystick::Count; ++gamepad)
        {
            const auto gamepadIndex = static_cast<std::uint16_t>(gamepad);
            const bool connected = sf::Joystick::isConnected(gamepad);
            s_snapshot.setDeviceConnected(InputDeviceType::Gamepad, gamepadIndex, connected);
            if (!connected) continue;

            const unsigned int buttonCount = sf::Joystick::getButtonCount(gamepad);
            for (unsigned int button = 0u; button < sf::Joystick::ButtonCount; ++button)
            {
                const bool down =
                    button < buttonCount && sf::Joystick::isButtonPressed(gamepad, button);
                s_snapshot.setButton(InputCode::gamepadButton(gamepadIndex, button), down);
            }

            for (unsigned int axisIndex = 0u; axisIndex < sf::Joystick::AxisCount; ++axisIndex)
            {
                const auto axis = static_cast<sf::Joystick::Axis>(axisIndex);
                const float value = sf::Joystick::hasAxis(gamepad, axis)
                                        ? sf::Joystick::getAxisPosition(gamepad, axis) / 100.f
                                        : 0.f;
                s_snapshot.setAxis(InputCode::gamepadAxis(gamepadIndex, axis), value);
            }
        }
    }

    sf::Keyboard::Key Input::toSfmlKey(Key key)
    {
        switch (key)
        {
        case Key::Z:
            return sf::Keyboard::Key::Z;

        case Key::Q:
            return sf::Keyboard::Key::Q;

        case Key::S:
            return sf::Keyboard::Key::S;

        case Key::D:
            return sf::Keyboard::Key::D;

        case Key::W:
            return sf::Keyboard::Key::W;

        case Key::A:
            return sf::Keyboard::Key::A;

        case Key::Space:
            return sf::Keyboard::Key::Space;

        case Key::Escape:
            return sf::Keyboard::Key::Escape;

        case Key::Left:
            return sf::Keyboard::Key::Left;

        case Key::Right:
            return sf::Keyboard::Key::Right;

        case Key::Up:
            return sf::Keyboard::Key::Up;

        case Key::Down:
            return sf::Keyboard::Key::Down;

        case Key::F1:
            return sf::Keyboard::Key::F1;

        case Key::Unknown:
        case Key::Count:
        default:
            return sf::Keyboard::Key::Unknown;
        }
    }

}

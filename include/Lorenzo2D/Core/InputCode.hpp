#pragma once

#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace l2d
{
    enum class InputDeviceType : std::uint8_t
    {
        Keyboard,
        Mouse,
        Gamepad,
        Touch
    };

    enum class InputControlType : std::uint8_t
    {
        Button,
        Axis,
        LogicalButton
    };

    struct InputCode
    {
        InputDeviceType device = InputDeviceType::Keyboard;
        std::uint16_t deviceIndex = 0;
        std::int32_t code = -1;
        InputControlType control = InputControlType::Button;

        [[nodiscard]] static constexpr InputCode keyboard(sf::Keyboard::Scancode scancode) noexcept
        {
            return {InputDeviceType::Keyboard, 0u, static_cast<std::int32_t>(scancode),
                    InputControlType::Button};
        }

        [[nodiscard]] static constexpr InputCode keyboardLogical(sf::Keyboard::Key key) noexcept
        {
            return {InputDeviceType::Keyboard, 0u, static_cast<std::int32_t>(key),
                    InputControlType::LogicalButton};
        }

        [[nodiscard]] static constexpr InputCode mouse(sf::Mouse::Button button) noexcept
        {
            return {InputDeviceType::Mouse, 0u, static_cast<std::int32_t>(button),
                    InputControlType::Button};
        }

        [[nodiscard]] static constexpr InputCode gamepadButton(std::uint16_t gamepad,
                                                               unsigned int button) noexcept
        {
            return {InputDeviceType::Gamepad, gamepad, static_cast<std::int32_t>(button),
                    InputControlType::Button};
        }

        [[nodiscard]] static constexpr InputCode gamepadAxis(std::uint16_t gamepad,
                                                             sf::Joystick::Axis axis) noexcept
        {
            return {InputDeviceType::Gamepad, gamepad, static_cast<std::int32_t>(axis),
                    InputControlType::Axis};
        }

        [[nodiscard]] static constexpr InputCode touch(std::uint16_t finger) noexcept
        {
            return {InputDeviceType::Touch, finger, 0, InputControlType::Button};
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            if (code < 0) return false;

            switch (device)
            {
            case InputDeviceType::Keyboard:
                if (deviceIndex != 0u) return false;
                if (control == InputControlType::Button)
                    return static_cast<unsigned int>(code) < sf::Keyboard::ScancodeCount;
                if (control == InputControlType::LogicalButton)
                    return static_cast<unsigned int>(code) < sf::Keyboard::KeyCount;
                return false;

            case InputDeviceType::Mouse:
                return deviceIndex == 0u && control == InputControlType::Button &&
                       static_cast<unsigned int>(code) < sf::Mouse::ButtonCount;

            case InputDeviceType::Gamepad:
                if (deviceIndex >= sf::Joystick::Count) return false;
                if (control == InputControlType::Button)
                    return static_cast<unsigned int>(code) < sf::Joystick::ButtonCount;
                if (control == InputControlType::Axis)
                    return static_cast<unsigned int>(code) < sf::Joystick::AxisCount;
                return false;

            case InputDeviceType::Touch:
                return control == InputControlType::Button && code == 0;
            }

            return false;
        }
    };

    [[nodiscard]] constexpr bool operator==(const InputCode& left, const InputCode& right) noexcept
    {
        return left.device == right.device && left.deviceIndex == right.deviceIndex &&
               left.code == right.code && left.control == right.control;
    }

    [[nodiscard]] constexpr bool operator!=(const InputCode& left, const InputCode& right) noexcept
    {
        return !(left == right);
    }

    struct InputCodeHash
    {
        [[nodiscard]] std::size_t operator()(const InputCode& inputCode) const noexcept
        {
            std::size_t hash = std::hash<std::int32_t>{}(inputCode.code);
            hash ^= std::hash<std::uint16_t>{}(inputCode.deviceIndex) + 0x9e3779b9u + (hash << 6u) +
                    (hash >> 2u);
            hash ^= std::hash<unsigned int>{}(static_cast<unsigned int>(inputCode.device)) +
                    0x9e3779b9u + (hash << 6u) + (hash >> 2u);
            hash ^= std::hash<unsigned int>{}(static_cast<unsigned int>(inputCode.control)) +
                    0x9e3779b9u + (hash << 6u) + (hash >> 2u);
            return hash;
        }
    };
}

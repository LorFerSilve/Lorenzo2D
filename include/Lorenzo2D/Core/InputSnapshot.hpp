#pragma once

#include <Lorenzo2D/Core/InputCode.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace l2d
{
    class InputSnapshot
    {
      public:
        void reset();
        void beginFrame();

        [[nodiscard]] std::uint64_t frameNumber() const noexcept;

        bool setDeviceConnected(InputDeviceType device, std::uint16_t deviceIndex, bool connected);
        [[nodiscard]] bool isDeviceConnected(InputDeviceType device,
                                             std::uint16_t deviceIndex = 0u) const noexcept;

        bool setButton(InputCode inputCode, bool down);
        bool setAxis(InputCode inputCode, float value);

        [[nodiscard]] float value(InputCode inputCode) const noexcept;
        [[nodiscard]] bool down(InputCode inputCode) const noexcept;
        [[nodiscard]] bool previousDown(InputCode inputCode) const noexcept;
        [[nodiscard]] bool pressed(InputCode inputCode) const noexcept;
        [[nodiscard]] bool released(InputCode inputCode) const noexcept;

      private:
        using StateMap = std::unordered_map<InputCode, float, InputCodeHash>;

        [[nodiscard]] float previousValue(InputCode inputCode) const noexcept;
        void clearDevice(InputDeviceType device, std::uint16_t deviceIndex);

      private:
        StateMap m_currentValues;
        StateMap m_previousValues;
        std::unordered_set<InputCode, InputCodeHash> m_pressedButtons;
        std::unordered_set<InputCode, InputCodeHash> m_releasedButtons;
        std::array<bool, sf::Joystick::Count> m_connectedGamepads{};
        std::uint64_t m_frameNumber = 0u;
    };
}

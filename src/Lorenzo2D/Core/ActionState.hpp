#pragma once

namespace l2d::core_detail
{
    class ActionState
    {
      public:
        constexpr void include(bool currentPressed, bool previousPressed) noexcept
        {
            m_currentPressed = m_currentPressed || currentPressed;
            m_previousPressed = m_previousPressed || previousPressed;
        }

        [[nodiscard]] constexpr bool isPressed() const noexcept
        {
            return m_currentPressed;
        }

        [[nodiscard]] constexpr bool wasPressed() const noexcept
        {
            return m_currentPressed && !m_previousPressed;
        }

        [[nodiscard]] constexpr bool wasReleased() const noexcept
        {
            return !m_currentPressed && m_previousPressed;
        }

      private:
        bool m_currentPressed = false;
        bool m_previousPressed = false;
    };

    [[nodiscard]] constexpr bool previousButtonState(bool currentPressed, bool wasPressed,
                                                     bool wasReleased) noexcept
    {
        return currentPressed ? !wasPressed : wasReleased;
    }
}

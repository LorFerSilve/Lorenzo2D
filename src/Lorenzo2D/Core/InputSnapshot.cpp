#include <Lorenzo2D/Core/InputSnapshot.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    void InputSnapshot::reset()
    {
        m_currentValues.clear();
        m_previousValues.clear();
        m_pressedButtons.clear();
        m_releasedButtons.clear();
        m_connectedGamepads.fill(false);
        m_frameNumber = 0u;
    }

    void InputSnapshot::beginFrame()
    {
        m_previousValues = m_currentValues;
        m_pressedButtons.clear();
        m_releasedButtons.clear();
        ++m_frameNumber;
    }

    std::uint64_t InputSnapshot::frameNumber() const noexcept
    {
        return m_frameNumber;
    }

    bool InputSnapshot::setDeviceConnected(InputDeviceType device, std::uint16_t deviceIndex,
                                           bool connected)
    {
        if (device != InputDeviceType::Gamepad || deviceIndex >= m_connectedGamepads.size())
            return false;

        if (m_connectedGamepads[deviceIndex] == connected) return true;

        m_connectedGamepads[deviceIndex] = connected;
        if (!connected) clearDevice(device, deviceIndex);
        return true;
    }

    bool InputSnapshot::isDeviceConnected(InputDeviceType device,
                                          std::uint16_t deviceIndex) const noexcept
    {
        switch (device)
        {
        case InputDeviceType::Keyboard:
        case InputDeviceType::Mouse:
            return deviceIndex == 0u;

        case InputDeviceType::Gamepad:
            return deviceIndex < m_connectedGamepads.size() && m_connectedGamepads[deviceIndex];

        case InputDeviceType::Touch:
            return true;
        }

        return false;
    }

    bool InputSnapshot::setButton(InputCode inputCode, bool isDown)
    {
        if (!inputCode.isValid() || inputCode.control == InputControlType::Axis) return false;
        if (inputCode.device == InputDeviceType::Gamepad &&
            !isDeviceConnected(inputCode.device, inputCode.deviceIndex))
            return !isDown;

        const bool wasDown = value(inputCode) > 0.5f;
        if (wasDown != isDown)
        {
            if (isDown)
                m_pressedButtons.insert(inputCode);
            else
                m_releasedButtons.insert(inputCode);
        }

        if (isDown)
            m_currentValues[inputCode] = 1.f;
        else
            m_currentValues.erase(inputCode);

        return true;
    }

    bool InputSnapshot::setAxis(InputCode inputCode, float axisValue)
    {
        if (!inputCode.isValid() || inputCode.control != InputControlType::Axis ||
            !std::isfinite(axisValue))
            return false;
        if (!isDeviceConnected(inputCode.device, inputCode.deviceIndex)) return false;

        axisValue = std::clamp(axisValue, -1.f, 1.f);
        if (axisValue == 0.f)
            m_currentValues.erase(inputCode);
        else
            m_currentValues[inputCode] = axisValue;

        return true;
    }

    float InputSnapshot::value(InputCode inputCode) const noexcept
    {
        if (!inputCode.isValid()) return 0.f;
        if (inputCode.device == InputDeviceType::Gamepad &&
            !isDeviceConnected(inputCode.device, inputCode.deviceIndex))
            return 0.f;

        const auto iterator = m_currentValues.find(inputCode);
        return iterator == m_currentValues.end() ? 0.f : iterator->second;
    }

    float InputSnapshot::previousValue(InputCode inputCode) const noexcept
    {
        if (!inputCode.isValid()) return 0.f;

        const auto iterator = m_previousValues.find(inputCode);
        return iterator == m_previousValues.end() ? 0.f : iterator->second;
    }

    bool InputSnapshot::down(InputCode inputCode) const noexcept
    {
        return value(inputCode) > 0.5f;
    }

    bool InputSnapshot::previousDown(InputCode inputCode) const noexcept
    {
        return previousValue(inputCode) > 0.5f;
    }

    bool InputSnapshot::pressed(InputCode inputCode) const noexcept
    {
        return m_pressedButtons.find(inputCode) != m_pressedButtons.end() ||
               (down(inputCode) && !previousDown(inputCode));
    }

    bool InputSnapshot::released(InputCode inputCode) const noexcept
    {
        return m_releasedButtons.find(inputCode) != m_releasedButtons.end() ||
               (!down(inputCode) && previousDown(inputCode));
    }

    void InputSnapshot::clearDevice(InputDeviceType device, std::uint16_t deviceIndex)
    {
        for (auto iterator = m_currentValues.begin(); iterator != m_currentValues.end();)
        {
            if (iterator->first.device == device && iterator->first.deviceIndex == deviceIndex)
                iterator = m_currentValues.erase(iterator);
            else
                ++iterator;
        }
    }
}

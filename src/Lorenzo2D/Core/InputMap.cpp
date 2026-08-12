#include <Lorenzo2D/Core/InputMap.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    namespace
    {
        const InputSnapshot EMPTY_SNAPSHOT;

        bool validActionName(const std::string& actionName)
        {
            return !actionName.empty();
        }

        bool validDeadzone(float deadzone)
        {
            return std::isfinite(deadzone) && deadzone >= 0.f && deadzone < 1.f;
        }

        float applyDeadzone(float value, float deadzone)
        {
            const float magnitude = std::abs(value);
            if (magnitude <= deadzone) return 0.f;

            const float scaledMagnitude = (magnitude - deadzone) / (1.f - deadzone);
            return std::copysign(std::min(scaledMagnitude, 1.f), value);
        }

        sf::Vector2f applyRadialDeadzone(sf::Vector2f value, float deadzone)
        {
            const float magnitude = std::sqrt(value.x * value.x + value.y * value.y);
            if (magnitude <= deadzone || magnitude == 0.f) return {0.f, 0.f};

            const float scaledMagnitude = std::min((magnitude - deadzone) / (1.f - deadzone), 1.f);
            return value * (scaledMagnitude / magnitude);
        }

        sf::Vector2f normalizeIfNeeded(sf::Vector2f value, bool normalize)
        {
            const float squaredMagnitude = value.x * value.x + value.y * value.y;
            if (!normalize || squaredMagnitude <= 1.f) return value;

            return value / std::sqrt(squaredMagnitude);
        }

    }

    InputMap::InputMap(const InputSnapshot& inputSnapshot) noexcept : m_snapshot(&inputSnapshot) {}

    void InputMap::setSnapshot(const InputSnapshot& inputSnapshot) noexcept
    {
        m_snapshot = &inputSnapshot;
        m_consumedPressFrames.clear();
        m_consumedReleaseFrames.clear();
    }

    bool InputMap::bindButton(const std::string& actionName, InputCode inputCode)
    {
        if (!inputCode.isValid() || inputCode.control == InputControlType::Axis ||
            !canBind(actionName, InputActionType::Button))
            return false;

        std::vector<InputCode>& bindings = m_buttonBindings[actionName];
        if (std::find(bindings.begin(), bindings.end(), inputCode) == bindings.end())
            bindings.push_back(inputCode);
        return true;
    }

    bool InputMap::bindAxis1D(const std::string& actionName, InputCode negative, InputCode positive)
    {
        if (!negative.isValid() || !positive.isValid() ||
            negative.control == InputControlType::Axis ||
            positive.control == InputControlType::Axis ||
            !canBind(actionName, InputActionType::Axis1D))
            return false;

        std::vector<DigitalAxis1DBinding>& bindings = m_digitalAxis1DBindings[actionName];
        const auto duplicate =
            std::find_if(bindings.begin(), bindings.end(),
                         [negative, positive](const DigitalAxis1DBinding& binding)
                         { return binding.negative == negative && binding.positive == positive; });
        if (duplicate == bindings.end()) bindings.push_back({negative, positive});
        return true;
    }

    bool InputMap::bindAxis1D(const std::string& actionName, InputCode axis, float deadzone)
    {
        if (!axis.isValid() || axis.control != InputControlType::Axis || !validDeadzone(deadzone) ||
            !canBind(actionName, InputActionType::Axis1D))
            return false;

        std::vector<AnalogAxis1DBinding>& bindings = m_analogAxis1DBindings[actionName];
        const auto duplicate = std::find_if(
            bindings.begin(), bindings.end(), [axis, deadzone](const AnalogAxis1DBinding& binding)
            { return binding.axis == axis && binding.deadzone == deadzone; });
        if (duplicate == bindings.end()) bindings.push_back({axis, deadzone});
        return true;
    }

    bool InputMap::bindAxis2D(const std::string& actionName, InputCode left, InputCode right,
                              InputCode up, InputCode downCode, bool normalize)
    {
        if (!left.isValid() || !right.isValid() || !up.isValid() || !downCode.isValid() ||
            left.control == InputControlType::Axis || right.control == InputControlType::Axis ||
            up.control == InputControlType::Axis || downCode.control == InputControlType::Axis ||
            !canBind(actionName, InputActionType::Axis2D))
            return false;

        std::vector<DigitalAxis2DBinding>& bindings = m_digitalAxis2DBindings[actionName];
        const auto duplicate =
            std::find_if(bindings.begin(), bindings.end(),
                         [left, right, up, downCode, normalize](const DigitalAxis2DBinding& binding)
                         {
                             return binding.left == left && binding.right == right &&
                                    binding.up == up && binding.down == downCode &&
                                    binding.normalize == normalize;
                         });
        if (duplicate == bindings.end()) bindings.push_back({left, right, up, downCode, normalize});
        return true;
    }

    bool InputMap::bindAxis2D(const std::string& actionName, InputCode horizontalAxis,
                              InputCode verticalAxis, float deadzone, bool normalize)
    {
        if (!horizontalAxis.isValid() || !verticalAxis.isValid() ||
            horizontalAxis.control != InputControlType::Axis ||
            verticalAxis.control != InputControlType::Axis || !validDeadzone(deadzone) ||
            !canBind(actionName, InputActionType::Axis2D))
            return false;

        std::vector<AnalogAxis2DBinding>& bindings = m_analogAxis2DBindings[actionName];
        const auto duplicate = std::find_if(
            bindings.begin(), bindings.end(),
            [horizontalAxis, verticalAxis, deadzone, normalize](const AnalogAxis2DBinding& binding)
            {
                return binding.horizontalAxis == horizontalAxis &&
                       binding.verticalAxis == verticalAxis && binding.deadzone == deadzone &&
                       binding.normalize == normalize;
            });
        if (duplicate == bindings.end())
            bindings.push_back({horizontalAxis, verticalAxis, deadzone, normalize});
        return true;
    }

    bool InputMap::clearAction(const std::string& actionName)
    {
        if (m_actionTypes.erase(actionName) == 0u) return false;

        m_buttonBindings.erase(actionName);
        m_digitalAxis1DBindings.erase(actionName);
        m_analogAxis1DBindings.erase(actionName);
        m_digitalAxis2DBindings.erase(actionName);
        m_analogAxis2DBindings.erase(actionName);
        m_consumedPressFrames.erase(actionName);
        m_consumedReleaseFrames.erase(actionName);
        return true;
    }

    void InputMap::clearAll()
    {
        m_actionTypes.clear();
        m_buttonBindings.clear();
        m_digitalAxis1DBindings.clear();
        m_analogAxis1DBindings.clear();
        m_digitalAxis2DBindings.clear();
        m_analogAxis2DBindings.clear();
        m_consumedPressFrames.clear();
        m_consumedReleaseFrames.clear();
    }

    bool InputMap::hasAction(const std::string& actionName) const
    {
        return m_actionTypes.find(actionName) != m_actionTypes.end();
    }

    std::optional<InputActionType> InputMap::actionType(const std::string& actionName) const
    {
        const auto iterator = m_actionTypes.find(actionName);
        return iterator == m_actionTypes.end() ? std::nullopt
                                               : std::optional<InputActionType>(iterator->second);
    }

    bool InputMap::down(const std::string& actionName) const
    {
        const auto iterator = m_buttonBindings.find(actionName);
        if (iterator == m_buttonBindings.end()) return false;

        return std::any_of(iterator->second.begin(), iterator->second.end(),
                           [this](InputCode inputCode) { return snapshot().down(inputCode); });
    }

    bool InputMap::pressed(const std::string& actionName) const
    {
        const auto iterator = m_buttonBindings.find(actionName);
        if (iterator == m_buttonBindings.end()) return false;

        bool currentDown = false;
        bool previousDown = false;
        for (InputCode inputCode : iterator->second)
        {
            currentDown = currentDown || snapshot().down(inputCode);
            previousDown = previousDown || snapshot().previousDown(inputCode);
        }
        if (currentDown && !previousDown) return true;
        if (currentDown || previousDown) return false;

        return std::any_of(iterator->second.begin(), iterator->second.end(),
                           [this](InputCode inputCode) { return snapshot().pressed(inputCode); });
    }

    bool InputMap::released(const std::string& actionName) const
    {
        const auto iterator = m_buttonBindings.find(actionName);
        if (iterator == m_buttonBindings.end()) return false;

        bool currentDown = false;
        bool previousDown = false;
        for (InputCode inputCode : iterator->second)
        {
            currentDown = currentDown || snapshot().down(inputCode);
            previousDown = previousDown || snapshot().previousDown(inputCode);
        }
        if (!currentDown && previousDown) return true;
        if (currentDown || previousDown) return false;

        return std::any_of(iterator->second.begin(), iterator->second.end(),
                           [this](InputCode inputCode) { return snapshot().released(inputCode); });
    }

    bool InputMap::consumePressed(const std::string& actionName)
    {
        if (!pressed(actionName)) return false;

        const std::uint64_t frame = snapshot().frameNumber();
        const auto consumed = m_consumedPressFrames.find(actionName);
        if (consumed != m_consumedPressFrames.end() && consumed->second == frame) return false;

        m_consumedPressFrames[actionName] = frame;
        return true;
    }

    bool InputMap::consumeReleased(const std::string& actionName)
    {
        if (!released(actionName)) return false;

        const std::uint64_t frame = snapshot().frameNumber();
        const auto consumed = m_consumedReleaseFrames.find(actionName);
        if (consumed != m_consumedReleaseFrames.end() && consumed->second == frame) return false;

        m_consumedReleaseFrames[actionName] = frame;
        return true;
    }

    float InputMap::axis1D(const std::string& actionName) const
    {
        float result = 0.f;

        const auto digital = m_digitalAxis1DBindings.find(actionName);
        if (digital != m_digitalAxis1DBindings.end())
        {
            for (const DigitalAxis1DBinding& binding : digital->second)
            {
                result += (snapshot().down(binding.positive) ? 1.f : 0.f) -
                          (snapshot().down(binding.negative) ? 1.f : 0.f);
            }
        }

        const auto analog = m_analogAxis1DBindings.find(actionName);
        if (analog != m_analogAxis1DBindings.end())
        {
            for (const AnalogAxis1DBinding& binding : analog->second)
                result += applyDeadzone(snapshot().value(binding.axis), binding.deadzone);
        }

        return std::clamp(result, -1.f, 1.f);
    }

    sf::Vector2f InputMap::axis2D(const std::string& actionName) const
    {
        sf::Vector2f result{0.f, 0.f};
        bool shouldNormalize = false;

        const auto digital = m_digitalAxis2DBindings.find(actionName);
        if (digital != m_digitalAxis2DBindings.end())
        {
            for (const DigitalAxis2DBinding& binding : digital->second)
            {
                result.x += (snapshot().down(binding.right) ? 1.f : 0.f) -
                            (snapshot().down(binding.left) ? 1.f : 0.f);
                result.y += (snapshot().down(binding.down) ? 1.f : 0.f) -
                            (snapshot().down(binding.up) ? 1.f : 0.f);
                shouldNormalize = shouldNormalize || binding.normalize;
            }
        }

        const auto analog = m_analogAxis2DBindings.find(actionName);
        if (analog != m_analogAxis2DBindings.end())
        {
            for (const AnalogAxis2DBinding& binding : analog->second)
            {
                result += applyRadialDeadzone({snapshot().value(binding.horizontalAxis),
                                               snapshot().value(binding.verticalAxis)},
                                              binding.deadzone);
                shouldNormalize = shouldNormalize || binding.normalize;
            }
        }

        result.x = std::clamp(result.x, -1.f, 1.f);
        result.y = std::clamp(result.y, -1.f, 1.f);
        return normalizeIfNeeded(result, shouldNormalize);
    }

    const InputSnapshot& InputMap::snapshot() const noexcept
    {
        return m_snapshot == nullptr ? EMPTY_SNAPSHOT : *m_snapshot;
    }

    bool InputMap::canBind(const std::string& actionName, InputActionType type)
    {
        if (!validActionName(actionName)) return false;

        const auto iterator = m_actionTypes.find(actionName);
        if (iterator != m_actionTypes.end()) return iterator->second == type;

        m_actionTypes.emplace(actionName, type);
        return true;
    }

}

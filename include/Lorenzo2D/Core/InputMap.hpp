#pragma once

#include <Lorenzo2D/Core/InputSnapshot.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace l2d
{
    enum class InputActionType : std::uint8_t
    {
        Button,
        Axis1D,
        Axis2D
    };

    class InputMap
    {
      public:
        InputMap() = default;

        // InputMap borrows the snapshot; the referenced object must outlive
        // this map. Rvalues are rejected so a temporary cannot leave a
        // dangling snapshot pointer.
        explicit InputMap(const InputSnapshot& snapshot) noexcept;
        InputMap(InputSnapshot&&) = delete;
        InputMap(const InputSnapshot&&) = delete;

        void setSnapshot(const InputSnapshot& snapshot) noexcept;
        void setSnapshot(InputSnapshot&&) = delete;
        void setSnapshot(const InputSnapshot&&) = delete;
        void clearSnapshot() noexcept;
        [[nodiscard]] bool hasSnapshot() const noexcept;

        bool bindButton(const std::string& actionName, InputCode inputCode);
        bool bindAxis1D(const std::string& actionName, InputCode negative, InputCode positive);
        bool bindAxis1D(const std::string& actionName, InputCode axis, float deadzone = 0.15f);
        bool bindAxis2D(const std::string& actionName, InputCode left, InputCode right,
                        InputCode up, InputCode down, bool normalize = true);
        bool bindAxis2D(const std::string& actionName, InputCode horizontalAxis,
                        InputCode verticalAxis, float deadzone = 0.15f, bool normalize = true);

        bool clearAction(const std::string& actionName);
        void clearAll();

        [[nodiscard]] bool hasAction(const std::string& actionName) const;
        [[nodiscard]] std::optional<InputActionType> actionType(
            const std::string& actionName) const;

        [[nodiscard]] bool down(const std::string& actionName) const;
        [[nodiscard]] bool pressed(const std::string& actionName) const;
        [[nodiscard]] bool released(const std::string& actionName) const;
        bool consumePressed(const std::string& actionName);
        bool consumeReleased(const std::string& actionName);

        [[nodiscard]] float axis1D(const std::string& actionName) const;
        [[nodiscard]] sf::Vector2f axis2D(const std::string& actionName) const;

      private:
        struct DigitalAxis1DBinding
        {
            InputCode negative;
            InputCode positive;
        };

        struct AnalogAxis1DBinding
        {
            InputCode axis;
            float deadzone = 0.15f;
        };

        struct DigitalAxis2DBinding
        {
            InputCode left;
            InputCode right;
            InputCode up;
            InputCode down;
            bool normalize = true;
        };

        struct AnalogAxis2DBinding
        {
            InputCode horizontalAxis;
            InputCode verticalAxis;
            float deadzone = 0.15f;
            bool normalize = true;
        };

        [[nodiscard]] const InputSnapshot& snapshot() const noexcept;
        [[nodiscard]] bool canBind(const std::string& actionName, InputActionType type);

      private:
        const InputSnapshot* m_snapshot = nullptr;
        std::unordered_map<std::string, InputActionType> m_actionTypes;
        std::unordered_map<std::string, std::vector<InputCode>> m_buttonBindings;
        std::unordered_map<std::string, std::vector<DigitalAxis1DBinding>> m_digitalAxis1DBindings;
        std::unordered_map<std::string, std::vector<AnalogAxis1DBinding>> m_analogAxis1DBindings;
        std::unordered_map<std::string, std::vector<DigitalAxis2DBinding>> m_digitalAxis2DBindings;
        std::unordered_map<std::string, std::vector<AnalogAxis2DBinding>> m_analogAxis2DBindings;
        std::unordered_map<std::string, std::uint64_t> m_consumedPressFrames;
        std::unordered_map<std::string, std::uint64_t> m_consumedReleaseFrames;
    };
}

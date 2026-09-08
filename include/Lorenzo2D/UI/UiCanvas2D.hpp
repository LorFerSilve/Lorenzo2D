#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    struct UiButtonStyle2D
    {
        sf::Color idleColor{52, 62, 78};
        sf::Color hoveredColor{72, 86, 108};
        sf::Color pressedColor{42, 50, 64};
        sf::Color disabledColor{34, 38, 46};
        sf::Color textColor = sf::Color::White;
        unsigned int characterSize = 18u;
        sf::Vector2f textOffset{12.f, 8.f};
    };

    struct UiButton2D
    {
        std::string id;
        std::string label;
        sf::Vector2f position{0.f, 0.f};
        sf::Vector2f size{120.f, 40.f};
        UiButtonStyle2D style;
        bool enabled = true;
        bool visible = true;
    };

    class UiCanvas2D
    {
      public:
        static bool isValidButton(const UiButton2D& button) noexcept;

        bool addButton(UiButton2D button);
        bool removeButton(std::string_view id);
        void clear() noexcept;

        bool hasButton(std::string_view id) const noexcept;
        std::size_t buttonCount() const noexcept;

        bool setButtonEnabled(std::string_view id, bool enabled);
        bool setButtonVisible(std::string_view id, bool visible);
        bool setButtonLabel(std::string_view id, std::string label);
        bool setButtonBounds(std::string_view id, sf::Vector2f position, sf::Vector2f size);

        void update(const PointerState& pointer);
        void cancelInteraction() noexcept;

        bool wasActivated(std::string_view id) const noexcept;
        std::optional<std::string> hoveredButton() const;
        std::optional<std::string> pressedButton() const;

        // Screen-space UI is rendered against the window's default view. The
        // caller's previous view is restored before returning.
        void render(sf::RenderWindow& window, FontHandle font = {}) const;

      private:
        std::size_t indexOf(std::string_view id) const noexcept;
        std::optional<std::size_t> hitTest(sf::Vector2f screenPosition) const noexcept;

        std::vector<UiButton2D> m_buttons;
        std::optional<std::size_t> m_hovered;
        std::optional<std::size_t> m_pressed;
        std::vector<std::string> m_activated;
        bool m_pointerDown = false;
    };
}

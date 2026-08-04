#pragma once

#include <array>
#include <cstddef>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

namespace sf
{
    class RenderWindow;
    class View;
}

namespace l2d
{
    class Application;

    enum class MouseButton
    {
        Unknown = 0,

        Left,
        Right,
        Middle,

        Count
    };

    class Mouse
    {
      public:
        static bool isButtonPressed(MouseButton button);
        static bool wasButtonPressed(MouseButton button);
        static bool wasButtonReleased(MouseButton button);

        static const sf::Vector2i& screenPosition();

        static sf::Vector2f worldPosition(const sf::RenderWindow& window);
        static sf::Vector2f worldPosition(const sf::RenderWindow& window, const sf::View& view);

      private:
        static void update(const sf::RenderWindow& window);

        static sf::Mouse::Button toSfmlButton(MouseButton button);
        static std::size_t buttonToIndex(MouseButton button);

      private:
        static std::array<bool, static_cast<std::size_t>(MouseButton::Count)> s_currentButtons;
        static std::array<bool, static_cast<std::size_t>(MouseButton::Count)> s_previousButtons;

        static sf::Vector2i s_screenPosition;

        friend class Application;
    };
}

#include <Lorenzo2D/Core/Mouse.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

namespace l2d
{
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> Mouse::s_currentButtons = {};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> Mouse::s_previousButtons = {};

    sf::Vector2i Mouse::s_screenPosition = { 0, 0 };

    bool Mouse::isButtonPressed(MouseButton button)
    {
        const std::size_t index = buttonToIndex(button);

        if (index >= s_currentButtons.size())
            return false;

        return s_currentButtons[index];
    }

    bool Mouse::wasButtonPressed(MouseButton button)
    {
        const std::size_t index = buttonToIndex(button);

        if (index >= s_currentButtons.size())
            return false;

        return s_currentButtons[index] && !s_previousButtons[index];
    }

    bool Mouse::wasButtonReleased(MouseButton button)
    {
        const std::size_t index = buttonToIndex(button);

        if (index >= s_currentButtons.size())
            return false;

        return !s_currentButtons[index] && s_previousButtons[index];
    }

    const sf::Vector2i& Mouse::screenPosition()
    {
        return s_screenPosition;
    }

    sf::Vector2f Mouse::worldPosition(const sf::RenderWindow& window)
    {
        return window.mapPixelToCoords(s_screenPosition);
    }

    sf::Vector2f Mouse::worldPosition(const sf::RenderWindow& window, const sf::View& view)
    {
        return window.mapPixelToCoords(s_screenPosition, view);
    }

    void Mouse::update(const sf::RenderWindow& window)
    {
        s_previousButtons = s_currentButtons;

        for (std::size_t index = 0; index < s_currentButtons.size(); ++index)
        {
            const MouseButton button = static_cast<MouseButton>(index);

            if (button == MouseButton::Unknown || button == MouseButton::Count)
            {
                s_currentButtons[index] = false;
                continue;
            }

            s_currentButtons[index] = sf::Mouse::isButtonPressed(
                toSfmlButton(button)
            );
        }

        s_screenPosition = sf::Mouse::getPosition(window);
    }

    sf::Mouse::Button Mouse::toSfmlButton(MouseButton button)
    {
        switch (button)
        {
        case MouseButton::Left:
            return sf::Mouse::Button::Left;

        case MouseButton::Right:
            return sf::Mouse::Button::Right;

        case MouseButton::Middle:
            return sf::Mouse::Button::Middle;

        case MouseButton::Unknown:
        case MouseButton::Count:
        default:
            return sf::Mouse::Button::Left;
        }
    }

    std::size_t Mouse::buttonToIndex(MouseButton button)
    {
        return static_cast<std::size_t>(button);
    }
}
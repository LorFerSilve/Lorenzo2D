#include <Lorenzo2D/Core/Mouse.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

namespace l2d
{
    bool Mouse::isButtonPressed(MouseButton button)
    {
        if (button == MouseButton::Unknown || button == MouseButton::Count) return false;
        return Input::snapshot().down(InputCode::mouse(toSfmlButton(button)));
    }

    bool Mouse::wasButtonPressed(MouseButton button)
    {
        if (button == MouseButton::Unknown || button == MouseButton::Count) return false;
        return Input::snapshot().pressed(InputCode::mouse(toSfmlButton(button)));
    }

    bool Mouse::wasButtonReleased(MouseButton button)
    {
        if (button == MouseButton::Unknown || button == MouseButton::Count) return false;
        return Input::snapshot().released(InputCode::mouse(toSfmlButton(button)));
    }

    const sf::Vector2i& Mouse::screenPosition()
    {
        return Pointer::primary().screenPosition;
    }

    sf::Vector2f Mouse::worldPosition(const sf::RenderWindow& window)
    {
        return Pointer::worldPosition(window);
    }

    sf::Vector2f Mouse::worldPosition(const sf::RenderWindow& window, const sf::View& view)
    {
        return Pointer::worldPosition(window, view);
    }

    void Mouse::update(const sf::RenderWindow& window)
    {
        (void)window;
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

}

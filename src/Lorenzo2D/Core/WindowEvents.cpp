#include <Lorenzo2D/Core/WindowEvents.hpp>

namespace l2d
{
    bool WindowEvents::s_closeRequested = false;

    bool WindowEvents::s_wasResized = false;
    sf::Vector2u WindowEvents::s_resizedSize = {0, 0};

    bool WindowEvents::s_mouseWheelScrolled = false;
    float WindowEvents::s_mouseWheelDelta = 0.f;
    sf::Vector2i WindowEvents::s_mouseWheelPosition = {0, 0};
    MouseWheel WindowEvents::s_mouseWheel = MouseWheel::Unknown;

    bool WindowEvents::closeRequested()
    {
        return s_closeRequested;
    }

    bool WindowEvents::wasResized()
    {
        return s_wasResized;
    }

    const sf::Vector2u& WindowEvents::resizedSize()
    {
        return s_resizedSize;
    }

    bool WindowEvents::mouseWheelScrolled()
    {
        return s_mouseWheelScrolled;
    }

    float WindowEvents::mouseWheelDelta()
    {
        return s_mouseWheelDelta;
    }

    const sf::Vector2i& WindowEvents::mouseWheelPosition()
    {
        return s_mouseWheelPosition;
    }

    MouseWheel WindowEvents::mouseWheel()
    {
        return s_mouseWheel;
    }

    void WindowEvents::beginFrame()
    {
        s_closeRequested = false;

        s_wasResized = false;

        s_mouseWheelScrolled = false;
        s_mouseWheelDelta = 0.f;
        s_mouseWheelPosition = {0, 0};
        s_mouseWheel = MouseWheel::Unknown;
    }

    void WindowEvents::processEvent(const sf::Event& event)
    {
        if (event.is<sf::Event::Closed>())
        {
            s_closeRequested = true;
            return;
        }

        if (const auto* resized = event.getIf<sf::Event::Resized>())
        {
            s_wasResized = true;
            s_resizedSize = resized->size;
            return;
        }

        if (const auto* mouseWheelScrolled = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            s_mouseWheelScrolled = true;
            s_mouseWheelDelta = mouseWheelScrolled->delta;
            s_mouseWheelPosition = mouseWheelScrolled->position;
            s_mouseWheel = fromSfmlMouseWheel(mouseWheelScrolled->wheel);
            return;
        }
    }

    MouseWheel WindowEvents::fromSfmlMouseWheel(sf::Mouse::Wheel wheel)
    {
        switch (wheel)
        {
        case sf::Mouse::Wheel::Vertical:
            return MouseWheel::Vertical;

        case sf::Mouse::Wheel::Horizontal:
            return MouseWheel::Horizontal;

        default:
            return MouseWheel::Unknown;
        }
    }
}

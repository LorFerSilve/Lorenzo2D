#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

namespace l2d
{
    class Application;

    enum class MouseWheel
    {
        Unknown,
        Vertical,
        Horizontal
    };

    class WindowEvents
    {
    public:
        static bool closeRequested();

        static bool wasResized();
        static const sf::Vector2u& resizedSize();

        static bool mouseWheelScrolled();
        static float mouseWheelDelta();
        static const sf::Vector2i& mouseWheelPosition();
        static MouseWheel mouseWheel();

    private:
        static void beginFrame();
        static void processEvent(const sf::Event& event);

        static MouseWheel fromSfmlMouseWheel(sf::Mouse::Wheel wheel);

    private:
        static bool s_closeRequested;

        static bool s_wasResized;
        static sf::Vector2u s_resizedSize;

        static bool s_mouseWheelScrolled;
        static float s_mouseWheelDelta;
        static sf::Vector2i s_mouseWheelPosition;
        static MouseWheel s_mouseWheel;

        friend class Application;
    };
}
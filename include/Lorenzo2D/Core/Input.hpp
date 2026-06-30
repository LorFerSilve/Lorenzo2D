#pragma once

#include <array>
#include <cstddef>

#include <SFML/Window/Keyboard.hpp>

namespace l2d
{
    class Application;

    enum class Key
    {
        Unknown = 0,

        Z,
        Q,
        S,
        D,

        W,
        A,

        Space,
        Escape,

        Left,
        Right,
        Up,
        Down,

        F1,

        Count
    };

    class Input
    {
    public:
        static bool isKeyPressed(Key key);
        static bool wasKeyPressed(Key key);
        static bool wasKeyReleased(Key key);

    private:
        static void update();

        static sf::Keyboard::Key toSfmlKey(Key key);
        static std::size_t keyToIndex(Key key);

    private:
        static std::array<bool, static_cast<std::size_t>(Key::Count)> s_currentKeys;
        static std::array<bool, static_cast<std::size_t>(Key::Count)> s_previousKeys;

        friend class Application;
    };
}
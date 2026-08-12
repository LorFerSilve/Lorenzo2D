#pragma once

#include <Lorenzo2D/Core/InputSnapshot.hpp>

#include <SFML/Window/Keyboard.hpp>

namespace sf
{
    class Event;
}

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
        [[nodiscard]] static const InputSnapshot& snapshot() noexcept;

        [[nodiscard]] static InputCode physicalKey(sf::Keyboard::Scancode scancode) noexcept;
        [[nodiscard]] static InputCode logicalKey(sf::Keyboard::Key key) noexcept;
        [[nodiscard]] static InputCode code(Key key) noexcept;

        static bool isKeyPressed(Key key);
        static bool wasKeyPressed(Key key);
        static bool wasKeyReleased(Key key);

      private:
        static void reset();
        static void beginFrame();
        static void processEvent(const sf::Event& event);
        static void update();

        static sf::Keyboard::Key toSfmlKey(Key key);

      private:
        static InputSnapshot s_snapshot;

        friend class Application;
    };
}

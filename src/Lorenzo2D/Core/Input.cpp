#include <Lorenzo2D/Core/Input.hpp>

namespace l2d
{
    std::array<bool, static_cast<std::size_t>(Key::Count)> Input::s_currentKeys = {};
    std::array<bool, static_cast<std::size_t>(Key::Count)> Input::s_previousKeys = {};

    bool Input::isKeyPressed(Key key)
    {
        const std::size_t index = keyToIndex(key);

        if (index >= s_currentKeys.size())
            return false;

        return s_currentKeys[index];
    }

    bool Input::wasKeyPressed(Key key)
    {
        const std::size_t index = keyToIndex(key);

        if (index >= s_currentKeys.size())
            return false;

        return s_currentKeys[index] && !s_previousKeys[index];
    }

    bool Input::wasKeyReleased(Key key)
    {
        const std::size_t index = keyToIndex(key);

        if (index >= s_currentKeys.size())
            return false;

        return !s_currentKeys[index] && s_previousKeys[index];
    }

    void Input::update()
    {
        s_previousKeys = s_currentKeys;

        for (std::size_t index = 0; index < s_currentKeys.size(); ++index)
        {
            const Key key = static_cast<Key>(index);

            if (key == Key::Unknown || key == Key::Count)
            {
                s_currentKeys[index] = false;
                continue;
            }

            s_currentKeys[index] = sf::Keyboard::isKeyPressed(toSfmlKey(key));
        }
    }

    sf::Keyboard::Key Input::toSfmlKey(Key key)
    {
        switch (key)
        {
        case Key::Z:
            return sf::Keyboard::Key::Z;

        case Key::Q:
            return sf::Keyboard::Key::Q;

        case Key::S:
            return sf::Keyboard::Key::S;

        case Key::D:
            return sf::Keyboard::Key::D;

        case Key::W:
            return sf::Keyboard::Key::W;

        case Key::A:
            return sf::Keyboard::Key::A;

        case Key::Space:
            return sf::Keyboard::Key::Space;

        case Key::Escape:
            return sf::Keyboard::Key::Escape;

        case Key::Left:
            return sf::Keyboard::Key::Left;

        case Key::Right:
            return sf::Keyboard::Key::Right;

        case Key::Up:
            return sf::Keyboard::Key::Up;

        case Key::Down:
            return sf::Keyboard::Key::Down;

        case Key::F1:
            return sf::Keyboard::Key::F1;

        case Key::Unknown:
        case Key::Count:
        default:
            return sf::Keyboard::Key::Unknown;
        }
    }

    std::size_t Input::keyToIndex(Key key)
    {
        return static_cast<std::size_t>(key);
    }
}
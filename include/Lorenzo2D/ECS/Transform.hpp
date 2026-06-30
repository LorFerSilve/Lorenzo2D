#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class Transform
    {
    public:
        Transform();
        explicit Transform(sf::Vector2f position);

        const sf::Vector2f& position() const;
        void setPosition(sf::Vector2f position);
        void move(sf::Vector2f offset);

        float rotation() const;
        void setRotation(float rotation);
        void rotate(float angle);

        const sf::Vector2f& scale() const;
        void setScale(sf::Vector2f scale);

    private:
        sf::Vector2f m_position;
        float m_rotation;
        sf::Vector2f m_scale;
    };
}
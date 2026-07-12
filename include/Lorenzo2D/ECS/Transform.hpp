#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    struct TransformState
    {
        sf::Vector2f position = { 0.f, 0.f };
        float rotation = 0.f;
        sf::Vector2f scale = { 1.f, 1.f };
    };

    class Scene;

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

        TransformState interpolated(float alpha) const;
        void resetInterpolation();

    private:
        void capturePrevious();
        void synchronizePreviousBeforeFirstSnapshot();

    private:
        TransformState m_current;
        TransformState m_previous;
        bool m_hasHistory = false;

        friend class Scene;
    };
}

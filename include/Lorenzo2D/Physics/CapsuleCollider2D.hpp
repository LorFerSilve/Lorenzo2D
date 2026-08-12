#pragma once

#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>

namespace l2d
{
    // A vertical capsule in local space. Height is the total tip-to-tip height
    // and is always at least twice the radius.
    class CapsuleCollider2D : public Collider2D
    {
      public:
        explicit CapsuleCollider2D(float radius = 25.f, float height = 100.f);

        float radius() const;
        void setRadius(float radius);

        float height() const;
        void setHeight(float height);

        sf::Vector2f center() const;
        float worldRadius() const;
        std::array<sf::Vector2f, 2> worldSegment() const;

        sf::Vector2f min() const;
        sf::Vector2f max() const;

      private:
        float m_radius;
        float m_height;
    };
}

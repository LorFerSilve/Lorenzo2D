#pragma once

#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class CircleCollider2D : public Collider2D
    {
      public:
        explicit CircleCollider2D(float radius = 50.f);

        float radius() const;
        void setRadius(float radius);

        sf::Vector2f center() const;
        float worldRadius() const;

        bool overlaps(const CircleCollider2D& other) const;
        bool overlaps(const BoxCollider2D& box) const;

        sf::Vector2f collisionResolutionVector(const BoxCollider2D& box) const;

      private:
        float m_radius;
    };
}

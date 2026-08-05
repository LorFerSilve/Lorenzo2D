#pragma once

#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>

namespace l2d
{
    class BoxCollider2D : public Collider2D
    {
      public:
        explicit BoxCollider2D(sf::Vector2f size = {100.f, 100.f});

        const sf::Vector2f& size() const;
        void setSize(sf::Vector2f size);

        sf::Vector2f center() const;
        sf::Vector2f worldHalfExtents() const;
        std::array<sf::Vector2f, 4> corners() const;

        sf::Vector2f min() const;
        sf::Vector2f max() const;

        bool overlaps(const BoxCollider2D& other) const;

      private:
        sf::Vector2f m_size;
    };
}

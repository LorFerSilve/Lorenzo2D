#pragma once

#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

namespace l2d
{
    class ConvexPolygonCollider2D : public Collider2D
    {
      public:
        static constexpr std::size_t MaximumVertexCount = 16u;

        ConvexPolygonCollider2D();
        explicit ConvexPolygonCollider2D(std::vector<sf::Vector2f> vertices);

        const std::vector<sf::Vector2f>& vertices() const;

        // Transactional: invalid input leaves the existing polygon unchanged.
        // Accepted vertices form one strictly convex polygon. Clockwise input
        // is normalized to counter-clockwise winding.
        bool setVertices(std::vector<sf::Vector2f> vertices);
        static bool isValidVertices(const std::vector<sf::Vector2f>& vertices);

        std::vector<sf::Vector2f> worldVertices() const;
        sf::Vector2f center() const;
        sf::Vector2f min() const;
        sf::Vector2f max() const;

      private:
        std::vector<sf::Vector2f> m_vertices;
    };
}

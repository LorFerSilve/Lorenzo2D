#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        constexpr double CrossEpsilon = 1e-8;

        double cross(sf::Vector2f first, sf::Vector2f second, sf::Vector2f third)
        {
            return (static_cast<double>(second.x) - first.x) *
                       (static_cast<double>(third.y) - second.y) -
                   (static_cast<double>(second.y) - first.y) *
                       (static_cast<double>(third.x) - second.x);
        }

        double signedArea(const std::vector<sf::Vector2f>& vertices)
        {
            double twiceArea = 0.0;
            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                const sf::Vector2f current = vertices[index];
                const sf::Vector2f next = vertices[(index + 1u) % vertices.size()];
                twiceArea += static_cast<double>(current.x) * next.y -
                             static_cast<double>(current.y) * next.x;
            }
            return twiceArea * 0.5;
        }

        std::vector<sf::Vector2f> defaultVertices()
        {
            return {{-50.f, 50.f}, {0.f, -50.f}, {50.f, 50.f}};
        }
    }

    ConvexPolygonCollider2D::ConvexPolygonCollider2D()
        : Collider2D(ColliderType::ConvexPolygon), m_vertices(defaultVertices())
    {
    }

    ConvexPolygonCollider2D::ConvexPolygonCollider2D(std::vector<sf::Vector2f> vertices)
        : ConvexPolygonCollider2D()
    {
        (void)setVertices(std::move(vertices));
    }

    const std::vector<sf::Vector2f>& ConvexPolygonCollider2D::vertices() const
    {
        return m_vertices;
    }

    bool ConvexPolygonCollider2D::setVertices(std::vector<sf::Vector2f> vertices)
    {
        if (!isValidVertices(vertices)) return false;

        if (signedArea(vertices) < 0.0) std::reverse(vertices.begin(), vertices.end());
        m_vertices = std::move(vertices);
        return true;
    }

    bool ConvexPolygonCollider2D::isValidVertices(const std::vector<sf::Vector2f>& vertices)
    {
        if (vertices.size() < 3u || vertices.size() > MaximumVertexCount) return false;

        double winding = 0.0;
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            const sf::Vector2f vertex = vertices[index];
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) return false;

            const double turn = cross(vertex, vertices[(index + 1u) % vertices.size()],
                                      vertices[(index + 2u) % vertices.size()]);
            if (!std::isfinite(turn) || std::fabs(turn) <= CrossEpsilon) return false;

            if (winding == 0.0)
                winding = turn;
            else if ((winding > 0.0) != (turn > 0.0))
                return false;
        }

        return std::isfinite(signedArea(vertices)) &&
               std::fabs(signedArea(vertices)) > CrossEpsilon;
    }

    std::vector<sf::Vector2f> ConvexPolygonCollider2D::worldVertices() const
    {
        std::vector<sf::Vector2f> result;
        result.reserve(m_vertices.size());
        for (const sf::Vector2f vertex : m_vertices)
            result.push_back(localToWorldPoint(offset() + vertex));
        return result;
    }

    sf::Vector2f ConvexPolygonCollider2D::center() const
    {
        const std::vector<sf::Vector2f> transformed = worldVertices();
        double x = 0.0;
        double y = 0.0;
        for (const sf::Vector2f vertex : transformed)
        {
            x += vertex.x;
            y += vertex.y;
        }
        return {static_cast<float>(x / transformed.size()),
                static_cast<float>(y / transformed.size())};
    }

    sf::Vector2f ConvexPolygonCollider2D::min() const
    {
        const std::vector<sf::Vector2f> transformed = worldVertices();
        sf::Vector2f result = transformed.front();
        for (const sf::Vector2f vertex : transformed)
        {
            result.x = std::min(result.x, vertex.x);
            result.y = std::min(result.y, vertex.y);
        }
        return result;
    }

    sf::Vector2f ConvexPolygonCollider2D::max() const
    {
        const std::vector<sf::Vector2f> transformed = worldVertices();
        sf::Vector2f result = transformed.front();
        for (const sf::Vector2f vertex : transformed)
        {
            result.x = std::max(result.x, vertex.x);
            result.y = std::max(result.y, vertex.y);
        }
        return result;
    }
}

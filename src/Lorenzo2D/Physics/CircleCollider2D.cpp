#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        float sanitizeRadius(float radius)
        {
            if (!std::isfinite(radius) || radius < 0.f) return 0.f;

            return radius;
        }
    }

    CircleCollider2D::CircleCollider2D(float radius)
        : Collider2D(ColliderType::Circle), m_radius(sanitizeRadius(radius))
    {
    }

    float CircleCollider2D::radius() const
    {
        return m_radius;
    }

    void CircleCollider2D::setRadius(float radius)
    {
        m_radius = sanitizeRadius(radius);
    }

    sf::Vector2f CircleCollider2D::center() const
    {
        return worldPosition();
    }

    float CircleCollider2D::worldRadius() const
    {
        const sf::Vector2f scale = worldScale();
        const double scaledRadius = static_cast<double>(m_radius) * std::max(scale.x, scale.y);
        const double maximum = static_cast<double>(std::numeric_limits<float>::max());

        if (!std::isfinite(scaledRadius) || scaledRadius > maximum)
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        return static_cast<float>(scaledRadius);
    }

    bool CircleCollider2D::overlaps(const CircleCollider2D& other) const
    {
        CollisionManifold2D manifold;
        return computeCollisionManifold(*this, other, manifold);
    }

    bool CircleCollider2D::overlaps(const BoxCollider2D& box) const
    {
        CollisionManifold2D manifold;
        return computeCollisionManifold(*this, box, manifold);
    }

    sf::Vector2f CircleCollider2D::collisionResolutionVector(const BoxCollider2D& box) const
    {
        CollisionManifold2D manifold;

        if (!computeCollisionManifold(*this, box, manifold)) return {0.f, 0.f};

        return {-manifold.normal.x * manifold.penetration,
                -manifold.normal.y * manifold.penetration};
    }
}

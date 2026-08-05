#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <cmath>

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
        setOffset({m_radius, m_radius});
    }

    float CircleCollider2D::radius() const
    {
        return m_radius;
    }

    void CircleCollider2D::setRadius(float radius)
    {
        m_radius = sanitizeRadius(radius);
        setOffset({m_radius, m_radius});
    }

    sf::Vector2f CircleCollider2D::center() const
    {
        return worldPosition();
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

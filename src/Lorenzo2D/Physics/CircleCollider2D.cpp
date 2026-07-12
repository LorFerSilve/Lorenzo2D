#include <Lorenzo2D/Physics/CircleCollider2D.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    namespace
    {
        float sanitizeRadius(float radius)
        {
            if (!std::isfinite(radius) || radius < 0.f)
                return 0.f;

            return radius;
        }
    }

    CircleCollider2D::CircleCollider2D(float radius)
        : Collider2D(ColliderType::Circle),
        m_radius(sanitizeRadius(radius))
    {
        setOffset({ m_radius, m_radius });
    }

    float CircleCollider2D::radius() const
    {
        return m_radius;
    }

    void CircleCollider2D::setRadius(float radius)
    {
        m_radius = sanitizeRadius(radius);
        setOffset({ m_radius, m_radius });
    }

    sf::Vector2f CircleCollider2D::center() const
    {
        return worldPosition();
    }

    bool CircleCollider2D::overlaps(const CircleCollider2D& other) const
    {
        const sf::Vector2f a = center();
        const sf::Vector2f b = other.center();

        const float dx = a.x - b.x;
        const float dy = a.y - b.y;

        const float radiusSum = m_radius + other.m_radius;

        return dx * dx + dy * dy <= radiusSum * radiusSum;
    }

    bool CircleCollider2D::overlaps(const BoxCollider2D& box) const
    {
        const sf::Vector2f circleCenter = center();

        const sf::Vector2f boxMin = box.min();
        const sf::Vector2f boxMax = box.max();

        const float closestX = std::clamp(circleCenter.x, boxMin.x, boxMax.x);
        const float closestY = std::clamp(circleCenter.y, boxMin.y, boxMax.y);

        const float dx = circleCenter.x - closestX;
        const float dy = circleCenter.y - closestY;

        return dx * dx + dy * dy <= m_radius * m_radius;
    }

    sf::Vector2f CircleCollider2D::collisionResolutionVector(const BoxCollider2D& box) const
    {
        const sf::Vector2f circleCenter = center();

        const sf::Vector2f boxMin = box.min();
        const sf::Vector2f boxMax = box.max();

        const float closestX = std::clamp(circleCenter.x, boxMin.x, boxMax.x);
        const float closestY = std::clamp(circleCenter.y, boxMin.y, boxMax.y);

        const float dx = circleCenter.x - closestX;
        const float dy = circleCenter.y - closestY;

        const float distanceSquared = dx * dx + dy * dy;
        const float radiusSquared = m_radius * m_radius;

        if (distanceSquared > radiusSquared)
            return { 0.f, 0.f };

        constexpr float EPSILON = 0.0001f;

        if (distanceSquared > EPSILON)
        {
            const float distance = std::sqrt(distanceSquared);
            const float penetration = m_radius - distance;

            const sf::Vector2f normal{
                dx / distance,
                dy / distance
            };

            return normal * penetration;
        }

        // Special case: het middelpunt van de cirkel zit binnenin de box.
        // Dan is dx/dy nul, dus moeten we de dichtstbijzijnde zijde zoeken.
        const float distanceToLeft = circleCenter.x - boxMin.x;
        const float distanceToRight = boxMax.x - circleCenter.x;
        const float distanceToTop = circleCenter.y - boxMin.y;
        const float distanceToBottom = boxMax.y - circleCenter.y;

        float minDistance = distanceToLeft;
        sf::Vector2f resolution{ -(m_radius + distanceToLeft), 0.f };

        if (distanceToRight < minDistance)
        {
            minDistance = distanceToRight;
            resolution = { m_radius + distanceToRight, 0.f };
        }

        if (distanceToTop < minDistance)
        {
            minDistance = distanceToTop;
            resolution = { 0.f, -(m_radius + distanceToTop) };
        }

        if (distanceToBottom < minDistance)
        {
            resolution = { 0.f, m_radius + distanceToBottom };
        }

        return resolution;
    }
}

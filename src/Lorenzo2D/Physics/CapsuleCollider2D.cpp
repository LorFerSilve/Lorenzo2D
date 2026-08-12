#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr float MinimumDimension = 0.0001f;
        constexpr float MaximumRadius = std::numeric_limits<float>::max() * 0.5f;
        constexpr double Pi = 3.14159265358979323846;

        float sanitizedDimension(float value)
        {
            return std::isfinite(value) && value >= MinimumDimension ? value : MinimumDimension;
        }

        float sanitizedRadius(float value)
        {
            return std::min(sanitizedDimension(value), MaximumRadius);
        }

        sf::Vector2f scaled(sf::Vector2f value, double scalar)
        {
            const double x = static_cast<double>(value.x) * scalar;
            const double y = static_cast<double>(value.y) * scalar;
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > maximum ||
                std::fabs(y) > maximum)
            {
                const float invalid = std::numeric_limits<float>::quiet_NaN();
                return {invalid, invalid};
            }

            return {static_cast<float>(x), static_cast<float>(y)};
        }
    }

    CapsuleCollider2D::CapsuleCollider2D(float radius, float height)
        : Collider2D(ColliderType::Capsule), m_radius(sanitizedRadius(radius)),
          m_height(sanitizedDimension(height))
    {
        m_height = std::max(m_height, 2.f * m_radius);
    }

    float CapsuleCollider2D::radius() const
    {
        return m_radius;
    }

    void CapsuleCollider2D::setRadius(float radius)
    {
        m_radius = sanitizedRadius(radius);
        m_height = std::max(m_height, 2.f * m_radius);
    }

    float CapsuleCollider2D::height() const
    {
        return m_height;
    }

    void CapsuleCollider2D::setHeight(float height)
    {
        m_height = std::max(sanitizedDimension(height), 2.f * m_radius);
    }

    sf::Vector2f CapsuleCollider2D::center() const
    {
        return worldPosition();
    }

    float CapsuleCollider2D::worldRadius() const
    {
        const sf::Vector2f scale = worldScale();
        const double radius = static_cast<double>(m_radius) * std::max(scale.x, scale.y);

        if (!std::isfinite(radius) || radius > std::numeric_limits<float>::max())
            return std::numeric_limits<float>::quiet_NaN();

        return static_cast<float>(radius);
    }

    std::array<sf::Vector2f, 2> CapsuleCollider2D::worldSegment() const
    {
        const sf::Vector2f capsuleCenter = center();
        const sf::Vector2f scale = worldScale();
        const double halfSegment =
            static_cast<double>(std::max(0.f, m_height * 0.5f - m_radius)) * scale.y;
        const double radians = static_cast<double>(worldRotation()) * Pi / 180.0;
        const sf::Vector2f axis = {static_cast<float>(-std::sin(radians)),
                                   static_cast<float>(std::cos(radians))};
        const sf::Vector2f extent = scaled(axis, halfSegment);
        return {capsuleCenter - extent, capsuleCenter + extent};
    }

    sf::Vector2f CapsuleCollider2D::min() const
    {
        const auto segment = worldSegment();
        const float radiusValue = worldRadius();
        return {std::min(segment[0].x, segment[1].x) - radiusValue,
                std::min(segment[0].y, segment[1].y) - radiusValue};
    }

    sf::Vector2f CapsuleCollider2D::max() const
    {
        const auto segment = worldSegment();
        const float radiusValue = worldRadius();
        return {std::max(segment[0].x, segment[1].x) + radiusValue,
                std::max(segment[0].y, segment[1].y) + radiusValue};
    }
}

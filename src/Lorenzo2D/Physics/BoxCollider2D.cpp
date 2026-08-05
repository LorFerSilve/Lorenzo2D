#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr float MIN_DIMENSION = 0.0001f;

        float sanitizeDimension(float dimension)
        {
            if (!std::isfinite(dimension) || dimension < MIN_DIMENSION) return MIN_DIMENSION;

            return dimension;
        }

        sf::Vector2f sanitizeSize(sf::Vector2f size)
        {
            return {sanitizeDimension(size.x), sanitizeDimension(size.y)};
        }

        sf::Vector2f rotate(sf::Vector2f value, double radians)
        {
            const double cosine = std::cos(radians);
            const double sine = std::sin(radians);
            const double x =
                static_cast<double>(value.x) * cosine - static_cast<double>(value.y) * sine;
            const double y =
                static_cast<double>(value.x) * sine + static_cast<double>(value.y) * cosine;
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

    BoxCollider2D::BoxCollider2D(sf::Vector2f size)
        : Collider2D(ColliderType::Box), m_size(sanitizeSize(size))
    {
    }

    const sf::Vector2f& BoxCollider2D::size() const
    {
        return m_size;
    }

    void BoxCollider2D::setSize(sf::Vector2f size)
    {
        m_size = sanitizeSize(size);
    }

    sf::Vector2f BoxCollider2D::center() const
    {
        return worldPosition();
    }

    sf::Vector2f BoxCollider2D::worldHalfExtents() const
    {
        const sf::Vector2f scale = worldScale();
        const double x = static_cast<double>(m_size.x) * scale.x * 0.5;
        const double y = static_cast<double>(m_size.y) * scale.y * 0.5;
        const double maximum = static_cast<double>(std::numeric_limits<float>::max());

        if (!std::isfinite(x) || !std::isfinite(y) || x > maximum || y > maximum)
        {
            const float invalid = std::numeric_limits<float>::quiet_NaN();
            return {invalid, invalid};
        }

        return {static_cast<float>(x), static_cast<float>(y)};
    }

    std::array<sf::Vector2f, 4> BoxCollider2D::corners() const
    {
        const sf::Vector2f boxCenter = center();
        const sf::Vector2f half = worldHalfExtents();
        const double radians =
            static_cast<double>(worldRotation()) * 3.14159265358979323846 / 180.0;
        const std::array<sf::Vector2f, 4> localCorners = {
            sf::Vector2f{-half.x, -half.y}, sf::Vector2f{half.x, -half.y},
            sf::Vector2f{half.x, half.y}, sf::Vector2f{-half.x, half.y}};
        std::array<sf::Vector2f, 4> result{};

        for (std::size_t index = 0; index < localCorners.size(); ++index)
        {
            result[index] = boxCenter + rotate(localCorners[index], radians);
        }

        return result;
    }

    sf::Vector2f BoxCollider2D::min() const
    {
        const std::array<sf::Vector2f, 4> boxCorners = corners();
        sf::Vector2f minimum = boxCorners.front();

        for (const sf::Vector2f corner : boxCorners)
        {
            minimum.x = std::min(minimum.x, corner.x);
            minimum.y = std::min(minimum.y, corner.y);
        }

        return minimum;
    }

    sf::Vector2f BoxCollider2D::max() const
    {
        const std::array<sf::Vector2f, 4> boxCorners = corners();
        sf::Vector2f maximum = boxCorners.front();

        for (const sf::Vector2f corner : boxCorners)
        {
            maximum.x = std::max(maximum.x, corner.x);
            maximum.y = std::max(maximum.y, corner.y);
        }

        return maximum;
    }

    bool BoxCollider2D::overlaps(const BoxCollider2D& other) const
    {
        CollisionManifold2D manifold;
        return computeCollisionManifold(*this, other, manifold);
    }
}

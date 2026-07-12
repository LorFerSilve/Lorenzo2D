#include <Lorenzo2D/Physics/BoxCollider2D.hpp>

#include <cmath>

namespace l2d
{
    namespace
    {
        constexpr float MIN_DIMENSION = 0.0001f;

        float sanitizeDimension(float dimension)
        {
            if (!std::isfinite(dimension) || dimension < MIN_DIMENSION)
                return MIN_DIMENSION;

            return dimension;
        }

        sf::Vector2f sanitizeSize(sf::Vector2f size)
        {
            return {
                sanitizeDimension(size.x),
                sanitizeDimension(size.y)
            };
        }
    }

    BoxCollider2D::BoxCollider2D(sf::Vector2f size)
        : Collider2D(ColliderType::Box),
        m_size(sanitizeSize(size))
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

    sf::Vector2f BoxCollider2D::min() const
    {
        return worldPosition();
    }

    sf::Vector2f BoxCollider2D::max() const
    {
        const sf::Vector2f position = worldPosition();

        return {
            position.x + m_size.x,
            position.y + m_size.y
        };
    }

    bool BoxCollider2D::overlaps(const BoxCollider2D& other) const
    {
        const sf::Vector2f aMin = min();
        const sf::Vector2f aMax = max();

        const sf::Vector2f bMin = other.min();
        const sf::Vector2f bMax = other.max();

        return aMin.x < bMax.x &&
            aMax.x > bMin.x &&
            aMin.y < bMax.y &&
            aMax.y > bMin.y;
    }
}

#include <Lorenzo2D/Physics/BoxCollider2D.hpp>

namespace l2d
{
    BoxCollider2D::BoxCollider2D(sf::Vector2f size)
        : Collider2D(ColliderType::Box),
        m_size(size)
    {
    }

    const sf::Vector2f& BoxCollider2D::size() const
    {
        return m_size;
    }

    void BoxCollider2D::setSize(sf::Vector2f size)
    {
        m_size = size;
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
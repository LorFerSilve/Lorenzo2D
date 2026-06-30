#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

namespace l2d
{
    Collider2D::Collider2D(ColliderType type)
        : m_type(type),
        m_offset(0.f, 0.f),
        m_isColliding(false)
    {
    }

    ColliderType Collider2D::type() const
    {
        return m_type;
    }

    const sf::Vector2f& Collider2D::offset() const
    {
        return m_offset;
    }

    void Collider2D::setOffset(sf::Vector2f offset)
    {
        m_offset = offset;
    }

    sf::Vector2f Collider2D::worldPosition() const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return m_offset;

        return gameObject->transform.position() + m_offset;
    }

    bool Collider2D::isColliding() const
    {
        return m_isColliding;
    }

    void Collider2D::setColliding(bool colliding)
    {
        m_isColliding = colliding;
    }
}
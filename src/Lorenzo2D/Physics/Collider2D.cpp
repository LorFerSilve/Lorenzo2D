#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    namespace
    {
        float sanitizeUnitValue(float value)
        {
            if (!std::isfinite(value))
                return 0.f;

            return std::clamp(value, 0.f, 1.f);
        }

        float sanitizeOffsetComponent(float value)
        {
            if (!std::isfinite(value))
                return 0.f;

            return value;
        }
    }

    Collider2D::Collider2D(ColliderType type)
        : m_type(type),
        m_offset(0.f, 0.f),
        m_material(),
        m_filter(),
        m_isSensor(false),
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
        m_offset = {
            sanitizeOffsetComponent(offset.x),
            sanitizeOffsetComponent(offset.y)
        };
    }

    sf::Vector2f Collider2D::worldPosition() const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return m_offset;

        return gameObject->transform.position() + m_offset;
    }

    const PhysicsMaterial2D& Collider2D::material() const
    {
        return m_material;
    }

    void Collider2D::setMaterial(PhysicsMaterial2D material)
    {
        material.restitution = sanitizeUnitValue(material.restitution);
        material.staticFriction = sanitizeUnitValue(material.staticFriction);
        material.dynamicFriction = std::min(
            sanitizeUnitValue(material.dynamicFriction),
            material.staticFriction
        );

        m_material = material;
    }

    const CollisionFilter2D& Collider2D::filter() const
    {
        return m_filter;
    }

    void Collider2D::setFilter(CollisionFilter2D filter)
    {
        m_filter = filter;
    }

    bool Collider2D::canCollideWith(const Collider2D& other) const
    {
        return (m_filter.maskBits & other.m_filter.categoryBits) != 0u &&
            (other.m_filter.maskBits & m_filter.categoryBits) != 0u;
    }

    bool Collider2D::isSensor() const
    {
        return m_isSensor;
    }

    void Collider2D::setSensor(bool sensor)
    {
        m_isSensor = sensor;
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

#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        ColliderId allocateColliderId()
        {
            static std::atomic<ColliderId> nextId{1};
            ColliderId id = nextId.fetch_add(1, std::memory_order_relaxed);

            while (id == InvalidColliderId)
            {
                id = nextId.fetch_add(1, std::memory_order_relaxed);
            }

            return id;
        }

        bool checkedFloat(double value, float& result)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(value) || std::fabs(value) > maximum) return false;

            result = static_cast<float>(value);
            return true;
        }

        float sanitizeUnitValue(float value)
        {
            if (!std::isfinite(value)) return 0.f;

            return std::clamp(value, 0.f, 1.f);
        }

        float sanitizeOffsetComponent(float value)
        {
            if (!std::isfinite(value)) return 0.f;

            return value;
        }
    }

    Collider2D::Collider2D(ColliderType type)
        : m_id(allocateColliderId()), m_type(type), m_offset(0.f, 0.f), m_material(), m_filter(),
          m_isSensor(false), m_isColliding(false)
    {
    }

    ColliderId Collider2D::id() const
    {
        return m_id;
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
        m_offset = {sanitizeOffsetComponent(offset.x), sanitizeOffsetComponent(offset.y)};
    }

    sf::Vector2f Collider2D::worldPosition() const
    {
        return localToWorldPoint(m_offset);
    }

    sf::Vector2f Collider2D::worldScale() const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr) return {1.f, 1.f};

        const sf::Vector2f scale = gameObject->transform.scale();
        return {std::fabs(scale.x), std::fabs(scale.y)};
    }

    float Collider2D::worldRotation() const
    {
        const GameObject* gameObject = owner();
        return gameObject != nullptr ? gameObject->transform.rotation() : 0.f;
    }

    sf::Vector2f Collider2D::localToWorldPoint(sf::Vector2f localPoint) const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr) return localPoint;

        const sf::Vector2f position = gameObject->transform.position();
        const sf::Vector2f scale = gameObject->transform.scale();
        const double radians =
            static_cast<double>(gameObject->transform.rotation()) * 3.14159265358979323846 / 180.0;
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        const double scaledX = static_cast<double>(localPoint.x) * scale.x;
        const double scaledY = static_cast<double>(localPoint.y) * scale.y;
        const double worldX = static_cast<double>(position.x) + scaledX * cosine - scaledY * sine;
        const double worldY = static_cast<double>(position.y) + scaledX * sine + scaledY * cosine;
        sf::Vector2f result;

        if (!checkedFloat(worldX, result.x) || !checkedFloat(worldY, result.y))
        {
            const float invalid = std::numeric_limits<float>::quiet_NaN();
            return {invalid, invalid};
        }

        return result;
    }

    const PhysicsMaterial2D& Collider2D::material() const
    {
        return m_material;
    }

    void Collider2D::setMaterial(PhysicsMaterial2D material)
    {
        material.restitution = sanitizeUnitValue(material.restitution);
        material.staticFriction = sanitizeUnitValue(material.staticFriction);
        material.dynamicFriction =
            std::min(sanitizeUnitValue(material.dynamicFriction), material.staticFriction);

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

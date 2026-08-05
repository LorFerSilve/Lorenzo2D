#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Physics/PhysicsMaterial2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <limits>

namespace l2d
{
    class PhysicsWorld2D;

    enum class ColliderType
    {
        Box,
        Circle
    };

    struct CollisionFilter2D
    {
        std::uint32_t categoryBits = 1u;
        std::uint32_t maskBits = std::numeric_limits<std::uint32_t>::max();
    };

    class Collider2D : public Component
    {
      public:
        explicit Collider2D(ColliderType type);
        virtual ~Collider2D() = default;

        ColliderType type() const;

        const sf::Vector2f& offset() const;
        void setOffset(sf::Vector2f offset);

        sf::Vector2f worldPosition() const;

        const PhysicsMaterial2D& material() const;
        void setMaterial(PhysicsMaterial2D material);

        const CollisionFilter2D& filter() const;
        void setFilter(CollisionFilter2D filter);
        bool canCollideWith(const Collider2D& other) const;

        bool isSensor() const;
        void setSensor(bool sensor);

        bool isColliding() const;

      private:
        void setColliding(bool colliding);

      private:
        ColliderType m_type;
        sf::Vector2f m_offset;
        PhysicsMaterial2D m_material;
        CollisionFilter2D m_filter;
        bool m_isSensor;
        bool m_isColliding;

        friend class PhysicsWorld2D;
    };
}

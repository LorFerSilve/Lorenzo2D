#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class PhysicsWorld2D;

    enum class ColliderType
    {
        Box,
        Circle
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

        bool isColliding() const;

    private:
        void setColliding(bool colliding);

    private:
        ColliderType m_type;
        sf::Vector2f m_offset;
        bool m_isColliding;

        friend class PhysicsWorld2D;
    };
}
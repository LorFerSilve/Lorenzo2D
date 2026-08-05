#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>

namespace l2d
{
    using JointId = std::uint64_t;

    constexpr JointId InvalidJointId = 0;

    class DistanceJoint2D : public Component
    {
      public:
        explicit DistanceJoint2D(GameObjectId connectedObjectId = InvalidGameObjectId,
                                 float restLength = 0.f);

        JointId id() const;

        GameObjectId connectedObjectId() const;
        void setConnectedObjectId(GameObjectId objectId);

        const sf::Vector2f& localAnchor() const;
        void setLocalAnchor(sf::Vector2f anchor);

        const sf::Vector2f& connectedLocalAnchor() const;
        void setConnectedLocalAnchor(sf::Vector2f anchor);

        float restLength() const;
        void setRestLength(float length);

        float stiffness() const;
        void setStiffness(float stiffness);

        float damping() const;
        void setDamping(float damping);

        bool collideConnected() const;
        void setCollideConnected(bool collideConnected);

      private:
        JointId m_id;
        GameObjectId m_connectedObjectId;
        sf::Vector2f m_localAnchor;
        sf::Vector2f m_connectedLocalAnchor;
        float m_restLength;
        float m_stiffness;
        float m_damping;
        bool m_collideConnected;
    };
}

#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class PhysicsWorld2D;

    enum class BodyType2D
    {
        Static,
        Kinematic,
        Dynamic
    };

    class RigidBody2D : public Component
    {
    public:
        RigidBody2D();

        BodyType2D bodyType() const;
        void setBodyType(BodyType2D bodyType);

        const sf::Vector2f& velocity() const;
        void setVelocity(sf::Vector2f velocity);
        void addVelocity(sf::Vector2f velocity);

        const sf::Vector2f& acceleration() const;
        void setAcceleration(sf::Vector2f acceleration);

        void addForce(sf::Vector2f force);
        void applyImpulse(sf::Vector2f impulse);
        void clearForces();

        float mass() const;
        void setMass(float mass);
        float inverseMass() const;

        bool useGravity() const;
        void setUseGravity(bool useGravity);

        float gravityScale() const;
        void setGravityScale(float gravityScale);

        bool isGrounded() const;

    private:
        void integrate(float deltaTime, sf::Vector2f worldGravity);
        void setGrounded(bool grounded);

    private:
        BodyType2D m_bodyType;

        sf::Vector2f m_velocity;
        sf::Vector2f m_acceleration;
        sf::Vector2f m_forceAccumulator;

        float m_mass;
        bool m_useGravity;
        float m_gravityScale;

        bool m_isGrounded;

        friend class PhysicsWorld2D;
    };
}

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
        void applyImpulseAtPoint(sf::Vector2f impulse, sf::Vector2f worldPoint);
        sf::Vector2f velocityAtWorldPoint(sf::Vector2f worldPoint) const;
        void clearForces();

        float mass() const;
        void setMass(float mass);
        float inverseMass() const;

        bool useGravity() const;
        void setUseGravity(bool useGravity);

        float gravityScale() const;
        void setGravityScale(float gravityScale);

        float angularVelocity() const;
        void setAngularVelocity(float angularVelocity);
        void addTorque(float torque);
        void applyAngularImpulse(float impulse);

        float inertia() const;
        void setInertia(float inertia);
        float inverseInertia() const;

        bool fixedRotation() const;
        void setFixedRotation(bool fixedRotation);

        bool allowsSleep() const;
        void setAllowsSleep(bool allowsSleep);
        bool isAwake() const;
        void wakeUp();
        void sleep();

        bool isGrounded() const;

      private:
        void integrate(float deltaTime, sf::Vector2f worldGravity);
        void setGrounded(bool grounded);
        void setSleepTimer(float timer);
        float sleepTimer() const;
        void applySolverImpulseAtPoint(sf::Vector2f impulse, sf::Vector2f worldPoint);

      private:
        BodyType2D m_bodyType;

        sf::Vector2f m_velocity;
        sf::Vector2f m_acceleration;
        sf::Vector2f m_forceAccumulator;
        float m_angularVelocity;
        float m_torqueAccumulator;

        float m_mass;
        float m_inertia;
        bool m_fixedRotation;
        bool m_useGravity;
        float m_gravityScale;

        bool m_allowsSleep;
        bool m_isAwake;
        float m_sleepTimer;
        bool m_isGrounded;

        friend class PhysicsWorld2D;
    };
}

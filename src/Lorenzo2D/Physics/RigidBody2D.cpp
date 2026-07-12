#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <cmath>

namespace l2d
{
    namespace
    {
        constexpr float GRAVITY = 980.f;
        constexpr float MIN_MASS = 0.0001f;
        constexpr float DEFAULT_GRAVITY_SCALE = 1.f;

        float sanitizeMass(float mass)
        {
            if (!std::isfinite(mass) || mass < MIN_MASS)
                return MIN_MASS;

            return mass;
        }
    }

    RigidBody2D::RigidBody2D()
        : m_velocity(0.f, 0.f),
        m_acceleration(0.f, 0.f),
        m_forceAccumulator(0.f, 0.f),
        m_mass(1.f),
        m_useGravity(false),
        m_gravityScale(1.f),
        m_isGrounded(false)
    {
    }

    const sf::Vector2f& RigidBody2D::velocity() const
    {
        return m_velocity;
    }

    void RigidBody2D::setVelocity(sf::Vector2f velocity)
    {
        m_velocity = velocity;
    }

    void RigidBody2D::addVelocity(sf::Vector2f velocity)
    {
        m_velocity += velocity;
    }

    const sf::Vector2f& RigidBody2D::acceleration() const
    {
        return m_acceleration;
    }

    void RigidBody2D::setAcceleration(sf::Vector2f acceleration)
    {
        m_acceleration = acceleration;
    }

    void RigidBody2D::addForce(sf::Vector2f force)
    {
        m_forceAccumulator += force;
    }

    float RigidBody2D::mass() const
    {
        return m_mass;
    }

    void RigidBody2D::setMass(float mass)
    {
        m_mass = sanitizeMass(mass);
    }

    bool RigidBody2D::useGravity() const
    {
        return m_useGravity;
    }

    void RigidBody2D::setUseGravity(bool useGravity)
    {
        m_useGravity = useGravity;
    }

    float RigidBody2D::gravityScale() const
    {
        return m_gravityScale;
    }

    void RigidBody2D::setGravityScale(float gravityScale)
    {
        if (!std::isfinite(gravityScale))
            gravityScale = DEFAULT_GRAVITY_SCALE;

        m_gravityScale = gravityScale;
    }

    bool RigidBody2D::isGrounded() const
    {
        return m_isGrounded;
    }

    void RigidBody2D::integrate(float deltaTime)
    {
        GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        sf::Vector2f totalAcceleration = m_acceleration;

        totalAcceleration += m_forceAccumulator * (1.f / m_mass);

        if (m_useGravity)
        {
            totalAcceleration.y += GRAVITY * m_gravityScale;
        }

        m_velocity += totalAcceleration * deltaTime;

        gameObject->transform.move(m_velocity * deltaTime);

        m_forceAccumulator = { 0.f, 0.f };
    }

    void RigidBody2D::setGrounded(bool grounded)
    {
        m_isGrounded = grounded;
    }
}

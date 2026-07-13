#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr float MIN_MASS = 0.0001f;
        constexpr float DEFAULT_GRAVITY_SCALE = 1.f;

        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        sf::Vector2f sanitizeVector(sf::Vector2f value)
        {
            if (!isFinite(value))
                return { 0.f, 0.f };

            return value;
        }

        float sanitizeMass(float mass)
        {
            if (!std::isfinite(mass) || mass < MIN_MASS)
                return MIN_MASS;

            return mass;
        }

        void moveSafely(
            GameObject& gameObject,
            sf::Vector2f direction,
            double scale
        )
        {
            if (!isFinite(direction) || !std::isfinite(scale))
                return;

            const sf::Vector2f position = gameObject.transform.position();
            const double x =
                static_cast<double>(position.x) +
                static_cast<double>(direction.x) * scale;
            const double y =
                static_cast<double>(position.y) +
                static_cast<double>(direction.y) * scale;
            const double maximum =
                static_cast<double>(std::numeric_limits<float>::max());

            if (
                !std::isfinite(x) ||
                !std::isfinite(y) ||
                std::fabs(x) > maximum ||
                std::fabs(y) > maximum
            )
            {
                return;
            }

            gameObject.transform.setPosition({
                static_cast<float>(x),
                static_cast<float>(y)
            });
        }
    }

    RigidBody2D::RigidBody2D()
        : m_bodyType(BodyType2D::Dynamic),
        m_velocity(0.f, 0.f),
        m_acceleration(0.f, 0.f),
        m_forceAccumulator(0.f, 0.f),
        m_mass(1.f),
        m_useGravity(false),
        m_gravityScale(1.f),
        m_isGrounded(false)
    {
    }

    BodyType2D RigidBody2D::bodyType() const
    {
        return m_bodyType;
    }

    void RigidBody2D::setBodyType(BodyType2D bodyType)
    {
        switch (bodyType)
        {
        case BodyType2D::Static:
        case BodyType2D::Kinematic:
        case BodyType2D::Dynamic:
            break;
        default:
            bodyType = BodyType2D::Dynamic;
            break;
        }

        m_bodyType = bodyType;

        if (m_bodyType != BodyType2D::Dynamic)
        {
            clearForces();
            m_isGrounded = false;
        }

        if (m_bodyType == BodyType2D::Static)
            m_velocity = { 0.f, 0.f };
    }

    const sf::Vector2f& RigidBody2D::velocity() const
    {
        return m_velocity;
    }

    void RigidBody2D::setVelocity(sf::Vector2f velocity)
    {
        if (m_bodyType == BodyType2D::Static)
        {
            m_velocity = { 0.f, 0.f };
            return;
        }

        m_velocity = sanitizeVector(velocity);
    }

    void RigidBody2D::addVelocity(sf::Vector2f velocity)
    {
        if (m_bodyType == BodyType2D::Static || !isFinite(velocity))
            return;

        const sf::Vector2f updatedVelocity = m_velocity + velocity;

        if (isFinite(updatedVelocity))
            m_velocity = updatedVelocity;
    }

    const sf::Vector2f& RigidBody2D::acceleration() const
    {
        return m_acceleration;
    }

    void RigidBody2D::setAcceleration(sf::Vector2f acceleration)
    {
        m_acceleration = sanitizeVector(acceleration);
    }

    void RigidBody2D::addForce(sf::Vector2f force)
    {
        if (m_bodyType != BodyType2D::Dynamic || !isFinite(force))
            return;

        const sf::Vector2f updatedForce = m_forceAccumulator + force;

        if (isFinite(updatedForce))
            m_forceAccumulator = updatedForce;
    }

    void RigidBody2D::applyImpulse(sf::Vector2f impulse)
    {
        if (m_bodyType != BodyType2D::Dynamic || !isFinite(impulse))
            return;

        const double inverseBodyMass = 1.0 / static_cast<double>(m_mass);
        const double x =
            static_cast<double>(m_velocity.x) +
            static_cast<double>(impulse.x) * inverseBodyMass;
        const double y =
            static_cast<double>(m_velocity.y) +
            static_cast<double>(impulse.y) * inverseBodyMass;
        const double maximum =
            static_cast<double>(std::numeric_limits<float>::max());

        if (
            !std::isfinite(x) ||
            !std::isfinite(y) ||
            std::fabs(x) > maximum ||
            std::fabs(y) > maximum
        )
        {
            return;
        }

        m_velocity = {
            static_cast<float>(x),
            static_cast<float>(y)
        };
    }

    void RigidBody2D::clearForces()
    {
        m_forceAccumulator = { 0.f, 0.f };
    }

    float RigidBody2D::mass() const
    {
        return m_mass;
    }

    void RigidBody2D::setMass(float mass)
    {
        m_mass = sanitizeMass(mass);
    }

    float RigidBody2D::inverseMass() const
    {
        if (m_bodyType != BodyType2D::Dynamic)
            return 0.f;

        return 1.f / m_mass;
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

    void RigidBody2D::integrate(
        float deltaTime,
        sf::Vector2f worldGravity
    )
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.f)
            return;

        GameObject* gameObject = owner();

        if (m_bodyType == BodyType2D::Static)
        {
            m_velocity = { 0.f, 0.f };
        }
        else if (m_bodyType == BodyType2D::Kinematic)
        {
            if (gameObject != nullptr)
            {
                moveSafely(
                    *gameObject,
                    m_velocity,
                    static_cast<double>(deltaTime)
                );
            }
        }
        else
        {
            const sf::Vector2f gravity = sanitizeVector(worldGravity);
            const double inverseBodyMass =
                1.0 / static_cast<double>(m_mass);
            double accelerationX =
                static_cast<double>(m_acceleration.x) +
                static_cast<double>(m_forceAccumulator.x) *
                    inverseBodyMass;
            double accelerationY =
                static_cast<double>(m_acceleration.y) +
                static_cast<double>(m_forceAccumulator.y) *
                    inverseBodyMass;

            if (m_useGravity)
            {
                accelerationX +=
                    static_cast<double>(gravity.x) *
                    static_cast<double>(m_gravityScale);
                accelerationY +=
                    static_cast<double>(gravity.y) *
                    static_cast<double>(m_gravityScale);
            }

            const double nextVelocityX =
                static_cast<double>(m_velocity.x) +
                accelerationX * static_cast<double>(deltaTime);
            const double nextVelocityY =
                static_cast<double>(m_velocity.y) +
                accelerationY * static_cast<double>(deltaTime);
            const double maximum =
                static_cast<double>(std::numeric_limits<float>::max());

            if (
                std::isfinite(nextVelocityX) &&
                std::isfinite(nextVelocityY) &&
                std::fabs(nextVelocityX) <= maximum &&
                std::fabs(nextVelocityY) <= maximum
            )
            {
                m_velocity = {
                    static_cast<float>(nextVelocityX),
                    static_cast<float>(nextVelocityY)
                };
            }

            if (gameObject != nullptr)
            {
                moveSafely(
                    *gameObject,
                    m_velocity,
                    static_cast<double>(deltaTime)
                );
            }
        }

        clearForces();
    }

    void RigidBody2D::setGrounded(bool grounded)
    {
        m_isGrounded =
            m_bodyType == BodyType2D::Dynamic && grounded;
    }
}

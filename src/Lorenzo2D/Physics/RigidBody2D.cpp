#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr float MIN_MASS = 0.0001f;
        constexpr float MIN_INERTIA = 0.0001f;
        constexpr float DEFAULT_GRAVITY_SCALE = 1.f;
        constexpr double RADIANS_TO_DEGREES = 57.295779513082320876;

        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        sf::Vector2f sanitizeVector(sf::Vector2f value)
        {
            return isFinite(value) ? value : sf::Vector2f{0.f, 0.f};
        }

        float sanitizePositive(float value, float minimum)
        {
            return std::isfinite(value) && value >= minimum ? value : minimum;
        }

        bool checkedFloat(double value, float& result)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(value) || std::fabs(value) > maximum) return false;

            result = static_cast<float>(value);
            return true;
        }

        void moveSafely(GameObject& gameObject, sf::Vector2f direction, double scale)
        {
            if (!isFinite(direction) || !std::isfinite(scale)) return;

            const sf::Vector2f position = gameObject.transform.position();
            sf::Vector2f nextPosition;

            if (!checkedFloat(static_cast<double>(position.x) +
                                  static_cast<double>(direction.x) * scale,
                              nextPosition.x) ||
                !checkedFloat(static_cast<double>(position.y) +
                                  static_cast<double>(direction.y) * scale,
                              nextPosition.y))
            {
                return;
            }

            gameObject.transform.setPosition(nextPosition);
        }

        void rotateSafely(GameObject& gameObject, float angularVelocity, double deltaTime)
        {
            const double deltaDegrees =
                static_cast<double>(angularVelocity) * deltaTime * RADIANS_TO_DEGREES;
            float rotationDelta = 0.f;

            if (checkedFloat(deltaDegrees, rotationDelta))
                gameObject.transform.rotate(rotationDelta);
        }
    }

    RigidBody2D::RigidBody2D()
        : m_bodyType(BodyType2D::Dynamic), m_velocity(0.f, 0.f), m_acceleration(0.f, 0.f),
          m_forceAccumulator(0.f, 0.f), m_angularVelocity(0.f), m_torqueAccumulator(0.f),
          m_mass(1.f), m_inertia(1.f), m_fixedRotation(true), m_useGravity(false),
          m_gravityScale(1.f), m_allowsSleep(true), m_isAwake(true), m_sleepTimer(0.f),
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
        clearForces();
        m_isGrounded = false;
        m_sleepTimer = 0.f;

        if (m_bodyType == BodyType2D::Static)
        {
            m_velocity = {0.f, 0.f};
            m_angularVelocity = 0.f;
            m_isAwake = false;
        }
        else
        {
            m_isAwake = true;
        }
    }

    const sf::Vector2f& RigidBody2D::velocity() const
    {
        return m_velocity;
    }

    void RigidBody2D::setVelocity(sf::Vector2f velocity)
    {
        if (m_bodyType == BodyType2D::Static)
        {
            m_velocity = {0.f, 0.f};
            return;
        }

        m_velocity = sanitizeVector(velocity);
        wakeUp();
    }

    void RigidBody2D::addVelocity(sf::Vector2f velocity)
    {
        if (m_bodyType == BodyType2D::Static || !isFinite(velocity)) return;

        const sf::Vector2f updatedVelocity = m_velocity + velocity;

        if (isFinite(updatedVelocity))
        {
            m_velocity = updatedVelocity;
            wakeUp();
        }
    }

    const sf::Vector2f& RigidBody2D::acceleration() const
    {
        return m_acceleration;
    }

    void RigidBody2D::setAcceleration(sf::Vector2f acceleration)
    {
        m_acceleration = sanitizeVector(acceleration);

        if (m_bodyType == BodyType2D::Dynamic && m_acceleration != sf::Vector2f{0.f, 0.f})
        {
            wakeUp();
        }
    }

    void RigidBody2D::addForce(sf::Vector2f force)
    {
        if (m_bodyType != BodyType2D::Dynamic || !isFinite(force)) return;

        const sf::Vector2f updatedForce = m_forceAccumulator + force;

        if (isFinite(updatedForce))
        {
            m_forceAccumulator = updatedForce;
            wakeUp();
        }
    }

    void RigidBody2D::applyImpulse(sf::Vector2f impulse)
    {
        const GameObject* gameObject = owner();
        const sf::Vector2f point =
            gameObject != nullptr ? gameObject->transform.position() : sf::Vector2f{0.f, 0.f};
        applyImpulseAtPoint(impulse, point);
    }

    void RigidBody2D::applyImpulseAtPoint(sf::Vector2f impulse, sf::Vector2f worldPoint)
    {
        if (m_bodyType != BodyType2D::Dynamic || !isFinite(impulse) || !isFinite(worldPoint))
        {
            return;
        }

        wakeUp();
        applySolverImpulseAtPoint(impulse, worldPoint);
    }

    void RigidBody2D::clearForces()
    {
        m_forceAccumulator = {0.f, 0.f};
        m_torqueAccumulator = 0.f;
    }

    float RigidBody2D::mass() const
    {
        return m_mass;
    }

    void RigidBody2D::setMass(float mass)
    {
        m_mass = sanitizePositive(mass, MIN_MASS);
        wakeUp();
    }

    float RigidBody2D::inverseMass() const
    {
        return m_bodyType == BodyType2D::Dynamic ? 1.f / m_mass : 0.f;
    }

    bool RigidBody2D::useGravity() const
    {
        return m_useGravity;
    }

    void RigidBody2D::setUseGravity(bool useGravity)
    {
        m_useGravity = useGravity;
        if (useGravity) wakeUp();
    }

    float RigidBody2D::gravityScale() const
    {
        return m_gravityScale;
    }

    void RigidBody2D::setGravityScale(float gravityScale)
    {
        m_gravityScale = std::isfinite(gravityScale) ? gravityScale : DEFAULT_GRAVITY_SCALE;
        wakeUp();
    }

    float RigidBody2D::angularVelocity() const
    {
        return m_angularVelocity;
    }

    void RigidBody2D::setAngularVelocity(float angularVelocity)
    {
        if (m_bodyType == BodyType2D::Static || m_fixedRotation)
        {
            m_angularVelocity = 0.f;
            return;
        }

        m_angularVelocity = std::isfinite(angularVelocity) ? angularVelocity : 0.f;
        wakeUp();
    }

    void RigidBody2D::addTorque(float torque)
    {
        if (m_bodyType != BodyType2D::Dynamic || m_fixedRotation || !std::isfinite(torque)) return;

        const double updated = static_cast<double>(m_torqueAccumulator) + torque;
        float updatedTorque = 0.f;

        if (checkedFloat(updated, updatedTorque))
        {
            m_torqueAccumulator = updatedTorque;
            wakeUp();
        }
    }

    void RigidBody2D::applyAngularImpulse(float impulse)
    {
        if (m_bodyType != BodyType2D::Dynamic || m_fixedRotation || !std::isfinite(impulse)) return;

        const double updated =
            static_cast<double>(m_angularVelocity) + static_cast<double>(impulse) / m_inertia;
        float updatedVelocity = 0.f;

        if (checkedFloat(updated, updatedVelocity))
        {
            m_angularVelocity = updatedVelocity;
            wakeUp();
        }
    }

    float RigidBody2D::inertia() const
    {
        return m_inertia;
    }

    void RigidBody2D::setInertia(float inertia)
    {
        m_inertia = sanitizePositive(inertia, MIN_INERTIA);
        wakeUp();
    }

    float RigidBody2D::inverseInertia() const
    {
        return m_bodyType == BodyType2D::Dynamic && !m_fixedRotation ? 1.f / m_inertia : 0.f;
    }

    bool RigidBody2D::fixedRotation() const
    {
        return m_fixedRotation;
    }

    void RigidBody2D::setFixedRotation(bool fixedRotation)
    {
        m_fixedRotation = fixedRotation;
        if (fixedRotation) m_angularVelocity = 0.f;
        wakeUp();
    }

    bool RigidBody2D::allowsSleep() const
    {
        return m_allowsSleep;
    }

    void RigidBody2D::setAllowsSleep(bool allowsSleep)
    {
        m_allowsSleep = allowsSleep;
        if (!allowsSleep) wakeUp();
    }

    bool RigidBody2D::isAwake() const
    {
        return m_bodyType != BodyType2D::Dynamic || m_isAwake;
    }

    void RigidBody2D::wakeUp()
    {
        if (m_bodyType == BodyType2D::Static) return;

        m_isAwake = true;
        m_sleepTimer = 0.f;
    }

    void RigidBody2D::sleep()
    {
        if (m_bodyType != BodyType2D::Dynamic || !m_allowsSleep) return;

        m_isAwake = false;
        m_sleepTimer = 0.f;
        m_velocity = {0.f, 0.f};
        m_angularVelocity = 0.f;
        clearForces();
    }

    bool RigidBody2D::isGrounded() const
    {
        return m_isGrounded;
    }

    void RigidBody2D::integrate(float deltaTime, sf::Vector2f worldGravity)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.f) return;

        GameObject* gameObject = owner();

        if (m_bodyType == BodyType2D::Static)
        {
            m_velocity = {0.f, 0.f};
            m_angularVelocity = 0.f;
            clearForces();
            return;
        }

        if (m_bodyType == BodyType2D::Dynamic && !m_isAwake) return;

        if (m_bodyType == BodyType2D::Dynamic)
        {
            const sf::Vector2f gravity = sanitizeVector(worldGravity);
            const double inverseBodyMass = 1.0 / static_cast<double>(m_mass);
            double accelerationX = static_cast<double>(m_acceleration.x) +
                                   static_cast<double>(m_forceAccumulator.x) * inverseBodyMass;
            double accelerationY = static_cast<double>(m_acceleration.y) +
                                   static_cast<double>(m_forceAccumulator.y) * inverseBodyMass;

            if (m_useGravity)
            {
                accelerationX +=
                    static_cast<double>(gravity.x) * static_cast<double>(m_gravityScale);
                accelerationY +=
                    static_cast<double>(gravity.y) * static_cast<double>(m_gravityScale);
            }

            sf::Vector2f nextVelocity;

            if (checkedFloat(static_cast<double>(m_velocity.x) + accelerationX * deltaTime,
                             nextVelocity.x) &&
                checkedFloat(static_cast<double>(m_velocity.y) + accelerationY * deltaTime,
                             nextVelocity.y))
            {
                m_velocity = nextVelocity;
            }

            float nextAngularVelocity = 0.f;

            if (!m_fixedRotation &&
                checkedFloat(static_cast<double>(m_angularVelocity) +
                                 static_cast<double>(m_torqueAccumulator) / m_inertia * deltaTime,
                             nextAngularVelocity))
            {
                m_angularVelocity = nextAngularVelocity;
            }
        }

        if (gameObject != nullptr)
        {
            moveSafely(*gameObject, m_velocity, deltaTime);
            if (!m_fixedRotation) rotateSafely(*gameObject, m_angularVelocity, deltaTime);
        }
    }

    void RigidBody2D::setGrounded(bool grounded)
    {
        m_isGrounded = m_bodyType == BodyType2D::Dynamic && grounded;
    }

    void RigidBody2D::setSleepTimer(float timer)
    {
        m_sleepTimer = std::isfinite(timer) && timer >= 0.f ? timer : 0.f;
    }

    float RigidBody2D::sleepTimer() const
    {
        return m_sleepTimer;
    }

    sf::Vector2f RigidBody2D::velocityAtWorldPoint(sf::Vector2f worldPoint) const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr || !isFinite(worldPoint)) return m_velocity;

        const sf::Vector2f offset = worldPoint - gameObject->transform.position();
        const double angularVelocity = m_fixedRotation ? 0.0 : m_angularVelocity;
        const double x = static_cast<double>(m_velocity.x) - angularVelocity * offset.y;
        const double y = static_cast<double>(m_velocity.y) + angularVelocity * offset.x;
        sf::Vector2f result;

        return checkedFloat(x, result.x) && checkedFloat(y, result.y) ? result : m_velocity;
    }

    void RigidBody2D::applySolverImpulseAtPoint(sf::Vector2f impulse, sf::Vector2f worldPoint)
    {
        if (m_bodyType != BodyType2D::Dynamic || !isFinite(impulse) || !isFinite(worldPoint))
        {
            return;
        }

        sf::Vector2f nextVelocity;

        if (!checkedFloat(static_cast<double>(m_velocity.x) +
                              static_cast<double>(impulse.x) / static_cast<double>(m_mass),
                          nextVelocity.x) ||
            !checkedFloat(static_cast<double>(m_velocity.y) +
                              static_cast<double>(impulse.y) / static_cast<double>(m_mass),
                          nextVelocity.y))
        {
            return;
        }

        float nextAngularVelocity = m_angularVelocity;
        const GameObject* gameObject = owner();

        if (gameObject != nullptr && !m_fixedRotation)
        {
            const sf::Vector2f lever = worldPoint - gameObject->transform.position();
            const double angularImpulse =
                static_cast<double>(lever.x) * impulse.y - static_cast<double>(lever.y) * impulse.x;

            if (!checkedFloat(static_cast<double>(m_angularVelocity) +
                                  angularImpulse / static_cast<double>(m_inertia),
                              nextAngularVelocity))
            {
                return;
            }
        }

        m_velocity = nextVelocity;
        m_angularVelocity = nextAngularVelocity;
    }
}

#include <Lorenzo2D/Movement/TopDownController2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float length(sf::Vector2f value)
        {
            return static_cast<float>(
                std::hypot(static_cast<double>(value.x), static_cast<double>(value.y)));
        }

        sf::Vector2f moveTowards(sf::Vector2f current, sf::Vector2f target, float maximumDelta)
        {
            const sf::Vector2f difference = target - current;
            const float distance = length(difference);
            if (distance <= maximumDelta || distance <= std::numeric_limits<float>::epsilon())
                return target;
            return current + difference * (maximumDelta / distance);
        }

        sf::Vector2f sanitizedInput(sf::Vector2f input, float deadzone)
        {
            const float magnitude = length(input);
            if (!finite(input) || !std::isfinite(magnitude) || magnitude <= deadzone) return {};

            const sf::Vector2f direction = input / magnitude;
            const float clampedMagnitude = std::min(magnitude, 1.f);
            const float remappedMagnitude = (clampedMagnitude - deadzone) / (1.f - deadzone);
            return direction * remappedMagnitude;
        }
    }

    CharacterMotorConfig2D topDownCharacterMotorConfig2D()
    {
        CharacterMotorConfig2D config;
        config.groundProbeDistance = 0.f;
        config.snapToGround = false;
        config.inheritPlatformTranslation = false;
        return config;
    }

    TopDownController2D::TopDownController2D() = default;

    TopDownController2D::TopDownController2D(TopDownControllerConfig2D config)
    {
        (void)setConfig(std::move(config));
    }

    bool TopDownController2D::isValidConfig(const TopDownControllerConfig2D& config)
    {
        return std::isfinite(config.maximumSpeed) && config.maximumSpeed > 0.f &&
               std::isfinite(config.acceleration) && config.acceleration > 0.f &&
               std::isfinite(config.deceleration) && config.deceleration > 0.f &&
               std::isfinite(config.inputDeadzone) && config.inputDeadzone >= 0.f &&
               config.inputDeadzone < 1.f && std::isfinite(config.maximumDeltaTime) &&
               config.maximumDeltaTime > 0.f;
    }

    bool TopDownController2D::setConfig(TopDownControllerConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        m_config = std::move(config);
        stop();
        return true;
    }

    const TopDownControllerConfig2D& TopDownController2D::config() const
    {
        return m_config;
    }

    TopDownMoveResult2D TopDownController2D::move(const PhysicsQueryContext2D& queries,
                                                  sf::Vector2f input, float fixedDeltaTime)
    {
        TopDownMoveResult2D result;
        result.state = m_state;
        if (!isValidConfig(m_config) || !finite(input) || !std::isfinite(fixedDeltaTime) ||
            fixedDeltaTime <= 0.f || fixedDeltaTime > m_config.maximumDeltaTime)
            return result;

        GameObject* character = owner();
        CharacterMotor2D* motor =
            character == nullptr ? nullptr : character->getComponent<CharacterMotor2D>();
        if (motor == nullptr) return result;

        const sf::Vector2f movementInput = sanitizedInput(input, m_config.inputDeadzone);
        const sf::Vector2f desiredVelocity = movementInput * m_config.maximumSpeed;
        const float rate =
            movementInput == sf::Vector2f{} ? m_config.deceleration : m_config.acceleration;
        sf::Vector2f nextVelocity =
            moveTowards(m_state.velocity, desiredVelocity, rate * fixedDeltaTime);

        CharacterMoveResult2D motorResult = motor->move(queries, nextVelocity * fixedDeltaTime);
        if (!motorResult.succeeded) return result;

        for (const CharacterMotorContact2D& contact : motorResult.contacts)
        {
            if (contact.recoveredOverlap || contact.groundProbe) continue;
            const float inward =
                nextVelocity.x * contact.normal.x + nextVelocity.y * contact.normal.y;
            if (inward < 0.f) nextVelocity -= contact.normal * inward;
        }

        m_state.velocity = nextVelocity;
        if (movementInput != sf::Vector2f{})
        {
            const float magnitude = length(movementInput);
            m_state.facingDirection = movementInput / magnitude;
        }
        m_state.moving = length(m_state.velocity) > motor->config().minimumMoveDistance;

        result.succeeded = true;
        result.inputDirection = movementInput;
        result.desiredVelocity = desiredVelocity;
        result.motorResult = std::move(motorResult);
        result.state = m_state;
        return result;
    }

    const TopDownControllerState2D& TopDownController2D::state() const
    {
        return m_state;
    }

    void TopDownController2D::stop()
    {
        m_state.velocity = {};
        m_state.moving = false;
    }
}

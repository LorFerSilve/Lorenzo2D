#include <Lorenzo2D/Movement/PlatformerController2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float dot(sf::Vector2f first, sf::Vector2f second)
        {
            return first.x * second.x + first.y * second.y;
        }

        float moveTowards(float current, float target, float maximumDelta)
        {
            if (current < target) return std::min(current + maximumDelta, target);
            if (current > target) return std::max(current - maximumDelta, target);
            return target;
        }

        float sanitizedInput(float input, float deadzone)
        {
            if (!std::isfinite(input) || std::abs(input) <= deadzone) return 0.f;
            const float magnitude = std::min(std::abs(input), 1.f);
            const float remapped = (magnitude - deadzone) / (1.f - deadzone);
            return std::copysign(remapped, input);
        }
    }

    CharacterMotorConfig2D platformerCharacterMotorConfig2D(
        std::uint32_t oneWayPlatformCategoryMask)
    {
        CharacterMotorConfig2D config;
        config.groundProbeDistance = 0.15f;
        config.maximumSlopeAngleDegrees = 50.f;
        config.oneWayPlatformCategoryMask = oneWayPlatformCategoryMask;
        config.snapToGround = true;
        config.inheritPlatformTranslation = true;
        return config;
    }

    PlatformerController2D::PlatformerController2D() = default;

    PlatformerController2D::PlatformerController2D(PlatformerControllerConfig2D config)
    {
        (void)setConfig(std::move(config));
    }

    bool PlatformerController2D::isValidConfig(const PlatformerControllerConfig2D& config)
    {
        return std::isfinite(config.maximumRunSpeed) && config.maximumRunSpeed > 0.f &&
               std::isfinite(config.groundAcceleration) && config.groundAcceleration > 0.f &&
               std::isfinite(config.groundDeceleration) && config.groundDeceleration > 0.f &&
               std::isfinite(config.airAcceleration) && config.airAcceleration >= 0.f &&
               std::isfinite(config.airDeceleration) && config.airDeceleration >= 0.f &&
               std::isfinite(config.gravity) && config.gravity > 0.f &&
               std::isfinite(config.maximumFallSpeed) && config.maximumFallSpeed > 0.f &&
               std::isfinite(config.jumpSpeed) && config.jumpSpeed > 0.f &&
               std::isfinite(config.jumpCutMultiplier) && config.jumpCutMultiplier > 0.f &&
               config.jumpCutMultiplier <= 1.f && std::isfinite(config.coyoteTime) &&
               config.coyoteTime >= 0.f && std::isfinite(config.jumpBufferTime) &&
               config.jumpBufferTime >= 0.f && std::isfinite(config.dropThroughTime) &&
               config.dropThroughTime >= 0.f && std::isfinite(config.stepHeight) &&
               config.stepHeight >= 0.f && std::isfinite(config.stepDownDistance) &&
               config.stepDownDistance >= 0.f && std::isfinite(config.inputDeadzone) &&
               config.inputDeadzone >= 0.f && config.inputDeadzone < 1.f &&
               std::isfinite(config.maximumDeltaTime) && config.maximumDeltaTime > 0.f;
    }

    bool PlatformerController2D::setConfig(PlatformerControllerConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        m_config = std::move(config);
        reset();
        return true;
    }

    const PlatformerControllerConfig2D& PlatformerController2D::config() const
    {
        return m_config;
    }

    PlatformerMoveResult2D PlatformerController2D::move(const PhysicsQueryContext2D& queries,
                                                        const PlatformerInput2D& input,
                                                        float fixedDeltaTime)
    {
        PlatformerMoveResult2D result;
        result.state = m_state;
        if (!isValidConfig(m_config) || !std::isfinite(input.horizontal) ||
            !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.f ||
            fixedDeltaTime > m_config.maximumDeltaTime)
            return result;

        GameObject* character = owner();
        CharacterMotor2D* motor =
            character == nullptr ? nullptr : character->getComponent<CharacterMotor2D>();
        if (motor == nullptr) return result;

        const bool wasGrounded = m_state.grounded;
        const bool wasTouchingWall = m_state.touchingWall;
        const bool wasTouchingCeiling = m_state.touchingCeiling;
        const CharacterMotorState2D motorState = motor->state();
        const bool groundedAtStart = motorState.grounded || m_state.grounded;
        const sf::Vector2f up = motor->config().upDirection;
        const sf::Vector2f down = -up;
        const sf::Vector2f right = {-up.y, up.x};

        PlatformerControllerState2D next = m_state;
        next.coyoteTimeRemaining = groundedAtStart
                                       ? m_config.coyoteTime
                                       : std::max(0.f, next.coyoteTimeRemaining - fixedDeltaTime);
        next.jumpBufferTimeRemaining =
            input.jumpPressed ? m_config.jumpBufferTime
                              : std::max(0.f, next.jumpBufferTimeRemaining - fixedDeltaTime);
        next.dropThroughTimeRemaining =
            std::max(0.f, next.dropThroughTimeRemaining - fixedDeltaTime);

        if (input.dropDown && groundedAtStart && motorState.onOneWayPlatform)
        {
            next.dropThroughTimeRemaining = m_config.dropThroughTime;
            next.coyoteTimeRemaining = 0.f;
            next.jumpBufferTimeRemaining = 0.f;
            result.events.droppedThrough = true;
        }

        const float horizontalInput = sanitizedInput(input.horizontal, m_config.inputDeadzone);
        float horizontalSpeed = dot(next.velocity, right);
        float verticalSpeed = dot(next.velocity, down);
        const float desiredHorizontalSpeed = horizontalInput * m_config.maximumRunSpeed;
        const bool accelerating = horizontalInput != 0.f;
        const float horizontalRate =
            groundedAtStart
                ? (accelerating ? m_config.groundAcceleration : m_config.groundDeceleration)
                : (accelerating ? m_config.airAcceleration : m_config.airDeceleration);
        horizontalSpeed =
            moveTowards(horizontalSpeed, desiredHorizontalSpeed, horizontalRate * fixedDeltaTime);

        const bool jumpWindowOpen = groundedAtStart || next.coyoteTimeRemaining > 0.f;
        const bool jumpRequested = input.jumpPressed || next.jumpBufferTimeRemaining > 0.f;
        const bool canJump =
            jumpWindowOpen && jumpRequested && next.dropThroughTimeRemaining <= 0.f;
        if (canJump)
        {
            verticalSpeed = -m_config.jumpSpeed;
            next.coyoteTimeRemaining = 0.f;
            next.jumpBufferTimeRemaining = 0.f;
            result.events.jumped = true;
        }
        else
        {
            if (groundedAtStart && verticalSpeed > 0.f) verticalSpeed = 0.f;
            verticalSpeed = std::min(verticalSpeed + m_config.gravity * fixedDeltaTime,
                                     m_config.maximumFallSpeed);
        }

        if (input.jumpReleased && verticalSpeed < 0.f) verticalSpeed *= m_config.jumpCutMultiplier;

        const bool ignoreOneWayPlatforms = next.dropThroughTimeRemaining > 0.f;
        const sf::Vector2f requestedVelocity = right * horizontalSpeed + down * verticalSpeed;
        CharacterMoveResult2D motorResult =
            motor->move(queries, requestedVelocity * fixedDeltaTime, ignoreOneWayPlatforms);
        if (!motorResult.succeeded) return result;

        CharacterStepResult2D stepResult;
        if (m_config.stepHeight > 0.f && groundedAtStart && motorResult.state.touchingWall &&
            std::abs(horizontalSpeed) > motor->config().minimumMoveDistance)
        {
            const float appliedHorizontal = dot(motorResult.movementDisplacement, right);
            const float requestedHorizontal = horizontalSpeed * fixedDeltaTime;
            const float remainingHorizontal = requestedHorizontal - appliedHorizontal;
            if (std::abs(remainingHorizontal) > motor->config().minimumMoveDistance)
            {
                stepResult =
                    motor->tryStep(queries, right * remainingHorizontal, m_config.stepHeight,
                                   m_config.stepDownDistance, ignoreOneWayPlatforms);
                result.events.steppedUp = stepResult.succeeded;
            }
        }

        const CharacterMotorState2D& finalMotorState = motor->state();
        if (finalMotorState.grounded && verticalSpeed > 0.f) verticalSpeed = 0.f;
        if (finalMotorState.touchingCeiling && verticalSpeed < 0.f) verticalSpeed = 0.f;
        if (finalMotorState.touchingWall)
        {
            for (const CharacterMotorContact2D& contact : motorResult.contacts)
            {
                if (contact.kind != CharacterContactKind2D::Wall) continue;
                if (dot(right * horizontalSpeed, contact.normal) < 0.f)
                {
                    horizontalSpeed = 0.f;
                    break;
                }
            }
        }

        next.velocity = right * horizontalSpeed + down * verticalSpeed;
        next.grounded = finalMotorState.grounded;
        next.touchingWall = finalMotorState.touchingWall;
        next.touchingCeiling = finalMotorState.touchingCeiling;
        next.onOneWayPlatform = finalMotorState.onOneWayPlatform;
        if (horizontalInput != 0.f) next.facingRight = horizontalInput > 0.f;
        next.rising = verticalSpeed < 0.f;
        next.falling = verticalSpeed > 0.f && !next.grounded;

        result.events.landed = !wasGrounded && next.grounded;
        result.events.leftGround = wasGrounded && !next.grounded;
        result.events.hitWall = !wasTouchingWall && next.touchingWall;
        result.events.hitCeiling = !wasTouchingCeiling && next.touchingCeiling;

        m_state = next;
        result.succeeded = true;
        result.horizontalInput = horizontalInput;
        result.motorResult = std::move(motorResult);
        result.stepResult = std::move(stepResult);
        result.state = m_state;
        return result;
    }

    const PlatformerControllerState2D& PlatformerController2D::state() const
    {
        return m_state;
    }

    bool PlatformerController2D::setVelocity(sf::Vector2f velocity)
    {
        if (!finite(velocity)) return false;
        m_state.velocity = velocity;
        return true;
    }

    void PlatformerController2D::reset()
    {
        m_state = {};
    }
}

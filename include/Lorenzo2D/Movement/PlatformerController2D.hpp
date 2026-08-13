#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>

namespace l2d
{
    struct PlatformerControllerConfig2D
    {
        float maximumRunSpeed = 240.f;
        float groundAcceleration = 1800.f;
        float groundDeceleration = 2200.f;
        float airAcceleration = 900.f;
        float airDeceleration = 300.f;
        float gravity = 1800.f;
        float maximumFallSpeed = 900.f;
        float jumpSpeed = 620.f;
        float jumpCutMultiplier = 0.45f;
        float coyoteTime = 0.1f;
        float jumpBufferTime = 0.12f;
        float dropThroughTime = 0.18f;
        float stepHeight = 8.f;
        float stepDownDistance = 2.f;
        float inputDeadzone = 0.1f;
        float maximumDeltaTime = 0.25f;
    };

    struct PlatformerInput2D
    {
        float horizontal = 0.f;
        bool jumpPressed = false;
        bool jumpReleased = false;
        bool dropDown = false;
    };

    struct PlatformerControllerState2D
    {
        sf::Vector2f velocity = {0.f, 0.f};
        bool grounded = false;
        bool touchingWall = false;
        bool touchingCeiling = false;
        bool onOneWayPlatform = false;
        bool facingRight = true;
        bool rising = false;
        bool falling = false;
        float coyoteTimeRemaining = 0.f;
        float jumpBufferTimeRemaining = 0.f;
        float dropThroughTimeRemaining = 0.f;
    };

    struct PlatformerEvents2D
    {
        bool jumped = false;
        bool landed = false;
        bool leftGround = false;
        bool hitWall = false;
        bool hitCeiling = false;
        bool steppedUp = false;
        bool droppedThrough = false;
    };

    struct PlatformerMoveResult2D
    {
        bool succeeded = false;
        float horizontalInput = 0.f;
        CharacterMoveResult2D motorResult;
        CharacterStepResult2D stepResult;
        PlatformerControllerState2D state;
        PlatformerEvents2D events;
    };

    // Defaults for a gravity-driven character. Reserve a collider category
    // bit for one-way platforms and pass it here and to their colliders.
    CharacterMotorConfig2D platformerCharacterMotorConfig2D(
        std::uint32_t oneWayPlatformCategoryMask = 0u);

    // Device-independent side-view movement. Pass edge-triggered jump fields
    // once per fixed tick; held horizontal input remains in [-1, 1].
    class PlatformerController2D final : public Component
    {
      public:
        PlatformerController2D();
        explicit PlatformerController2D(PlatformerControllerConfig2D config);

        static bool isValidConfig(const PlatformerControllerConfig2D& config);
        bool setConfig(PlatformerControllerConfig2D config);
        const PlatformerControllerConfig2D& config() const;

        PlatformerMoveResult2D move(const PhysicsQueryContext2D& queries,
                                    const PlatformerInput2D& input, float fixedDeltaTime);

        const PlatformerControllerState2D& state() const;
        bool setVelocity(sf::Vector2f velocity);
        void reset();

      private:
        PlatformerControllerConfig2D m_config;
        PlatformerControllerState2D m_state;
    };
}

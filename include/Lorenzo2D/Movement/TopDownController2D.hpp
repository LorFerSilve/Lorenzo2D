#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    struct TopDownControllerConfig2D
    {
        float maximumSpeed = 180.f;
        float acceleration = 900.f;
        float deceleration = 1100.f;
        float inputDeadzone = 0.1f;
        float maximumDeltaTime = 0.25f;
    };

    struct TopDownControllerState2D
    {
        sf::Vector2f velocity = {0.f, 0.f};
        sf::Vector2f facingDirection = {0.f, 1.f};
        bool moving = false;
    };

    struct TopDownMoveResult2D
    {
        bool succeeded = false;
        sf::Vector2f inputDirection = {0.f, 0.f};
        sf::Vector2f desiredVelocity = {0.f, 0.f};
        CharacterMoveResult2D motorResult;
        TopDownControllerState2D state;
    };

    // Returns motor defaults suited to a horizontal top-down plane: no
    // platform inheritance, ground probing, or vertical ground snap.
    CharacterMotorConfig2D topDownCharacterMotorConfig2D();

    // Device-independent free top-down movement. Sample an InputMap, AI
    // command, or network command in game code and pass the resulting vector
    // once per fixed tick.
    class TopDownController2D final : public Component
    {
      public:
        TopDownController2D();
        explicit TopDownController2D(TopDownControllerConfig2D config);

        static bool isValidConfig(const TopDownControllerConfig2D& config);
        bool setConfig(TopDownControllerConfig2D config);
        const TopDownControllerConfig2D& config() const;

        TopDownMoveResult2D move(const PhysicsQueryContext2D& queries, sf::Vector2f input,
                                 float fixedDeltaTime);

        const TopDownControllerState2D& state() const;
        void stop();

      private:
        TopDownControllerConfig2D m_config;
        TopDownControllerState2D m_state;
    };
}

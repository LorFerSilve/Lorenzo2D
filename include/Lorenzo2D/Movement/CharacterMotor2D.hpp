#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

namespace l2d
{
    enum class CharacterContactKind2D
    {
        Ground,
        Wall,
        Ceiling
    };

    struct CharacterMotorConfig2D
    {
        float skinWidth = 0.01f;
        float groundProbeDistance = 0.1f;
        float maximumSlopeAngleDegrees = 50.f;
        float minimumMoveDistance = 0.00001f;
        float maximumMoveDistance = 100000.f;
        float maximumPlatformDisplacement = 10000.f;
        std::size_t maximumSlideIterations = 4u;
        std::size_t maximumRecoveryIterations = 4u;
        sf::Vector2f upDirection = {0.f, -1.f};
        PhysicsQueryFilter2D queryFilter;
        bool snapToGround = true;
        bool inheritPlatformTranslation = true;
    };

    struct CharacterMotorContact2D
    {
        GameObjectHandle object;
        ColliderId colliderId = InvalidColliderId;
        sf::Vector2f point = {0.f, 0.f};
        sf::Vector2f normal = {0.f, 0.f};
        float distance = 0.f;
        float penetration = 0.f;
        CharacterContactKind2D kind = CharacterContactKind2D::Wall;
        bool recoveredOverlap = false;
        bool groundProbe = false;
    };

    struct CharacterMotorState2D
    {
        bool grounded = false;
        bool touchingWall = false;
        bool touchingCeiling = false;
        sf::Vector2f groundNormal = {0.f, 0.f};
        GameObjectHandle support;
        ColliderId supportColliderId = InvalidColliderId;
    };

    struct CharacterMoveResult2D
    {
        bool succeeded = false;
        sf::Vector2f requestedDisplacement = {0.f, 0.f};
        sf::Vector2f inheritedDisplacement = {0.f, 0.f};
        sf::Vector2f recoveryDisplacement = {0.f, 0.f};
        sf::Vector2f movementDisplacement = {0.f, 0.f};
        sf::Vector2f snapDisplacement = {0.f, 0.f};
        sf::Vector2f remainingDisplacement = {0.f, 0.f};
        CharacterMotorState2D state;
        std::vector<CharacterMotorContact2D> contacts;

        sf::Vector2f totalDisplacement() const;
    };

    // A fixed-step, query-driven kinematic motor shared by gameplay controllers.
    // Call move() with a PhysicsQueryContext2D built after static/kinematic
    // obstacles have reached their positions for the current tick.
    class CharacterMotor2D final : public Component
    {
      public:
        CharacterMotor2D();
        explicit CharacterMotor2D(CharacterMotorConfig2D config);

        static bool isValidConfig(const CharacterMotorConfig2D& config);
        bool setConfig(CharacterMotorConfig2D config);
        const CharacterMotorConfig2D& config() const;

        // InvalidColliderId selects the lowest-ID supported active collider on
        // the owner. Box, circle, and capsule colliders are supported. Changing
        // this selection clears transient contacts and platform support.
        void setColliderId(ColliderId colliderId);
        ColliderId colliderId() const;

        CharacterMoveResult2D move(const PhysicsQueryContext2D& queries, sf::Vector2f displacement);

        // Evaluates the same collision-aware move while restoring the owner's
        // transform and all transient motor state before returning. This is
        // useful for transactional movement such as an all-or-nothing grid step.
        CharacterMoveResult2D testMove(const PhysicsQueryContext2D& queries,
                                       sf::Vector2f displacement);

        const CharacterMotorState2D& state() const;
        void clearState();

      private:
        CharacterMotorConfig2D m_config;
        CharacterMotorState2D m_state;
        ColliderId m_colliderId = InvalidColliderId;
        sf::Vector2f m_supportPosition = {0.f, 0.f};
        bool m_hasSupportPosition = false;
    };
}

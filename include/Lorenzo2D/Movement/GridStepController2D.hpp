#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>

namespace l2d
{
    enum class GridDirection2D : std::uint8_t
    {
        None,
        Left,
        Right,
        Up,
        Down
    };

    enum class GridAxisPriority2D : std::uint8_t
    {
        Horizontal,
        Vertical
    };

    struct GridStepControllerConfig2D
    {
        sf::Vector2f cellSize = {32.f, 32.f};
        sf::Vector2f gridOrigin = {0.f, 0.f};
        float stepDuration = 0.16f;
        float inputThreshold = 0.5f;
        float alignmentTolerance = 0.01f;
        float collisionTolerance = 0.001f;
        float maximumDeltaTime = 0.25f;
        GridAxisPriority2D axisPriority = GridAxisPriority2D::Vertical;
        bool bufferTurns = true;
    };

    struct GridStepControllerState2D
    {
        bool synchronized = false;
        bool stepping = false;
        sf::Vector2i cell = {0, 0};
        GridDirection2D direction = GridDirection2D::None;
        GridDirection2D facingDirection = GridDirection2D::Down;
        GridDirection2D bufferedDirection = GridDirection2D::None;
        float progress = 0.f;
        sf::Vector2f stepStart = {0.f, 0.f};
        sf::Vector2f stepTarget = {0.f, 0.f};
    };

    struct GridStepResult2D
    {
        bool succeeded = false;
        bool stepStarted = false;
        bool stepCompleted = false;
        bool blocked = false;
        bool rolledBack = false;
        CharacterMoveResult2D motorResult;
        GridStepControllerState2D state;
    };

    // Deterministically resolves analog or digital 2D input to one cardinal
    // direction. Equal axes use the configured stable priority.
    GridDirection2D gridDirectionFromInput(
        sf::Vector2f input, float threshold = 0.5f,
        GridAxisPriority2D priority = GridAxisPriority2D::Vertical);

    // Smooth, four-directional, all-or-nothing cell movement. A full step is
    // collision-tested before presentation movement begins, so blocked cells
    // never leave the character between grid coordinates.
    class GridStepController2D final : public Component
    {
      public:
        GridStepController2D();
        explicit GridStepController2D(GridStepControllerConfig2D config);

        static bool isValidConfig(const GridStepControllerConfig2D& config);
        bool setConfig(GridStepControllerConfig2D config);
        const GridStepControllerConfig2D& config() const;

        bool synchronizeToGrid(bool snapToNearestCell = false);
        bool cancelStep();

        GridStepResult2D move(const PhysicsQueryContext2D& queries, GridDirection2D direction,
                              float fixedDeltaTime);
        GridStepResult2D move(const PhysicsQueryContext2D& queries, sf::Vector2f input,
                              float fixedDeltaTime);

        const GridStepControllerState2D& state() const;

      private:
        void clearState();

        GridStepControllerConfig2D m_config;
        GridStepControllerState2D m_state;
        sf::Vector2i m_stepCell = {0, 0};
    };
}

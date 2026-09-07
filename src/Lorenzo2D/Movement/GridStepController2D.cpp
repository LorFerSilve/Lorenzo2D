#include <Lorenzo2D/Movement/GridStepController2D.hpp>

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

        bool validDirection(GridDirection2D direction)
        {
            return direction == GridDirection2D::None || direction == GridDirection2D::Left ||
                   direction == GridDirection2D::Right || direction == GridDirection2D::Up ||
                   direction == GridDirection2D::Down;
        }

        bool validPriority(GridAxisPriority2D priority)
        {
            return priority == GridAxisPriority2D::Horizontal ||
                   priority == GridAxisPriority2D::Vertical;
        }

        sf::Vector2i cellOffset(GridDirection2D direction)
        {
            switch (direction)
            {
            case GridDirection2D::Left:
                return {-1, 0};
            case GridDirection2D::Right:
                return {1, 0};
            case GridDirection2D::Up:
                return {0, -1};
            case GridDirection2D::Down:
                return {0, 1};
            case GridDirection2D::None:
                break;
            }
            return {};
        }

        sf::Vector2f worldOffset(GridDirection2D direction, sf::Vector2f cellSize)
        {
            const sf::Vector2i cell = cellOffset(direction);
            return {static_cast<float>(cell.x) * cellSize.x,
                    static_cast<float>(cell.y) * cellSize.y};
        }

        bool negligible(sf::Vector2f value, float tolerance)
        {
            return length(value) <= tolerance;
        }
    }

    GridDirection2D gridDirectionFromInput(sf::Vector2f input, float threshold,
                                           GridAxisPriority2D priority)
    {
        if (!finite(input) || !std::isfinite(threshold) || threshold < 0.f || threshold > 1.f ||
            !validPriority(priority))
            return GridDirection2D::None;

        const float horizontal = std::abs(input.x);
        const float vertical = std::abs(input.y);
        if (horizontal == 0.f && vertical == 0.f) return GridDirection2D::None;
        if (horizontal < threshold && vertical < threshold) return GridDirection2D::None;

        const bool chooseHorizontal =
            horizontal > vertical ||
            (horizontal == vertical && priority == GridAxisPriority2D::Horizontal);
        if (chooseHorizontal && horizontal >= threshold)
            return input.x < 0.f ? GridDirection2D::Left : GridDirection2D::Right;
        if (vertical >= threshold)
            return input.y < 0.f ? GridDirection2D::Up : GridDirection2D::Down;
        return input.x < 0.f ? GridDirection2D::Left : GridDirection2D::Right;
    }

    GridStepController2D::GridStepController2D() = default;

    GridStepController2D::GridStepController2D(GridStepControllerConfig2D config)
    {
        (void)setConfig(config);
    }

    bool GridStepController2D::isValidConfig(const GridStepControllerConfig2D& config)
    {
        return finite(config.cellSize) && config.cellSize.x > 0.f && config.cellSize.y > 0.f &&
               finite(config.gridOrigin) && std::isfinite(config.stepDuration) &&
               config.stepDuration > 0.f && std::isfinite(config.inputThreshold) &&
               config.inputThreshold >= 0.f && config.inputThreshold <= 1.f &&
               std::isfinite(config.alignmentTolerance) && config.alignmentTolerance >= 0.f &&
               std::isfinite(config.collisionTolerance) && config.collisionTolerance >= 0.f &&
               std::isfinite(config.maximumDeltaTime) && config.maximumDeltaTime > 0.f &&
               validPriority(config.axisPriority);
    }

    bool GridStepController2D::setConfig(GridStepControllerConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        if (m_state.stepping && owner() != nullptr)
            owner()->transform.setPosition(m_state.stepStart);
        m_config = config;
        clearState();
        return true;
    }

    const GridStepControllerConfig2D& GridStepController2D::config() const
    {
        return m_config;
    }

    bool GridStepController2D::synchronizeToGrid(bool snapToNearestCell)
    {
        GameObject* character = owner();
        if (character == nullptr || !isValidConfig(m_config)) return false;
        if (m_state.stepping)
        {
            if (!snapToNearestCell || !cancelStep()) return false;
        }

        const sf::Vector2f position = character->transform.position();
        const double rawX =
            (static_cast<double>(position.x) - m_config.gridOrigin.x) / m_config.cellSize.x;
        const double rawY =
            (static_cast<double>(position.y) - m_config.gridOrigin.y) / m_config.cellSize.y;
        const double roundedX = std::round(rawX);
        const double roundedY = std::round(rawY);
        if (!std::isfinite(rawX) || !std::isfinite(rawY) ||
            roundedX < static_cast<double>(std::numeric_limits<int>::min()) ||
            roundedX > static_cast<double>(std::numeric_limits<int>::max()) ||
            roundedY < static_cast<double>(std::numeric_limits<int>::min()) ||
            roundedY > static_cast<double>(std::numeric_limits<int>::max()))
            return false;

        const sf::Vector2i cell{static_cast<int>(roundedX), static_cast<int>(roundedY)};
        const sf::Vector2f aligned{
            m_config.gridOrigin.x + static_cast<float>(cell.x) * m_config.cellSize.x,
            m_config.gridOrigin.y + static_cast<float>(cell.y) * m_config.cellSize.y};
        if (!finite(aligned) ||
            (!snapToNearestCell && length(position - aligned) > m_config.alignmentTolerance))
            return false;

        if (snapToNearestCell) character->transform.setPosition(aligned);
        m_state.synchronized = true;
        m_state.stepping = false;
        m_state.cell = cell;
        m_state.direction = GridDirection2D::None;
        m_state.bufferedDirection = GridDirection2D::None;
        m_state.progress = 0.f;
        m_state.stepStart = aligned;
        m_state.stepTarget = aligned;
        m_stepCell = cell;
        return true;
    }

    bool GridStepController2D::cancelStep()
    {
        if (!m_state.stepping) return false;
        GameObject* character = owner();
        if (character == nullptr) return false;
        character->transform.setPosition(m_state.stepStart);
        if (CharacterMotor2D* motor = character->getComponent<CharacterMotor2D>())
            motor->clearState();
        m_state.stepping = false;
        m_state.direction = GridDirection2D::None;
        m_state.bufferedDirection = GridDirection2D::None;
        m_state.progress = 0.f;
        m_state.cell = m_stepCell;
        m_state.stepTarget = m_state.stepStart;
        return true;
    }

    GridStepResult2D GridStepController2D::move(const PhysicsQueryContext2D& queries,
                                                GridDirection2D direction, float fixedDeltaTime)
    {
        GridStepResult2D result;
        result.state = m_state;
        if (!isValidConfig(m_config) || !validDirection(direction) ||
            !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.f ||
            fixedDeltaTime > m_config.maximumDeltaTime)
            return result;

        GameObject* character = owner();
        CharacterMotor2D* motor =
            character == nullptr ? nullptr : character->getComponent<CharacterMotor2D>();
        if (motor == nullptr) return result;
        if (!m_state.synchronized && !synchronizeToGrid(false)) return result;
        if (!m_state.stepping)
        {
            const sf::Vector2f expectedPosition = {
                m_config.gridOrigin.x + static_cast<float>(m_state.cell.x) * m_config.cellSize.x,
                m_config.gridOrigin.y + static_cast<float>(m_state.cell.y) * m_config.cellSize.y};
            if (!finite(expectedPosition) ||
                !negligible(character->transform.position() - expectedPosition,
                            m_config.alignmentTolerance))
            {
                m_state.synchronized = false;
                result.state = m_state;
                return result;
            }
        }

        if (m_state.stepping && m_config.bufferTurns && direction != GridDirection2D::None &&
            direction != m_state.direction)
            m_state.bufferedDirection = direction;

        if (!m_state.stepping)
        {
            const GridDirection2D requested = m_state.bufferedDirection != GridDirection2D::None
                                                  ? m_state.bufferedDirection
                                                  : direction;
            m_state.bufferedDirection = GridDirection2D::None;
            if (requested == GridDirection2D::None)
            {
                result.succeeded = true;
                result.state = m_state;
                return result;
            }

            const sf::Vector2i offset = cellOffset(requested);
            const std::int64_t targetX = static_cast<std::int64_t>(m_state.cell.x) + offset.x;
            const std::int64_t targetY = static_cast<std::int64_t>(m_state.cell.y) + offset.y;
            if (targetX < std::numeric_limits<int>::min() ||
                targetX > std::numeric_limits<int>::max() ||
                targetY < std::numeric_limits<int>::min() ||
                targetY > std::numeric_limits<int>::max())
                return result;

            const sf::Vector2f fullStep = worldOffset(requested, m_config.cellSize);
            CharacterMoveResult2D probe = motor->testMove(queries, fullStep);
            const bool clear =
                probe.succeeded &&
                negligible(probe.movementDisplacement - fullStep, m_config.collisionTolerance) &&
                negligible(probe.inheritedDisplacement, m_config.collisionTolerance) &&
                negligible(probe.recoveryDisplacement, m_config.collisionTolerance) &&
                negligible(probe.snapDisplacement, m_config.collisionTolerance);
            if (!clear)
            {
                result.succeeded = probe.succeeded;
                result.blocked = probe.succeeded;
                result.motorResult = std::move(probe);
                result.state = m_state;
                return result;
            }

            m_stepCell = m_state.cell;
            m_state.stepping = true;
            m_state.direction = requested;
            m_state.facingDirection = requested;
            m_state.progress = 0.f;
            m_state.stepStart = character->transform.position();
            m_state.stepTarget = m_state.stepStart + fullStep;
            result.stepStarted = true;
        }

        const float nextProgress =
            std::min(1.f, m_state.progress + fixedDeltaTime / m_config.stepDuration);
        const sf::Vector2f desiredPosition =
            m_state.stepStart + (m_state.stepTarget - m_state.stepStart) * nextProgress;
        const sf::Vector2f requestedDisplacement =
            desiredPosition - character->transform.position();
        CharacterMoveResult2D movement = motor->move(queries, requestedDisplacement);
        const bool applied =
            movement.succeeded &&
            negligible(movement.movementDisplacement - requestedDisplacement,
                       m_config.collisionTolerance) &&
            negligible(movement.recoveryDisplacement, m_config.collisionTolerance) &&
            negligible(movement.inheritedDisplacement, m_config.collisionTolerance) &&
            negligible(movement.snapDisplacement, m_config.collisionTolerance);
        if (!applied)
        {
            character->transform.setPosition(m_state.stepStart);
            motor->clearState();
            m_state.stepping = false;
            m_state.direction = GridDirection2D::None;
            m_state.progress = 0.f;
            m_state.cell = m_stepCell;
            m_state.stepTarget = m_state.stepStart;
            result.succeeded = movement.succeeded;
            result.blocked = movement.succeeded;
            result.rolledBack = true;
            result.motorResult = std::move(movement);
            result.state = m_state;
            return result;
        }

        m_state.progress = nextProgress;
        result.succeeded = true;
        result.motorResult = std::move(movement);
        if (nextProgress >= 1.f)
        {
            character->transform.setPosition(m_state.stepTarget);
            const sf::Vector2i offset = cellOffset(m_state.direction);
            m_state.cell += offset;
            m_state.stepping = false;
            m_state.direction = GridDirection2D::None;
            m_state.progress = 0.f;
            m_state.stepStart = m_state.stepTarget;
            m_stepCell = m_state.cell;
            result.stepCompleted = true;
        }
        result.state = m_state;
        return result;
    }

    GridStepResult2D GridStepController2D::move(const PhysicsQueryContext2D& queries,
                                                sf::Vector2f input, float fixedDeltaTime)
    {
        return move(queries,
                    gridDirectionFromInput(input, m_config.inputThreshold, m_config.axisPriority),
                    fixedDeltaTime);
    }

    const GridStepControllerState2D& GridStepController2D::state() const
    {
        return m_state;
    }

    void GridStepController2D::clearState()
    {
        const GridDirection2D facing = m_state.facingDirection;
        m_state = {};
        m_state.facingDirection = facing;
        m_stepCell = {};
        if (GameObject* character = owner())
            if (CharacterMotor2D* motor = character->getComponent<CharacterMotor2D>())
                motor->clearState();
    }
}

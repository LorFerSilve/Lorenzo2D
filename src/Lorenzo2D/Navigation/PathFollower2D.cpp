#include <Lorenzo2D/Navigation/PathFollower2D.hpp>

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

        sf::Vector2f controllerInput(sf::Vector2f velocity, const TopDownControllerConfig2D& config)
        {
            const float speed = length(velocity);
            if (speed <= std::numeric_limits<float>::epsilon()) return {};
            const float speedScale = std::clamp(speed / config.maximumSpeed, 0.f, 1.f);
            const float inputMagnitude =
                config.inputDeadzone + speedScale * (1.f - config.inputDeadzone);
            return velocity / speed * inputMagnitude;
        }
    }

    PathFollower2D::PathFollower2D() = default;

    PathFollower2D::PathFollower2D(PathFollowerConfig2D config)
    {
        (void)setConfig(std::move(config));
    }

    bool PathFollower2D::isValidConfig(const PathFollowerConfig2D& config)
    {
        return std::isfinite(config.waypointTolerance) && config.waypointTolerance > 0.f &&
               std::isfinite(config.goalTolerance) && config.goalTolerance > 0.f &&
               std::isfinite(config.slowdownDistance) && config.slowdownDistance > 0.f &&
               config.slowdownDistance >= config.goalTolerance &&
               std::isfinite(config.agentRadius) && config.agentRadius > 0.f &&
               finite(config.positionOffset) && std::isfinite(config.stuckTimeout) &&
               config.stuckTimeout > 0.f && std::isfinite(config.minimumProgressDistance) &&
               config.minimumProgressDistance >= 0.f && std::isfinite(config.maximumDeltaTime) &&
               config.maximumDeltaTime > 0.f && LocalAvoidance2D::isValidConfig(config.avoidance);
    }

    bool PathFollower2D::setConfig(PathFollowerConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        m_config = std::move(config);
        clearPath();
        return true;
    }

    const PathFollowerConfig2D& PathFollower2D::config() const
    {
        return m_config;
    }

    bool PathFollower2D::setPath(const NavigationPath2D& pathValue)
    {
        if (!pathValue.succeeded() || pathValue.points.empty() ||
            !std::all_of(pathValue.points.begin(), pathValue.points.end(), finite))
            return false;

        m_path = pathValue.points;
        m_state = {};
        m_state.status = PathFollowerStatus2D::Following;
        m_state.destination = m_path.back();
        m_state.pathRevision = pathValue.gridRevision;
        m_hasPreviousWaypointDistance = false;
        return true;
    }

    NavigationPath2D PathFollower2D::setDestination(const NavigationGrid2D& grid,
                                                    const AStarPathfinder2D& pathfinder,
                                                    sf::Vector2f destination,
                                                    const NavigationPathOptions2D& options)
    {
        GameObject* object = owner();
        if (object == nullptr)
        {
            NavigationPath2D result;
            result.status = NavigationPathStatus2D::InvalidStart;
            result.gridRevision = grid.revision();
            return result;
        }
        NavigationPath2D result = pathfinder.findWorldPath(
            grid, object->transform.position() + m_config.positionOffset, destination, options);
        if (result.succeeded()) (void)setPath(result);
        return result;
    }

    void PathFollower2D::clearPath()
    {
        m_path.clear();
        m_state = {};
        m_hasPreviousWaypointDistance = false;
        GameObject* object = owner();
        TopDownController2D* controller =
            object == nullptr ? nullptr : object->getComponent<TopDownController2D>();
        if (controller != nullptr) controller->stop();
    }

    PathFollowResult2D PathFollower2D::follow(const PhysicsQueryContext2D& queries,
                                              float fixedDeltaTime)
    {
        return follow(queries, {}, fixedDeltaTime);
    }

    PathFollowResult2D PathFollower2D::follow(
        const PhysicsQueryContext2D& queries,
        const std::vector<NavigationAgentSnapshot2D>& neighbors, float fixedDeltaTime)
    {
        PathFollowResult2D result;
        result.state = m_state;
        GameObject* object = owner();
        TopDownController2D* controller =
            object == nullptr ? nullptr : object->getComponent<TopDownController2D>();
        if (!isValidConfig(m_config) || object == nullptr || controller == nullptr ||
            !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.f ||
            fixedDeltaTime > m_config.maximumDeltaTime || m_path.empty() ||
            m_state.status != PathFollowerStatus2D::Following)
            return result;

        sf::Vector2f position = object->transform.position() + m_config.positionOffset;
        while (m_state.nextWaypoint < m_path.size())
        {
            const bool goal = m_state.nextWaypoint + 1u == m_path.size();
            const float tolerance = goal ? m_config.goalTolerance : m_config.waypointTolerance;
            if (length(m_path[m_state.nextWaypoint] - position) > tolerance) break;
            ++m_state.nextWaypoint;
            result.waypointAdvanced = true;
            m_hasPreviousWaypointDistance = false;
        }

        if (m_state.nextWaypoint >= m_path.size())
        {
            controller->stop();
            m_state.status = PathFollowerStatus2D::Arrived;
            result.succeeded = true;
            result.state = m_state;
            return result;
        }

        const sf::Vector2f waypointDelta = m_path[m_state.nextWaypoint] - position;
        const float waypointDistance = length(waypointDelta);
        float remainingDistance = waypointDistance;
        for (std::size_t index = m_state.nextWaypoint + 1u; index < m_path.size(); ++index)
            remainingDistance += length(m_path[index] - m_path[index - 1u]);
        const float speedScale =
            std::clamp(remainingDistance / m_config.slowdownDistance, 0.1f, 1.f);
        result.preferredVelocity =
            waypointDelta / waypointDistance * (controller->config().maximumSpeed * speedScale);

        if (m_config.localAvoidance)
        {
            NavigationAgentSnapshot2D agent;
            agent.id = object->id();
            agent.position = position;
            agent.velocity = controller->state().velocity;
            agent.radius = m_config.agentRadius;
            agent.maximumSpeed = controller->config().maximumSpeed;
            result.avoidanceResult = LocalAvoidance2D::calculateVelocity(
                agent, neighbors, result.preferredVelocity, m_config.avoidance);
            if (!result.avoidanceResult.succeeded) return result;
        }
        else
        {
            result.avoidanceResult.succeeded = true;
            result.avoidanceResult.velocity = result.preferredVelocity;
        }

        const sf::Vector2f input =
            controllerInput(result.avoidanceResult.velocity, controller->config());
        result.moveResult = controller->move(queries, input, fixedDeltaTime);
        if (!result.moveResult.succeeded) return result;

        position = object->transform.position() + m_config.positionOffset;
        const float distanceAfterMove = length(m_path[m_state.nextWaypoint] - position);
        if (!m_hasPreviousWaypointDistance ||
            m_previousWaypointDistance - distanceAfterMove >= m_config.minimumProgressDistance)
        {
            m_state.elapsedWithoutProgress = 0.f;
        }
        else
        {
            m_state.elapsedWithoutProgress += fixedDeltaTime;
        }
        m_previousWaypointDistance = distanceAfterMove;
        m_hasPreviousWaypointDistance = true;

        if (m_state.elapsedWithoutProgress >= m_config.stuckTimeout)
        {
            controller->stop();
            m_state.status = PathFollowerStatus2D::Stuck;
            result.requestedRepath = true;
        }

        result.succeeded = true;
        result.state = m_state;
        return result;
    }

    bool PathFollower2D::isPathStale(const NavigationGrid2D& grid) const
    {
        return !m_path.empty() && m_state.pathRevision != grid.revision();
    }

    const std::vector<sf::Vector2f>& PathFollower2D::path() const
    {
        return m_path;
    }

    const PathFollowerState2D& PathFollower2D::state() const
    {
        return m_state;
    }
}

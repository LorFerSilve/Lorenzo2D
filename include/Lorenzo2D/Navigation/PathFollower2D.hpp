#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/LocalAvoidance2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace l2d
{
    struct PathFollowerConfig2D
    {
        float waypointTolerance = 4.f;
        float goalTolerance = 2.f;
        float slowdownDistance = 48.f;
        float agentRadius = 8.f;
        sf::Vector2f positionOffset = {0.f, 0.f};
        float stuckTimeout = 0.75f;
        float minimumProgressDistance = 0.25f;
        float maximumDeltaTime = 0.25f;
        bool localAvoidance = true;
        LocalAvoidanceConfig2D avoidance;
    };

    enum class PathFollowerStatus2D
    {
        Idle,
        Following,
        Arrived,
        Stuck,
        Invalid
    };

    struct PathFollowerState2D
    {
        PathFollowerStatus2D status = PathFollowerStatus2D::Idle;
        std::size_t nextWaypoint = 0u;
        sf::Vector2f destination = {0.f, 0.f};
        float elapsedWithoutProgress = 0.f;
        std::uint64_t pathRevision = 0u;
    };

    struct PathFollowResult2D
    {
        bool succeeded = false;
        bool waypointAdvanced = false;
        bool requestedRepath = false;
        sf::Vector2f preferredVelocity = {0.f, 0.f};
        LocalAvoidanceResult2D avoidanceResult;
        TopDownMoveResult2D moveResult;
        PathFollowerState2D state;
    };

    // Point-and-click path following layered on the shared top-down controller
    // and character motor. Paths are runtime commands; only tuning is prefab
    // data.
    class PathFollower2D final : public Component
    {
      public:
        PathFollower2D();
        explicit PathFollower2D(PathFollowerConfig2D config);

        static bool isValidConfig(const PathFollowerConfig2D& config);
        bool setConfig(PathFollowerConfig2D config);
        const PathFollowerConfig2D& config() const;

        bool setPath(const NavigationPath2D& path);
        NavigationPath2D setDestination(const NavigationGrid2D& grid,
                                        const AStarPathfinder2D& pathfinder,
                                        sf::Vector2f destination,
                                        const NavigationPathOptions2D& options = {});
        void clearPath();

        PathFollowResult2D follow(const PhysicsQueryContext2D& queries, float fixedDeltaTime);
        PathFollowResult2D follow(const PhysicsQueryContext2D& queries,
                                  const std::vector<NavigationAgentSnapshot2D>& neighbors,
                                  float fixedDeltaTime);

        bool isPathStale(const NavigationGrid2D& grid) const;
        const std::vector<sf::Vector2f>& path() const;
        const PathFollowerState2D& state() const;

      private:
        PathFollowerConfig2D m_config;
        PathFollowerState2D m_state;
        std::vector<sf::Vector2f> m_path;
        float m_previousWaypointDistance = 0.f;
        bool m_hasPreviousWaypointDistance = false;
    };
}

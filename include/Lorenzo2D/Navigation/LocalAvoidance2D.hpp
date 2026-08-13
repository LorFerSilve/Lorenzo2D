#pragma once

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

namespace l2d
{
    struct NavigationAgentSnapshot2D
    {
        GameObjectId id = InvalidGameObjectId;
        sf::Vector2f position = {0.f, 0.f};
        sf::Vector2f velocity = {0.f, 0.f};
        float radius = 8.f;
        float maximumSpeed = 180.f;
    };

    struct LocalAvoidanceConfig2D
    {
        float neighborDistance = 96.f;
        float timeHorizon = 0.75f;
        float personalSpace = 2.f;
        float avoidanceStrength = 1.f;
        std::size_t maximumNeighbors = 8u;
    };

    struct LocalAvoidanceResult2D
    {
        bool succeeded = false;
        sf::Vector2f velocity = {0.f, 0.f};
        std::size_t consideredNeighbors = 0u;
    };

    class LocalAvoidance2D
    {
      public:
        static constexpr std::size_t MaximumNeighborCount = 1024u;

        static bool isValidConfig(const LocalAvoidanceConfig2D& config);
        static bool isValidAgent(const NavigationAgentSnapshot2D& agent);

        static LocalAvoidanceResult2D calculateVelocity(
            const NavigationAgentSnapshot2D& agent,
            const std::vector<NavigationAgentSnapshot2D>& neighbors, sf::Vector2f preferredVelocity,
            const LocalAvoidanceConfig2D& config = {});
    };
}

#include <Lorenzo2D/Navigation/LocalAvoidance2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float squaredLength(sf::Vector2f value)
        {
            return value.x * value.x + value.y * value.y;
        }

        float length(sf::Vector2f value)
        {
            return static_cast<float>(
                std::hypot(static_cast<double>(value.x), static_cast<double>(value.y)));
        }

        float dot(sf::Vector2f left, sf::Vector2f right)
        {
            return left.x * right.x + left.y * right.y;
        }

        sf::Vector2f clamped(sf::Vector2f value, float maximum)
        {
            const float magnitude = length(value);
            if (magnitude <= maximum || magnitude <= std::numeric_limits<float>::epsilon())
                return value;
            return value * (maximum / magnitude);
        }

        struct Candidate
        {
            NavigationAgentSnapshot2D agent;
            float distanceSquared = 0.f;
        };

        bool betterCandidate(const Candidate& left, const Candidate& right)
        {
            if (left.distanceSquared != right.distanceSquared)
                return left.distanceSquared < right.distanceSquared;
            if (left.agent.id != right.agent.id) return left.agent.id < right.agent.id;
            if (left.agent.position.x != right.agent.position.x)
                return left.agent.position.x < right.agent.position.x;
            if (left.agent.position.y != right.agent.position.y)
                return left.agent.position.y < right.agent.position.y;
            if (left.agent.velocity.x != right.agent.velocity.x)
                return left.agent.velocity.x < right.agent.velocity.x;
            if (left.agent.velocity.y != right.agent.velocity.y)
                return left.agent.velocity.y < right.agent.velocity.y;
            if (left.agent.radius != right.agent.radius)
                return left.agent.radius < right.agent.radius;
            return left.agent.maximumSpeed < right.agent.maximumSpeed;
        }

        struct BetterCandidate
        {
            bool operator()(const Candidate& left, const Candidate& right) const
            {
                return betterCandidate(left, right);
            }
        };
    }

    bool LocalAvoidance2D::isValidConfig(const LocalAvoidanceConfig2D& config)
    {
        return std::isfinite(config.neighborDistance) && config.neighborDistance > 0.f &&
               std::isfinite(config.timeHorizon) && config.timeHorizon > 0.f &&
               std::isfinite(config.personalSpace) && config.personalSpace >= 0.f &&
               std::isfinite(config.avoidanceStrength) && config.avoidanceStrength >= 0.f &&
               config.maximumNeighbors > 0u && config.maximumNeighbors <= MaximumNeighborCount;
    }

    bool LocalAvoidance2D::isValidAgent(const NavigationAgentSnapshot2D& agent)
    {
        return agent.id != InvalidGameObjectId && finite(agent.position) &&
               finite(agent.velocity) && std::isfinite(agent.radius) && agent.radius > 0.f &&
               std::isfinite(agent.maximumSpeed) && agent.maximumSpeed > 0.f;
    }

    LocalAvoidanceResult2D LocalAvoidance2D::calculateVelocity(
        const NavigationAgentSnapshot2D& agent,
        const std::vector<NavigationAgentSnapshot2D>& neighbors, sf::Vector2f preferredVelocity,
        const LocalAvoidanceConfig2D& config)
    {
        LocalAvoidanceResult2D result;
        if (!isValidAgent(agent) || !isValidConfig(config) || !finite(preferredVelocity))
            return result;

        const float maximumDistanceSquared = config.neighborDistance * config.neighborDistance;
        std::priority_queue<Candidate, std::vector<Candidate>, BetterCandidate> nearest;
        for (const NavigationAgentSnapshot2D& neighbor : neighbors)
        {
            if (!isValidAgent(neighbor) || neighbor.id == agent.id) continue;
            const float distanceSquared = squaredLength(neighbor.position - agent.position);
            if (distanceSquared <= maximumDistanceSquared)
            {
                const Candidate candidate{neighbor, distanceSquared};
                if (nearest.size() < config.maximumNeighbors)
                    nearest.push(candidate);
                else if (betterCandidate(candidate, nearest.top()))
                {
                    nearest.pop();
                    nearest.push(candidate);
                }
            }
        }
        std::vector<Candidate> candidates;
        candidates.reserve(nearest.size());
        while (!nearest.empty())
        {
            candidates.push_back(nearest.top());
            nearest.pop();
        }
        std::sort(candidates.begin(), candidates.end(), betterCandidate);

        sf::Vector2f correction;
        for (const Candidate& candidate : candidates)
        {
            const NavigationAgentSnapshot2D& neighbor = candidate.agent;
            const sf::Vector2f relativePosition = agent.position - neighbor.position;
            const sf::Vector2f relativeVelocity = preferredVelocity - neighbor.velocity;
            const float velocitySquared = squaredLength(relativeVelocity);
            const float closestTime =
                velocitySquared <= std::numeric_limits<float>::epsilon()
                    ? 0.f
                    : std::clamp(-dot(relativePosition, relativeVelocity) / velocitySquared, 0.f,
                                 config.timeHorizon);
            const sf::Vector2f futureSeparation = relativePosition + relativeVelocity * closestTime;
            const float futureDistance = length(futureSeparation);
            const float requiredDistance = agent.radius + neighbor.radius + config.personalSpace;
            if (futureDistance >= requiredDistance) continue;

            sf::Vector2f direction;
            if (futureDistance > std::numeric_limits<float>::epsilon())
                direction = futureSeparation / futureDistance;
            else
                direction = {0.f, agent.id < neighbor.id ? -1.f : 1.f};
            const float weight = (requiredDistance - futureDistance) / requiredDistance;
            correction += direction * (weight * agent.maximumSpeed);
        }

        result.succeeded = true;
        result.consideredNeighbors = candidates.size();
        result.velocity =
            clamped(preferredVelocity + correction * config.avoidanceStrength, agent.maximumSpeed);
        return result;
    }
}

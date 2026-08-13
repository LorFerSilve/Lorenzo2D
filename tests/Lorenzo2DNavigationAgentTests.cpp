#include "TestSupport.hpp"

#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Navigation/LocalAvoidance2D.hpp>
#include <Lorenzo2D/Navigation/PathFollower2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

namespace
{
    struct Agent
    {
        l2d::GameObject& object;
        l2d::TopDownController2D& controller;
        l2d::PathFollower2D& follower;
    };

    Agent addAgent(l2d::Scene& scene, sf::Vector2f position = {5.f, 5.f})
    {
        l2d::GameObject& object = scene.createGameObject("Navigation agent");
        object.transform.setPosition(position);
        object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        object.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        l2d::TopDownControllerConfig2D controllerConfig;
        controllerConfig.maximumSpeed = 10.f;
        controllerConfig.acceleration = 100.f;
        controllerConfig.deceleration = 100.f;
        controllerConfig.inputDeadzone = 0.2f;
        controllerConfig.maximumDeltaTime = 0.2f;
        auto& controller = object.addComponent<l2d::TopDownController2D>(controllerConfig);
        l2d::PathFollowerConfig2D followerConfig;
        followerConfig.waypointTolerance = 0.5f;
        followerConfig.goalTolerance = 0.5f;
        followerConfig.slowdownDistance = 3.f;
        followerConfig.stuckTimeout = 0.4f;
        followerConfig.minimumProgressDistance = 0.01f;
        followerConfig.maximumDeltaTime = 0.2f;
        followerConfig.localAvoidance = false;
        auto& follower = object.addComponent<l2d::PathFollower2D>(followerConfig);
        return {object, controller, follower};
    }

    void testAvoidanceValidationAndDeterminism()
    {
        l2d::NavigationAgentSnapshot2D agent;
        agent.id = 1u;
        agent.radius = 1.f;
        agent.maximumSpeed = 10.f;
        l2d::NavigationAgentSnapshot2D collision;
        collision.id = 2u;
        collision.position = {5.f, 0.f};
        collision.velocity = {-10.f, 0.f};
        collision.radius = 1.f;
        collision.maximumSpeed = 10.f;
        l2d::NavigationAgentSnapshot2D distant = collision;
        distant.id = 3u;
        distant.position = {200.f, 0.f};

        std::vector<l2d::NavigationAgentSnapshot2D> neighbors{distant, collision};
        const auto first = l2d::LocalAvoidance2D::calculateVelocity(agent, neighbors, {10.f, 0.f});
        std::reverse(neighbors.begin(), neighbors.end());
        const auto second = l2d::LocalAvoidance2D::calculateVelocity(agent, neighbors, {10.f, 0.f});
        L2D_REQUIRE(first.succeeded);
        L2D_REQUIRE_EQUAL(first.consideredNeighbors, 1u);
        L2D_REQUIRE(first.velocity.y < 0.f);
        L2D_REQUIRE_APPROX_2D(first.velocity, second.velocity, 0.0001f);
        L2D_REQUIRE(std::hypot(first.velocity.x, first.velocity.y) <= 10.0001f);

        l2d::LocalAvoidanceConfig2D invalid;
        invalid.timeHorizon = 0.f;
        L2D_REQUIRE(!l2d::LocalAvoidance2D::isValidConfig(invalid));
        agent.id = l2d::InvalidGameObjectId;
        L2D_REQUIRE(!l2d::LocalAvoidance2D::isValidAgent(agent));
    }

    void testSetDestinationFollowAndStaleness()
    {
        l2d::Scene scene;
        Agent agent = addAgent(scene);
        l2d::NavigationGridConfig2D config;
        config.size = {4u, 1u};
        config.cellSize = {10.f, 10.f};
        l2d::NavigationGrid2D grid(config);
        const l2d::AStarPathfinder2D pathfinder;
        const auto path = agent.follower.setDestination(grid, pathfinder, {35.f, 5.f});
        L2D_REQUIRE(path.succeeded());
        L2D_REQUIRE(!agent.follower.isPathStale(grid));
        grid.setTraversalCost({1, 0}, 2.f);
        L2D_REQUIRE(agent.follower.isPathStale(grid));

        bool arrived = false;
        for (int tick = 0; tick < 100; ++tick)
        {
            const auto result = agent.follower.follow(l2d::PhysicsQueryContext2D(scene), 0.1f);
            L2D_REQUIRE(result.succeeded);
            if (result.state.status == l2d::PathFollowerStatus2D::Arrived)
            {
                arrived = true;
                break;
            }
        }
        L2D_REQUIRE(arrived);
        L2D_REQUIRE_APPROX_2D(agent.object.transform.position(), sf::Vector2f(35.f, 5.f), 0.6f);
        L2D_REQUIRE(!agent.controller.state().moving);
    }

    void testNavigationPositionOffset()
    {
        l2d::Scene scene;
        Agent agent = addAgent(scene, {0.f, 0.f});
        l2d::PathFollowerConfig2D followerConfig = agent.follower.config();
        followerConfig.positionOffset = {5.f, 5.f};
        L2D_REQUIRE(agent.follower.setConfig(followerConfig));

        l2d::NavigationGridConfig2D config;
        config.size = {2u, 1u};
        config.cellSize = {10.f, 10.f};
        l2d::NavigationGrid2D grid(config);
        L2D_REQUIRE(
            agent.follower.setDestination(grid, l2d::AStarPathfinder2D{}, {15.f, 5.f}).succeeded());
        for (int tick = 0; tick < 40; ++tick)
        {
            const auto result = agent.follower.follow(l2d::PhysicsQueryContext2D(scene), 0.1f);
            L2D_REQUIRE(result.succeeded);
            if (result.state.status == l2d::PathFollowerStatus2D::Arrived) break;
        }
        L2D_REQUIRE_EQUAL(agent.follower.state().status, l2d::PathFollowerStatus2D::Arrived);
        L2D_REQUIRE_APPROX_2D(agent.object.transform.position(), sf::Vector2f(10.f, 0.f), 0.6f);
    }

    void testStuckAndFailureContracts()
    {
        l2d::Scene scene;
        l2d::GameObject& wall = scene.createGameObject("Wall");
        wall.transform.setPosition({15.f, 5.f});
        wall.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 20.f});
        Agent agent = addAgent(scene);
        l2d::NavigationGridConfig2D config;
        config.size = {4u, 1u};
        config.cellSize = {10.f, 10.f};
        l2d::NavigationGrid2D grid(config);
        const l2d::AStarPathfinder2D pathfinder;
        L2D_REQUIRE(agent.follower.setDestination(grid, pathfinder, {35.f, 5.f}).succeeded());

        bool requestedRepath = false;
        for (int tick = 0; tick < 50; ++tick)
        {
            const auto result = agent.follower.follow(l2d::PhysicsQueryContext2D(scene), 0.1f);
            L2D_REQUIRE(result.succeeded);
            if (result.requestedRepath)
            {
                requestedRepath = true;
                break;
            }
        }
        L2D_REQUIRE(requestedRepath);
        L2D_REQUIRE_EQUAL(agent.follower.state().status, l2d::PathFollowerStatus2D::Stuck);

        l2d::GameObject& incomplete = scene.createGameObject("Incomplete agent");
        auto& follower = incomplete.addComponent<l2d::PathFollower2D>();
        l2d::NavigationPath2D manual;
        manual.status = l2d::NavigationPathStatus2D::Succeeded;
        manual.points = {{0.f, 0.f}, {10.f, 0.f}};
        L2D_REQUIRE(follower.setPath(manual));
        L2D_REQUIRE(!follower.follow(l2d::PhysicsQueryContext2D(scene), 0.1f).succeeded);

        l2d::PathFollowerConfig2D invalid;
        invalid.goalTolerance = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!l2d::PathFollower2D::isValidConfig(invalid));
    }

    auto replayFollowing()
    {
        l2d::Scene scene;
        Agent agent = addAgent(scene);
        l2d::NavigationGridConfig2D config;
        config.size = {4u, 1u};
        config.cellSize = {10.f, 10.f};
        l2d::NavigationGrid2D grid(config);
        agent.follower.setDestination(grid, l2d::AStarPathfinder2D{}, {35.f, 5.f});
        for (int tick = 0; tick < 30; ++tick)
            agent.follower.follow(l2d::PhysicsQueryContext2D(scene), 0.1f);
        return std::make_tuple(agent.object.transform.position(), agent.controller.state().velocity,
                               agent.follower.state().status, agent.follower.state().nextWaypoint);
    }

    void testDeterministicReplay()
    {
        L2D_REQUIRE_DETERMINISTIC_REPLAY(replayFollowing);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("local avoidance validation and determinism",
                       testAvoidanceValidationAndDeterminism, failures);
    l2d::test::runTest("path destination following and staleness",
                       testSetDestinationFollowAndStaleness, failures);
    l2d::test::runTest("path follower navigation position offset", testNavigationPositionOffset,
                       failures);
    l2d::test::runTest("path follower stuck and failures", testStuckAndFailureContracts, failures);
    l2d::test::runTest("path follower deterministic replay", testDeterministicReplay, failures);
    return failures == 0 ? 0 : 1;
}

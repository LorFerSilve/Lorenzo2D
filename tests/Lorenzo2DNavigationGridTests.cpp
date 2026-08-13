#include "TestSupport.hpp"

#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>

#include <limits>

namespace
{
    void testGridValidationCoordinatesAndRevision()
    {
        l2d::NavigationGridConfig2D config;
        config.size = {3u, 2u};
        config.cellSize = {10.f, 20.f};
        config.origin = {-5.f, 7.f};
        L2D_REQUIRE(l2d::NavigationGrid2D::isValidConfig(config));
        l2d::NavigationGrid2D grid(config);
        L2D_REQUIRE_EQUAL(grid.cellCount(), 6u);
        L2D_REQUIRE_APPROX_2D(grid.cellCenter({1, 1}), sf::Vector2f(10.f, 37.f), 0.0001f);
        L2D_REQUIRE_EQUAL(*grid.worldToCell({-4.9f, 7.1f}), sf::Vector2i(0, 0));
        L2D_REQUIRE_EQUAL(*grid.worldToCell({24.9f, 46.9f}), sf::Vector2i(2, 1));
        L2D_REQUIRE(!grid.worldToCell({25.f, 20.f}));

        const std::uint64_t initialRevision = grid.revision();
        L2D_REQUIRE(grid.setTraversalCost({1, 0}, 2.5f));
        L2D_REQUIRE(grid.revision() > initialRevision);
        const std::uint64_t changedRevision = grid.revision();
        L2D_REQUIRE(grid.setTraversalCost({1, 0}, 2.5f));
        L2D_REQUIRE_EQUAL(grid.revision(), changedRevision);
        L2D_REQUIRE(!grid.setTraversalCost({1, 0}, 0.f));
        L2D_REQUIRE(!grid.setWalkable({3, 0}, false));

        config.size = {0u, 2u};
        L2D_REQUIRE(!l2d::NavigationGrid2D::isValidConfig(config));
        config.size = {2u, 2u};
        config.cellSize.x = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!l2d::NavigationGrid2D::isValidConfig(config));
        config.cellSize = {1.f, 1.f};
        config.size.x = static_cast<unsigned int>(std::numeric_limits<int>::max()) + 1u;
        L2D_REQUIRE(!l2d::NavigationGrid2D::isValidConfig(config));
    }

    void testTileMapAndPhysicsBaking()
    {
        l2d::AsciiTileMapImporter importer;
        importer.mapCharacter('G', 1u);
        importer.mapCharacter('W', 2u);
        l2d::TileMapData data;
        L2D_REQUIRE(importer.import({"GWG", "GGG"}, {10.f, 10.f}, data));

        l2d::TileDefinition water = *data.definition(2u);
        water.movementCost = 3.f;
        L2D_REQUIRE(data.setDefinition(water));
        auto grid = l2d::navigationGridFromTileMap(data);
        L2D_REQUIRE(grid.has_value());
        L2D_REQUIRE(grid->cell({1, 0})->walkable);
        L2D_REQUIRE_APPROX(grid->cell({1, 0})->traversalCost, 3.f, 0.0001f);

        water.navigable = false;
        L2D_REQUIRE(data.setDefinition(water));
        grid = l2d::navigationGridFromTileMap(data);
        L2D_REQUIRE(!grid->cell({1, 0})->walkable);

        l2d::Scene scene;
        l2d::GameObject& obstacle = scene.createGameObject("Navigation obstacle");
        obstacle.transform.setPosition({15.f, 5.f});
        obstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{8.f, 8.f});
        l2d::NavigationGridConfig2D physicsConfig;
        physicsConfig.size = {3u, 1u};
        physicsConfig.cellSize = {10.f, 10.f};
        l2d::NavigationGrid2D physicsGrid(physicsConfig);
        L2D_REQUIRE_EQUAL(physicsGrid.bakeObstacles(l2d::PhysicsQueryContext2D(scene), 0.f), 1u);
        L2D_REQUIRE(physicsGrid.cell({0, 0})->walkable);
        L2D_REQUIRE(!physicsGrid.cell({1, 0})->walkable);
        L2D_REQUIRE(physicsGrid.cell({2, 0})->walkable);
    }

    void testDeterministicWeightedAStar()
    {
        l2d::NavigationGridConfig2D config;
        config.size = {5u, 3u};
        config.cellSize = {8.f, 8.f};
        l2d::NavigationGrid2D grid(config);
        L2D_REQUIRE(grid.setTraversalCost({2, 1}, 20.f));

        const l2d::AStarPathfinder2D pathfinder;
        const l2d::NavigationPath2D first = pathfinder.findPath(grid, {0, 1}, {4, 1});
        const l2d::NavigationPath2D second = pathfinder.findPath(grid, {0, 1}, {4, 1});
        L2D_REQUIRE(first.succeeded());
        L2D_REQUIRE_EQUAL(first.cells, second.cells);
        L2D_REQUIRE_APPROX(first.totalCost, 6.f, 0.0001f);
        L2D_REQUIRE_EQUAL(first.cells.front(), sf::Vector2i(0, 1));
        L2D_REQUIRE_EQUAL(first.cells.back(), sf::Vector2i(4, 1));
        L2D_REQUIRE(first.points.size() < first.cells.size());
        L2D_REQUIRE_EQUAL(first.gridRevision, grid.revision());

        const l2d::NavigationPath2D world = pathfinder.findWorldPath(grid, {1.f, 9.f}, {39.f, 9.f});
        L2D_REQUIRE(world.succeeded());
        L2D_REQUIRE_APPROX_2D(world.points.front(), sf::Vector2f(1.f, 9.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(world.points.back(), sf::Vector2f(39.f, 9.f), 0.0001f);
    }

    void testDiagonalRulesAndFailures()
    {
        l2d::NavigationGridConfig2D config;
        config.size = {2u, 2u};
        config.connectivity = l2d::NavigationConnectivity2D::EightWay;
        l2d::NavigationGrid2D grid(config);
        grid.setWalkable({1, 0}, false);
        grid.setWalkable({0, 1}, false);
        const l2d::AStarPathfinder2D pathfinder;
        L2D_REQUIRE_EQUAL(pathfinder.findPath(grid, {0, 0}, {1, 1}).status,
                          l2d::NavigationPathStatus2D::NoPath);

        config.allowDiagonalCornerCutting = true;
        L2D_REQUIRE(grid.reset(config));
        grid.setWalkable({1, 0}, false);
        grid.setWalkable({0, 1}, false);
        const l2d::NavigationPath2D diagonal = pathfinder.findPath(grid, {0, 0}, {1, 1});
        L2D_REQUIRE(diagonal.succeeded());
        L2D_REQUIRE_EQUAL(diagonal.cells.size(), 2u);

        L2D_REQUIRE_EQUAL(pathfinder.findPath(grid, {-1, 0}, {1, 1}).status,
                          l2d::NavigationPathStatus2D::InvalidStart);
        L2D_REQUIRE_EQUAL(pathfinder.findPath(grid, {0, 0}, {3, 1}).status,
                          l2d::NavigationPathStatus2D::InvalidGoal);
        l2d::NavigationPathOptions2D limited;
        limited.maximumVisitedNodes = 1u;
        L2D_REQUIRE_EQUAL(pathfinder.findPath(grid, {0, 0}, {1, 1}, limited).status,
                          l2d::NavigationPathStatus2D::SearchLimitReached);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("navigation grid validation and coordinates",
                       testGridValidationCoordinatesAndRevision, failures);
    l2d::test::runTest("navigation tilemap and physics bake", testTileMapAndPhysicsBaking,
                       failures);
    l2d::test::runTest("deterministic weighted A star", testDeterministicWeightedAStar, failures);
    l2d::test::runTest("navigation diagonal rules and failures", testDiagonalRulesAndFailures,
                       failures);
    return failures == 0 ? 0 : 1;
}

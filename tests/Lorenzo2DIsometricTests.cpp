#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Isometric/IsometricPlacementGrid2D.hpp>
#include <Lorenzo2D/Isometric/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include "TestSupport.hpp"

#include <SFML/Graphics/View.hpp>

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    using l2d::test::runTest;

    constexpr float ComparisonEpsilon = 0.0001f;

    l2d::IsometricProjection2D makeProjection(sf::Vector2f renderOrigin = {400.f, 64.f})
    {
        l2d::IsometricProjectionConfig2D config;
        config.worldCellSize = {32.f, 32.f};
        config.renderTileSize = {64.f, 32.f};
        config.renderOrigin = renderOrigin;
        return l2d::IsometricProjection2D(config);
    }

    void testProjectionValidationRoundTripAndBounds()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        l2d::IsometricProjection2D projection = makeProjection();
        const l2d::IsometricProjectionConfig2D validConfig = projection.config();

        l2d::IsometricProjectionConfig2D invalid = validConfig;
        invalid.worldCellSize.x = 0.f;
        L2D_REQUIRE(!l2d::IsometricProjection2D::isValidConfig(invalid));
        L2D_REQUIRE(!projection.setConfig(invalid));
        L2D_REQUIRE_EQUAL(projection.config().worldCellSize, validConfig.worldCellSize);

        invalid = validConfig;
        invalid.renderOrigin.y = nan;
        L2D_REQUIRE(!projection.setConfig(invalid));
        L2D_REQUIRE_EQUAL(projection.config().renderOrigin, validConfig.renderOrigin);

        const sf::Vector2f world{96.f, 64.f};
        const sf::Vector2f render = projection.worldToRender(world);
        L2D_REQUIRE_APPROX_2D(render, (sf::Vector2f{432.f, 144.f}), ComparisonEpsilon);
        L2D_REQUIRE_APPROX_2D(projection.renderToWorld(render), world, ComparisonEpsilon);
        L2D_REQUIRE_APPROX(projection.depthFor(world), 144.f, ComparisonEpsilon);

        sf::Vector2f renderMinimum;
        sf::Vector2f renderMaximum;
        L2D_REQUIRE(projection.projectBounds({32.f, 64.f}, {96.f, 128.f}, renderMinimum,
                                             renderMaximum));
        L2D_REQUIRE_APPROX_2D(renderMinimum, (sf::Vector2f{304.f, 112.f}),
                             ComparisonEpsilon);
        L2D_REQUIRE_APPROX_2D(renderMaximum, (sf::Vector2f{432.f, 176.f}),
                             ComparisonEpsilon);

        L2D_REQUIRE(!projection.projectBounds({10.f, 10.f}, {0.f, 20.f}, renderMinimum,
                                              renderMaximum));
        L2D_REQUIRE(std::isnan(projection.worldToRender({nan, 0.f}).x));
        L2D_REQUIRE(std::isnan(projection.renderToWorld({nan, 0.f}).x));
    }

    void testPlacementPickingAndOccupancyAreTransactional()
    {
        l2d::IsometricPlacementGridConfig2D config;
        config.size = {4u, 3u};
        config.cellSize = {32.f, 32.f};

        l2d::IsometricPlacementGrid2D grid(config);
        L2D_REQUIRE_EQUAL(grid.cellCount(), 12u);
        L2D_REQUIRE(grid.contains({3u, 2u}));
        L2D_REQUIRE(!grid.contains({4u, 2u}));

        const l2d::IsometricProjection2D projection = makeProjection({200.f, 50.f});
        const std::optional<sf::Vector2f> center = grid.cellWorldCenter({2u, 1u});
        L2D_REQUIRE(center.has_value());
        L2D_REQUIRE_APPROX_2D(*center, (sf::Vector2f{80.f, 48.f}), ComparisonEpsilon);

        const sf::Vector2f render = projection.worldToRender(*center);
        L2D_REQUIRE_APPROX_2D(render, (sf::Vector2f{232.f, 114.f}), ComparisonEpsilon);
        const std::optional<sf::Vector2u> picked = grid.pickCell(render, projection);
        L2D_REQUIRE(picked.has_value());
        L2D_REQUIRE_EQUAL(*picked, sf::Vector2u(2u, 1u));

        L2D_REQUIRE(grid.canPlace({0u, 0u}, {2u, 2u}));
        L2D_REQUIRE(grid.place({0u, 0u}, {2u, 2u}));
        L2D_REQUIRE(grid.occupied({1u, 1u}));
        L2D_REQUIRE(!grid.canPlace({1u, 1u}));
        L2D_REQUIRE(!grid.place({3u, 2u}, {2u, 1u}));

        L2D_REQUIRE(grid.setBlocked({3u, 2u}, true));
        L2D_REQUIRE(grid.blocked({3u, 2u}));
        L2D_REQUIRE(!grid.setOccupied({3u, 2u}, true));

        L2D_REQUIRE(grid.remove({0u, 0u}, {2u, 2u}));
        L2D_REQUIRE(!grid.occupied({1u, 1u}));
        L2D_REQUIRE(grid.blocked({3u, 2u}));
        L2D_REQUIRE(grid.place({0u, 0u}));
        grid.clearPlacements();
        L2D_REQUIRE(!grid.occupied({0u, 0u}));
        L2D_REQUIRE(grid.blocked({3u, 2u}));

        const l2d::IsometricPlacementGridConfig2D previous = grid.config();
        l2d::IsometricPlacementGridConfig2D invalid = previous;
        invalid.size.x = 0u;
        L2D_REQUIRE(!grid.reset(invalid));
        L2D_REQUIRE_EQUAL(grid.config().size, previous.size);
        L2D_REQUIRE_EQUAL(grid.cellCount(), 12u);
    }

    void testPlacementGridBakesIsometricCollisionCells()
    {
        l2d::TileMapData data;
        L2D_REQUIRE(data.setDimensions(3u, 2u));
        L2D_REQUIRE(data.setTileSize({32.f, 32.f}));
        data.setOrientation(l2d::TileMapOrientation::Isometric);

        l2d::TileDefinition solid;
        solid.id = 1u;
        solid.collision = l2d::TileCollisionKind::Solid;
        L2D_REQUIRE(data.setDefinition(solid));

        l2d::TileDefinition floor;
        floor.id = 2u;
        L2D_REQUIRE(data.setDefinition(floor));

        l2d::TileMapLayer ground;
        ground.name = "Ground";
        ground.tiles = {1u, 2u, 0u, 0u, 2u, 0u};
        L2D_REQUIRE(data.addLayer(ground));

        l2d::TileMapLayer collision;
        collision.name = "Reserved";
        collision.role = l2d::TileMapLayerRole::Collision;
        collision.visible = false;
        collision.tiles = {0u, 0u, 0u, 0u, 2u, 0u};
        L2D_REQUIRE(data.addLayer(collision));

        l2d::IsometricPlacementGrid2D grid;
        L2D_REQUIRE(grid.resetFromTileMap(data, {64.f, 96.f}));
        L2D_REQUIRE_EQUAL(grid.config().size, sf::Vector2u(3u, 2u));
        L2D_REQUIRE_EQUAL(grid.config().cellSize, sf::Vector2f(32.f, 32.f));
        L2D_REQUIRE_EQUAL(grid.config().worldOrigin, sf::Vector2f(64.f, 96.f));
        L2D_REQUIRE(grid.blocked({0u, 0u}));
        L2D_REQUIRE(grid.blocked({1u, 1u}));
        L2D_REQUIRE(grid.available({1u, 0u}));

        data.setOrientation(l2d::TileMapOrientation::Orthogonal);
        L2D_REQUIRE(!grid.resetFromTileMap(data));
        L2D_REQUIRE_EQUAL(grid.config().worldOrigin, sf::Vector2f(64.f, 96.f));
    }

    void testProjectedDepthUsesStableIsometricFootPoints()
    {
        l2d::Scene scene;

        l2d::GameObject& first = scene.createGameObject("First");
        first.transform.setPosition({64.f, 0.f});
        first.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);

        l2d::GameObject& second = scene.createGameObject("Second");
        second.transform.setPosition({0.f, 64.f});
        second.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);

        l2d::GameObject& third = scene.createGameObject("Third");
        third.transform.setPosition({64.f, 64.f});
        third.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);

        const l2d::IsometricProjection2D projection = makeProjection({0.f, 0.f});
        const l2d::RenderContext2D context{1.f, &projection, l2d::RenderPass2D::World};
        l2d::RenderQueue2D queue;
        queue.build(scene, context);

        L2D_REQUIRE_EQUAL(queue.size(), 3u);
        L2D_REQUIRE_APPROX(queue.entries()[0].sortKey.depth, 32.f, ComparisonEpsilon);
        L2D_REQUIRE_APPROX(queue.entries()[1].sortKey.depth, 32.f, ComparisonEpsilon);
        L2D_REQUIRE_APPROX(queue.entries()[2].sortKey.depth, 64.f, ComparisonEpsilon);
        L2D_REQUIRE(queue.entries()[0].insertionOrder < queue.entries()[1].insertionOrder);
    }

    void testProjectedTileMapCullingUsesRenderBounds()
    {
        l2d::TileMapData data;
        L2D_REQUIRE(data.setDimensions(32u, 32u));
        L2D_REQUIRE(data.setTileSize({16.f, 16.f}));
        data.setOrientation(l2d::TileMapOrientation::Isometric);

        l2d::TileDefinition groundDefinition;
        groundDefinition.id = 1u;
        L2D_REQUIRE(data.setDefinition(groundDefinition));

        l2d::TileMapLayer ground;
        ground.name = "Ground";
        ground.tiles.assign(data.cellCount(), 1u);
        L2D_REQUIRE(data.addLayer(std::move(ground)));

        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setRenderChunkSize({4u, 4u});
        L2D_REQUIRE(tileMap.loadFromData(scene, data));

        l2d::IsometricProjectionConfig2D projectionConfig;
        projectionConfig.worldCellSize = {16.f, 16.f};
        projectionConfig.renderTileSize = {32.f, 16.f};
        const l2d::IsometricProjection2D projection(projectionConfig);
        const l2d::RenderContext2D context{1.f, &projection, l2d::RenderPass2D::World};
        const sf::View projectedView({-384.f, 256.f}, {128.f, 128.f});

        const l2d::TileMapRenderStats orthogonalStats =
            tileMap.renderStatsForView(projectedView);
        const l2d::TileMapRenderStats projectedStats =
            tileMap.renderStatsForView(projectedView, context);

        L2D_REQUIRE_EQUAL(orthogonalStats.visibleChunkCount, 0u);
        L2D_REQUIRE(projectedStats.visibleChunkCount > 0u);
        L2D_REQUIRE(projectedStats.culledChunkCount > 0u);
        L2D_REQUIRE_EQUAL(projectedStats.visibleChunkCount + projectedStats.culledChunkCount,
                          projectedStats.chunkCount);
        L2D_REQUIRE_EQUAL(projectedStats.submittedVertexCount,
                          projectedStats.submittedTileCount * 6u);
    }
}

int main()
{
    int failures = 0;

    runTest("isometric projection validation round trip and bounds",
            testProjectionValidationRoundTripAndBounds, failures);
    runTest("isometric placement picking and occupancy",
            testPlacementPickingAndOccupancyAreTransactional, failures);
    runTest("isometric placement grid bakes collision cells",
            testPlacementGridBakesIsometricCollisionCells, failures);
    runTest("isometric projected depth is stable",
            testProjectedDepthUsesStableIsometricFootPoints, failures);
    runTest("isometric projected tilemap culling",
            testProjectedTileMapCullingUsesRenderBounds, failures);

    if (failures != 0)
    {
        std::cerr << failures << " isometric test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D isometric tests passed.\n";
    return 0;
}

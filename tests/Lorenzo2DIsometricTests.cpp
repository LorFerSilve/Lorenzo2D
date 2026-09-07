#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/IsometricTileGrid2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/Graphics/View.hpp>

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

    constexpr float Epsilon = 0.0001f;

    void requireApprox(sf::Vector2f actual, sf::Vector2f expected)
    {
        L2D_REQUIRE(std::fabs(actual.x - expected.x) <= Epsilon);
        L2D_REQUIRE(std::fabs(actual.y - expected.y) <= Epsilon);
    }

    void testProjectionRoundTripsAndDepth()
    {
        const l2d::IsometricProjection2D projection({32.f, 32.f}, {64.f, 32.f}, {320.f, 64.f});
        const sf::Vector2f world{96.f, 64.f};
        const sf::Vector2f render = projection.worldToRender(world);

        requireApprox(render, {352.f, 144.f});
        requireApprox(projection.renderToWorld(render), world);
        L2D_REQUIRE(std::fabs(projection.depthFor(world) - render.y) <= Epsilon);
        L2D_REQUIRE(!projection.isIdentity());
    }

    void testProjectionBoundsAreConservative()
    {
        const l2d::IsometricProjection2D projection({32.f, 32.f}, {64.f, 32.f});
        sf::Vector2f minimum;
        sf::Vector2f maximum;
        L2D_REQUIRE(projection.projectBounds({0.f, 0.f}, {64.f, 64.f}, minimum, maximum));
        requireApprox(minimum, {-64.f, 0.f});
        requireApprox(maximum, {64.f, 64.f});

        const float nan = std::numeric_limits<float>::quiet_NaN();
        L2D_REQUIRE(!projection.projectBounds({nan, 0.f}, {64.f, 64.f}, minimum, maximum));
    }

    void testPickingAndPlacement()
    {
        const l2d::IsometricProjection2D projection({32.f, 32.f}, {64.f, 32.f}, {256.f, 32.f});
        const l2d::IsometricTileGrid2D grid(8u, 6u, projection);
        const l2d::TileMapCell cell{3u, 2u};
        const std::optional<sf::Vector2f> render = grid.renderPosition(cell);
        L2D_REQUIRE(render.has_value());
        L2D_REQUIRE(grid.pick(*render) == std::optional<l2d::TileMapCell>(cell));

        const std::optional<sf::Vector2f> world = grid.worldPosition(cell);
        L2D_REQUIRE(world.has_value());
        requireApprox(*world, {112.f, 80.f});
        L2D_REQUIRE(!grid.pick({-10000.f, -10000.f}).has_value());
        L2D_REQUIRE(!grid.worldPosition({8u, 0u}).has_value());
    }

    void testIsometricTileDataConstructor()
    {
        l2d::TileMapData data;
        L2D_REQUIRE(data.setDimensions(4u, 3u));
        L2D_REQUIRE(data.setTileSize({32.f, 32.f}));
        data.setOrientation(l2d::TileMapOrientation::Isometric);
        const l2d::IsometricTileGrid2D grid(data, {64.f, 32.f});
        L2D_REQUIRE(grid.width() == 4u);
        L2D_REQUIRE(grid.height() == 3u);

        data.setOrientation(l2d::TileMapOrientation::Orthogonal);
        bool threw = false;
        try
        {
            const l2d::IsometricTileGrid2D invalid(data, {64.f, 32.f});
            (void)invalid;
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }
        L2D_REQUIRE(threw);
    }

    void testVisibleRegionCanDriveStreamingCulling()
    {
        const l2d::IsometricTileGrid2D grid(128u, 128u,
                                            l2d::IsometricProjection2D({32.f, 32.f}, {64.f, 32.f}));
        sf::View view({0.f, 0.f}, {256.f, 128.f});
        view.setCenter(grid.projection().cellToRender({64, 64}));

        const std::optional<l2d::TileMapRegion> region = grid.visibleRegionForView(view, 2u);
        L2D_REQUIRE(region.has_value());
        L2D_REQUIRE(region->columnCount < grid.width());
        L2D_REQUIRE(region->rowCount < grid.height());
        L2D_REQUIRE(region->firstColumn <= 64u);
        L2D_REQUIRE(region->firstRow <= 64u);
        L2D_REQUIRE(region->firstColumn + region->columnCount > 64u);
        L2D_REQUIRE(region->firstRow + region->rowCount > 64u);

        sf::Vector2f minimum;
        sf::Vector2f maximum;
        L2D_REQUIRE(grid.projectedBounds(*region, minimum, maximum));
        L2D_REQUIRE(minimum.x < maximum.x);
        L2D_REQUIRE(minimum.y < maximum.y);
    }

    void testProjectedDepthSortUsesIsometricFootPoint()
    {
        l2d::Scene scene;
        l2d::GameObject& back = scene.createGameObject("back");
        l2d::GameObject& front = scene.createGameObject("front");
        back.transform.setPosition({32.f, 32.f});
        front.transform.setPosition({64.f, 64.f});
        back.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);
        front.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);

        const l2d::IsometricProjection2D projection({32.f, 32.f}, {64.f, 32.f});
        const l2d::RenderContext2D context{1.f, &projection, l2d::RenderPass2D::World};
        l2d::RenderQueue2D queue;
        queue.build(scene, context);

        L2D_REQUIRE(queue.size() == 2u);
        L2D_REQUIRE(queue.entries()[0].gameObject.get() == &back);
        L2D_REQUIRE(queue.entries()[1].gameObject.get() == &front);
        L2D_REQUIRE(queue.entries()[0].sortKey.depth < queue.entries()[1].sortKey.depth);
    }
}

int main()
{
    int failures = 0;
    runTest("isometric projection round trips and depth", testProjectionRoundTripsAndDepth,
            failures);
    runTest("isometric projection bounds", testProjectionBoundsAreConservative, failures);
    runTest("isometric picking and placement", testPickingAndPlacement, failures);
    runTest("isometric tile data constructor", testIsometricTileDataConstructor, failures);
    runTest("isometric visible region", testVisibleRegionCanDriveStreamingCulling, failures);
    runTest("isometric projected depth sort", testProjectedDepthSortUsesIsometricFootPoint,
            failures);
    return failures == 0 ? 0 : 1;
}

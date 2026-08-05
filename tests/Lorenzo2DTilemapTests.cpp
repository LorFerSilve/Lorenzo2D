#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::approximatelyEqual;
    using l2d::test::runTest;
    using l2d::test::TemporaryFile;

    void testTileMapBuildsChunkedRenderingAndCullsByView()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;

        L2D_REQUIRE(tileMap.renderChunkSize() == sf::Vector2u(16u, 16u));
        tileMap.setRenderChunkSize({0u, 0u});
        L2D_REQUIRE(tileMap.renderChunkSize() == sf::Vector2u(1u, 1u));

        tileMap.setTileSize({1.f, 1.f});
        tileMap.setRenderChunkSize({8u, 8u});

        const l2d::TileMap::Layout denseLayout(32, std::string(32, '#'));
        tileMap.loadFromLayout(scene, denseLayout);

        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(8u, 8u));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 1024u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 16u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 1u);
        L2D_REQUIRE(scene.gameObjectCount() == 2u);

        const sf::View allVisible({16.f, 16.f}, {32.f, 32.f});
        const l2d::TileMapRenderStats allStats = tileMap.renderStatsForView(allVisible);
        L2D_REQUIRE(allStats.chunkCount == 16u);
        L2D_REQUIRE(allStats.visibleChunkCount == 16u);
        L2D_REQUIRE(allStats.culledChunkCount == 0u);
        L2D_REQUIRE(allStats.solidTileCount == 1024u);
        L2D_REQUIRE(allStats.submittedTileCount == 1024u);
        L2D_REQUIRE(allStats.submittedVertexCount == 6144u);
        L2D_REQUIRE(allStats.drawCallCount == 16u);

        const sf::View oneChunk({4.f, 4.f}, {7.f, 7.f});
        const l2d::TileMapRenderStats oneChunkStats = tileMap.renderStatsForView(oneChunk);
        L2D_REQUIRE(oneChunkStats.chunkCount == 16u);
        L2D_REQUIRE(oneChunkStats.visibleChunkCount == 1u);
        L2D_REQUIRE(oneChunkStats.culledChunkCount == 15u);
        L2D_REQUIRE(oneChunkStats.solidTileCount == 1024u);
        L2D_REQUIRE(oneChunkStats.submittedTileCount == 64u);
        L2D_REQUIRE(oneChunkStats.submittedVertexCount == 384u);
        L2D_REQUIRE(oneChunkStats.drawCallCount == 1u);

        sf::View rotatedView({4.f, 4.f}, {7.f, 7.f});
        rotatedView.setRotation(sf::degrees(45.f));
        const l2d::TileMapRenderStats rotatedStats = tileMap.renderStatsForView(rotatedView);
        L2D_REQUIRE(rotatedStats.visibleChunkCount == 4u);
        L2D_REQUIRE(rotatedStats.culledChunkCount == 12u);
        L2D_REQUIRE(rotatedStats.submittedTileCount == 256u);
        L2D_REQUIRE(rotatedStats.drawCallCount == 4u);

        const sf::View outsideMap({-20.f, -20.f}, {4.f, 4.f});
        const l2d::TileMapRenderStats outsideStats = tileMap.renderStatsForView(outsideMap);
        L2D_REQUIRE(outsideStats.chunkCount == 16u);
        L2D_REQUIRE(outsideStats.visibleChunkCount == 0u);
        L2D_REQUIRE(outsideStats.culledChunkCount == 16u);
        L2D_REQUIRE(outsideStats.solidTileCount == 1024u);
        L2D_REQUIRE(outsideStats.submittedTileCount == 0u);
        L2D_REQUIRE(outsideStats.submittedVertexCount == 0u);
        L2D_REQUIRE(outsideStats.drawCallCount == 0u);

        sf::View invalidView;
        invalidView.setSize({0.f, 0.f});
        const l2d::TileMapRenderStats invalidStats = tileMap.renderStatsForView(invalidView);
        L2D_REQUIRE(invalidStats.visibleChunkCount == 16u);
        L2D_REQUIRE(invalidStats.culledChunkCount == 0u);

        tileMap.setRenderChunkSize({4u, 4u});
        L2D_REQUIRE(tileMap.renderChunkSize() == sf::Vector2u(4u, 4u));
        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(8u, 8u));
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 16u);
    }

    void testTileMapMergesStaticCollisionRectanglesExactly()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({10.f, 20.f});
        tileMap.setRenderChunkSize({2u, 2u});

        const l2d::TileMap::Layout layout{"###.#", "##.##", "#..##", "#"};
        tileMap.loadFromLayout(scene, layout);

        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 12u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 6u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 5u);
        L2D_REQUIRE(scene.gameObjectCount() == 6u);

        std::vector<const l2d::BoxCollider2D*> colliders;
        for (const auto& gameObject : scene.gameObjects())
        {
            const l2d::BoxCollider2D* collider = gameObject->getComponent<l2d::BoxCollider2D>();
            if (collider == nullptr) continue;

            L2D_REQUIRE(gameObject->getComponent<l2d::RigidBody2D>() == nullptr);
            colliders.push_back(collider);
        }

        L2D_REQUIRE(colliders.size() == 5u);

        const std::vector<sf::Vector2f> expectedPositions{
            {0.f, 0.f}, {40.f, 0.f}, {0.f, 20.f}, {30.f, 20.f}, {0.f, 40.f}};
        const std::vector<sf::Vector2f> expectedSizes{
            {30.f, 20.f}, {10.f, 60.f}, {20.f, 20.f}, {10.f, 40.f}, {10.f, 40.f}};

        std::vector<std::vector<bool>> covered(layout.size());
        for (std::size_t row = 0; row < layout.size(); ++row)
            covered[row].resize(layout[row].size(), false);

        for (std::size_t index = 0; index < colliders.size(); ++index)
        {
            const sf::Vector2f position = colliders[index]->min();
            const sf::Vector2f size = colliders[index]->size();
            L2D_REQUIRE(approximatelyEqual(position.x, expectedPositions[index].x));
            L2D_REQUIRE(approximatelyEqual(position.y, expectedPositions[index].y));
            L2D_REQUIRE(approximatelyEqual(size.x, expectedSizes[index].x));
            L2D_REQUIRE(approximatelyEqual(size.y, expectedSizes[index].y));

            const std::size_t firstColumn = static_cast<std::size_t>(position.x / 10.f);
            const std::size_t firstRow = static_cast<std::size_t>(position.y / 20.f);
            const std::size_t columnCount = static_cast<std::size_t>(size.x / 10.f);
            const std::size_t rowCount = static_cast<std::size_t>(size.y / 20.f);

            for (std::size_t row = firstRow; row < firstRow + rowCount; ++row)
            {
                for (std::size_t column = firstColumn; column < firstColumn + columnCount; ++column)
                {
                    L2D_REQUIRE(row < layout.size());
                    L2D_REQUIRE(column < layout[row].size());
                    L2D_REQUIRE(layout[row][column] == '#');
                    L2D_REQUIRE(!covered[row][column]);
                    covered[row][column] = true;
                }
            }
        }

        for (std::size_t row = 0; row < layout.size(); ++row)
        {
            for (std::size_t column = 0; column < layout[row].size(); ++column)
            {
                L2D_REQUIRE(covered[row][column] == (layout[row][column] == '#'));
            }
        }

        l2d::Scene fractionalScene;
        l2d::TileMap fractionalMap;
        fractionalMap.setTileSize({1.1f, 1.1f});
        std::string fractionalRow(1u, '.');
        fractionalRow.append(100u, '#');
        fractionalMap.loadFromLayout(fractionalScene, {fractionalRow});

        const l2d::BoxCollider2D* fractionalCollider = nullptr;

        for (const auto& gameObject : fractionalScene.gameObjects())
        {
            const l2d::BoxCollider2D* collider = gameObject->getComponent<l2d::BoxCollider2D>();

            if (collider != nullptr)
            {
                L2D_REQUIRE(fractionalCollider == nullptr);
                fractionalCollider = collider;
            }
        }

        L2D_REQUIRE(fractionalCollider != nullptr);
        L2D_REQUIRE(fractionalCollider->max().x == fractionalMap.worldSize().x);
    }

    void testTileMapReloadAndUnloadOwnGeneratedObjects()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;

        tileMap.setTileSize(
            {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()});
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().y, 1.f));

        tileMap.setTileSize({std::numeric_limits<float>::denorm_min(), 0.00001f});
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().x, 0.0001f));
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().y, 0.0001f));

        tileMap.setTileSize({10.f, 20.f});
        tileMap.loadFromLayout(scene, {"#P", "", "C#"});
        L2D_REQUIRE(tileMap.layout().size() == 3u);
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, 20.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 60.f));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 2u);
        L2D_REQUIRE(scene.gameObjectCount() == 3u);

        sf::Vector2f playerPosition;
        L2D_REQUIRE(tileMap.findFirstTilePosition('P', playerPosition, true));
        L2D_REQUIRE(approximatelyEqual(playerPosition.x, 15.f));
        L2D_REQUIRE(approximatelyEqual(playerPosition.y, 10.f));

        tileMap.setTileSize({5.f, 5.f});
        L2D_REQUIRE(approximatelyEqual(tileMap.loadedTileSize().x, 10.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.loadedTileSize().y, 20.f));

        tileMap.loadFromLayout(scene, {"##"});
        L2D_REQUIRE(scene.gameObjectCount() == 5u);
        L2D_REQUIRE(scene.activeGameObjectCount() == 2u);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 3u);
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 1u);

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE(scene.gameObjectCount() == 2u);

        tileMap.loadFromLayout(scene, {"P..", "..."});
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 0u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 0u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 0u);
        L2D_REQUIRE(scene.activeGameObjectCount() == 0u);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 2u);

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE(scene.gameObjectCount() == 0u);

        tileMap.unload();
        L2D_REQUIRE(tileMap.layout().empty());
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, 0.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 0.f));
        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(0u, 0u));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 0u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 0u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 0u);
        L2D_REQUIRE(scene.activeGameObjectCount() == 0u);
    }

    void testTileMapMoveTransfersGeneratedTileOwnership()
    {
        l2d::Scene sourceScene;
        l2d::TileMap source;
        source.loadFromLayout(sourceScene, {"#"});

        l2d::TileMap moved(std::move(source));
        L2D_REQUIRE(source.layout().empty());
        L2D_REQUIRE(moved.layout().size() == 1);
        L2D_REQUIRE(source.buildStats().solidTileCount == 0u);
        L2D_REQUIRE(moved.buildStats().solidTileCount == 1u);
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 2u);

        l2d::Scene destinationScene;
        l2d::TileMap destination;
        destination.loadFromLayout(destinationScene, {"#"});
        destination = std::move(moved);

        L2D_REQUIRE(destinationScene.activeGameObjectCount() == 0u);
        L2D_REQUIRE(destinationScene.destroyQueuedGameObjectCount() == 2u);
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 2u);
        L2D_REQUIRE(moved.layout().empty());
        L2D_REQUIRE(moved.buildStats().solidTileCount == 0u);
        L2D_REQUIRE(destination.layout().size() == 1);
        L2D_REQUIRE(destination.buildStats().solidTileCount == 1u);

        destinationScene.destroyQueuedGameObjects();
        L2D_REQUIRE(destinationScene.gameObjectCount() == 0u);

        destination.unload();
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 0u);
        sourceScene.destroyQueuedGameObjects();
        L2D_REQUIRE(sourceScene.gameObjectCount() == 0u);

        l2d::TileMap mapWhoseSceneExpires;
        {
            l2d::Scene shortLivedScene;
            mapWhoseSceneExpires.loadFromLayout(shortLivedScene, {"#"});
            L2D_REQUIRE(shortLivedScene.gameObjectCount() == 2u);
        }
        mapWhoseSceneExpires.unload();
        L2D_REQUIRE(mapWhoseSceneExpires.layout().empty());
        L2D_REQUIRE(mapWhoseSceneExpires.buildStats().solidTileCount == 0u);
    }

    void testTileMapFileLoadingPreservesBlankRowsAndIsTransactional()
    {
        const TemporaryFile temporaryFile("lorenzo2d_blank_rows_test");
        const std::filesystem::path& path = temporaryFile.path();

        {
            std::ofstream file(path, std::ios::trunc);
            file << "#\n\n#\n";
        }

        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({10.f, 20.f});
        tileMap.setRenderChunkSize({2u, 3u});

        L2D_REQUIRE(tileMap.loadFromFile(scene, path.string()));
        L2D_REQUIRE(tileMap.layout().size() == 3);
        L2D_REQUIRE(tileMap.layout()[1].empty());
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 60.f));
        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(2u, 3u));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 2u);

        const l2d::TileMap::Layout previousLayout = tileMap.layout();
        const sf::Vector2f previousWorldSize = tileMap.worldSize();
        const std::size_t previousObjectCount = scene.gameObjectCount();
        const l2d::TileMapBuildStats previousBuildStats = tileMap.buildStats();

        tileMap.setRenderChunkSize({5u, 7u});

        L2D_REQUIRE(!tileMap.loadFromFile(scene, path.string() + ".missing"));
        L2D_REQUIRE(tileMap.layout() == previousLayout);
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, previousWorldSize.x));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, previousWorldSize.y));
        L2D_REQUIRE(scene.gameObjectCount() == previousObjectCount);
        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(2u, 3u));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == previousBuildStats.solidTileCount);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == previousBuildStats.renderChunkCount);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount ==
                    previousBuildStats.collisionRectangleCount);

        tileMap.setTileSize({std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});

        bool overflowRejected = false;

        try
        {
            tileMap.loadFromLayout(scene, {"##"});
        }
        catch (const std::overflow_error&)
        {
            overflowRejected = true;
        }

        L2D_REQUIRE(overflowRejected);
        L2D_REQUIRE(tileMap.layout() == previousLayout);
        L2D_REQUIRE(tileMap.loadedTileSize() == sf::Vector2f(10.f, 20.f));
        L2D_REQUIRE(tileMap.loadedRenderChunkSize() == sf::Vector2u(2u, 3u));
        L2D_REQUIRE(scene.gameObjectCount() == previousObjectCount);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 0u);
    }

    void testTileSetAtlasMappingsRenderMultipleLayoutCharacters()
    {
        l2d::TileSet tileSet;
        L2D_REQUIRE(!tileSet.setTile('#', {{0, 0}, {0, 16}}));
        L2D_REQUIRE(!tileSet.setTileFromGrid('#', {0u, 0u}, {0u, 16u}));
        L2D_REQUIRE(tileSet.setTileFromGrid('#', {0u, 0u}, {16u, 16u}));
        L2D_REQUIRE(tileSet.setTileFromGrid('G', {2u, 1u}, {16u, 16u}));
        L2D_REQUIRE(tileSet.tileCount() == 2u);
        L2D_REQUIRE(tileSet.textureRect('G') ==
                    std::optional<sf::IntRect>(sf::IntRect({32, 16}, {16, 16})));

        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({8.f, 8.f});
        tileMap.setRenderChunkSize({2u, 2u});
        tileMap.setTileSet(tileSet);
        tileMap.loadFromLayout(scene, {"#G.P"});

        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().renderedTileCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().texturedTileCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 1u);
        L2D_REQUIRE(tileMap.loadedTileSet().tileCount() == 2u);

        const sf::View view({16.f, 4.f}, {32.f, 8.f});
        const l2d::TileMapRenderStats stats = tileMap.renderStatsForView(view);
        L2D_REQUIRE(stats.solidTileCount == 1u);
        L2D_REQUIRE(stats.submittedTileCount == 2u);
        L2D_REQUIRE(stats.submittedVertexCount == 12u);
        L2D_REQUIRE(stats.drawCallCount == 1u);

        l2d::TileSet replacement;
        L2D_REQUIRE(replacement.setTileFromGrid('W', {3u, 0u}, {16u, 16u}));
        tileMap.setTileSet(replacement);
        L2D_REQUIRE(tileMap.tileSet().contains('W'));
        L2D_REQUIRE(!tileMap.loadedTileSet().contains('W'));
        L2D_REQUIRE(tileMap.loadedTileSet().contains('G'));

        L2D_REQUIRE(tileSet.removeTile('G'));
        L2D_REQUIRE(!tileSet.removeTile('G'));
        tileSet.clearTiles();
        L2D_REQUIRE(tileSet.tileCount() == 0u);
    }
}

int main()
{
    int failures = 0;

    runTest("tilemap chunks cull and report build cost",
            testTileMapBuildsChunkedRenderingAndCullsByView, failures);
    runTest("tilemap merges static collision exactly",
            testTileMapMergesStaticCollisionRectanglesExactly, failures);
    runTest("tilemap reload and unload own generated objects",
            testTileMapReloadAndUnloadOwnGeneratedObjects, failures);
    runTest("tilemap move transfers ownership", testTileMapMoveTransfersGeneratedTileOwnership,
            failures);
    runTest("tilemap files preserve blank rows",
            testTileMapFileLoadingPreservesBlankRowsAndIsTransactional, failures);
    runTest("tilesets map atlas cells onto layout characters",
            testTileSetAtlasMappingsRenderMultipleLayoutCharacters, failures);

    if (failures != 0)
    {
        std::cerr << failures << " tilemap test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D tilemap tests passed.\n";
    return 0;
}

#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>
#include <Lorenzo2D/Tilemap/TileMapColliderBuilder2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>
#include <Lorenzo2D/Tilemap/TiledJsonImporter.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>

#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
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

    void testAnimatedTileDefinitionsSelectFramesTransactionally()
    {
        l2d::TileSet tileSet;
        const std::vector<l2d::TileAnimationFrame> frames{{{{0, 0}, {16, 16}}, 0.1f},
                                                          {{{16, 0}, {16, 16}}, 0.2f}};

        L2D_REQUIRE(tileSet.setAnimatedTile('W', frames));
        L2D_REQUIRE(tileSet.contains('W'));
        L2D_REQUIRE(tileSet.isAnimated('W'));
        L2D_REQUIRE(tileSet.animatedTileCount() == 1u);
        L2D_REQUIRE(tileSet.animation('W') != nullptr);
        L2D_REQUIRE(tileSet.animation('W')->size() == 2u);
        L2D_REQUIRE(tileSet.textureRect('W', 0.f) ==
                    std::optional<sf::IntRect>(frames[0].textureRect));
        L2D_REQUIRE(tileSet.textureRect('W', 0.1f) ==
                    std::optional<sf::IntRect>(frames[1].textureRect));
        L2D_REQUIRE(tileSet.textureRect('W', 0.31f) ==
                    std::optional<sf::IntRect>(frames[0].textureRect));

        const std::vector<l2d::TileAnimationFrame> invalid{{{{32, 0}, {16, 16}}, -1.f}};
        L2D_REQUIRE(!tileSet.setAnimatedTile('W', invalid));
        L2D_REQUIRE(tileSet.animation('W')->size() == 2u);

        L2D_REQUIRE(tileSet.setTile('W', {{48, 0}, {16, 16}}));
        L2D_REQUIRE(!tileSet.isAnimated('W'));
        L2D_REQUIRE(tileSet.animatedTileCount() == 0u);
    }

    void testTileMapEditsOneChunkAndStreamsResidentRegions()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({1.f, 1.f});
        tileMap.setRenderChunkSize({2u, 2u});
        tileMap.loadFromLayout(scene, l2d::TileMap::Layout(4u, std::string(4u, '#')));

        L2D_REQUIRE(tileMap.tileAt(0u, 0u) == std::optional<char>('#'));
        L2D_REQUIRE(!tileMap.tileAt(99u, 0u));
        L2D_REQUIRE(tileMap.setStreamRegion({0u, 0u, 2u, 2u}));
        L2D_REQUIRE(tileMap.streamRegion());
        L2D_REQUIRE(tileMap.streamRegion()->columnCount == 2u);
        L2D_REQUIRE(!tileMap.setStreamRegion({0u, 0u, 0u, 2u}));

        const sf::View allVisible({2.f, 2.f}, {4.f, 4.f});
        const l2d::TileMapRenderStats streamed = tileMap.renderStatsForView(allVisible);
        L2D_REQUIRE(streamed.chunkCount == 4u);
        L2D_REQUIRE(streamed.residentChunkCount == 1u);
        L2D_REQUIRE(streamed.nonResidentChunkCount == 3u);
        L2D_REQUIRE(streamed.visibleChunkCount == 1u);
        L2D_REQUIRE(streamed.culledChunkCount == 3u);
        L2D_REQUIRE(streamed.submittedTileCount == 4u);

        L2D_REQUIRE(tileMap.setTile(0u, 0u, '.'));
        L2D_REQUIRE(tileMap.tileAt(0u, 0u) == std::optional<char>('.'));
        L2D_REQUIRE(tileMap.lastUpdateStats().rebuiltRenderChunkCount == 1u);
        L2D_REQUIRE(tileMap.lastUpdateStats().collisionGeometryRebuilt);
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 15u);
        L2D_REQUIRE(tileMap.buildStats().renderedTileCount == 15u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 4u);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 1u);

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE(scene.gameObjectCount() == 1u + tileMap.buildStats().collisionRectangleCount);

        L2D_REQUIRE(tileMap.setTile(0u, 0u, '.'));
        L2D_REQUIRE(tileMap.lastUpdateStats().rebuiltRenderChunkCount == 0u);
        L2D_REQUIRE(!tileMap.setTile(99u, 0u, '#'));

        tileMap.clearStreamRegion();
        L2D_REQUIRE(!tileMap.streamRegion());
        const l2d::TileMapRenderStats fullyResident = tileMap.renderStatsForView(allVisible);
        L2D_REQUIRE(fullyResident.residentChunkCount == 4u);
        L2D_REQUIRE(fullyResident.nonResidentChunkCount == 0u);
        L2D_REQUIRE(fullyResident.visibleChunkCount == 4u);
    }

    void testMappedTileEditAvoidsCollisionRebuild()
    {
        l2d::TileSet tileSet;
        L2D_REQUIRE(tileSet.setTile('G', {{0, 0}, {8, 8}}));
        L2D_REQUIRE(tileSet.setTile('H', {{8, 0}, {8, 8}}));

        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setRenderChunkSize({2u, 1u});
        tileMap.setTileSet(tileSet);
        tileMap.loadFromLayout(scene, {"GGGG"});

        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 2u);
        L2D_REQUIRE(tileMap.setTile(0u, 1u, 'H'));
        L2D_REQUIRE(tileMap.lastUpdateStats().rebuiltRenderChunkCount == 1u);
        L2D_REQUIRE(!tileMap.lastUpdateStats().collisionGeometryRebuilt);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 2u);
        L2D_REQUIRE(tileMap.buildStats().texturedTileCount == 4u);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 0u);
    }

    void testEmptyLoadedMapCanCreateItsFirstRenderChunk()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setRenderChunkSize({2u, 2u});
        tileMap.loadFromLayout(scene, {"...."});

        L2D_REQUIRE(scene.gameObjectCount() == 0u);
        L2D_REQUIRE(tileMap.setStreamRegion({0u, 0u, 2u, 1u}));
        L2D_REQUIRE(tileMap.setTile(0u, 1u, 'P'));
        L2D_REQUIRE(tileMap.lastUpdateStats().rebuiltRenderChunkCount == 0u);
        L2D_REQUIRE(scene.gameObjectCount() == 0u);

        L2D_REQUIRE(tileMap.setTile(0u, 0u, '#'));
        L2D_REQUIRE(tileMap.buildStats().solidTileCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().renderChunkCount == 1u);
        L2D_REQUIRE(tileMap.buildStats().collisionRectangleCount == 1u);
        L2D_REQUIRE(tileMap.lastUpdateStats().rebuiltRenderChunkCount == 1u);
        L2D_REQUIRE(tileMap.lastUpdateStats().collisionGeometryRebuilt);
        L2D_REQUIRE(scene.activeGameObjectCount() == 2u);
    }

    void testLayeredTileDataPropertiesAndLegacyImport()
    {
        l2d::AsciiTileMapImporter importer;
        importer.mapCharacter('G', 1u);
        importer.mapCharacter('W', 2u);

        l2d::TileMapData data;
        L2D_REQUIRE(importer.import({"GW", "G"}, {16.f, 24.f}, data));
        L2D_REQUIRE_EQUAL(data.width(), 2u);
        L2D_REQUIRE_EQUAL(data.height(), 2u);
        L2D_REQUIRE_EQUAL(data.tileAt(0u, 1u, 1u), std::optional<l2d::TileId>(l2d::EmptyTile));

        l2d::TileDefinition ground = *data.definition(1u);
        ground.movementCost = 1.25f;
        ground.properties["biome"] = std::string("grass");
        L2D_REQUIRE(data.setDefinition(ground));
        l2d::TileDefinition wall = *data.definition(2u);
        wall.collision = l2d::TileCollisionKind::Solid;
        wall.navigable = false;
        L2D_REQUIRE(data.setDefinition(wall));

        l2d::TileMapLayer triggers;
        triggers.name = "Triggers";
        triggers.role = l2d::TileMapLayerRole::Trigger;
        triggers.tiles = {l2d::EmptyTile, l2d::EmptyTile, 2u, l2d::EmptyTile};
        triggers.properties["event"] = std::string("enter");
        L2D_REQUIRE(data.addLayer(triggers));
        L2D_REQUIRE(data.isValid());
        L2D_REQUIRE_EQUAL(data.layers().size(), 2u);
        L2D_REQUIRE(std::get<std::string>(data.definition(1u)->properties.at("biome")) == "grass");
        L2D_REQUIRE(!data.setTile(0u, 0u, 0u, 999u));
        L2D_REQUIRE(!data.removeDefinition(1u));

        const auto collision = l2d::TileMapColliderBuilder2D::build(data);
        L2D_REQUIRE_EQUAL(collision.size(), 1u);
        L2D_REQUIRE_EQUAL(collision[0].position, sf::Vector2f(16.f, 0.f));
        L2D_REQUIRE_EQUAL(collision[0].size, sf::Vector2f(16.f, 24.f));

        l2d::Scene scene;
        l2d::TileMap runtime;
        L2D_REQUIRE(runtime.loadFromData(scene, data));
        L2D_REQUIRE_EQUAL(runtime.data().layers().size(), 2u);
        L2D_REQUIRE_EQUAL(runtime.buildStats().renderedTileCount, 3u);
        L2D_REQUIRE_EQUAL(runtime.buildStats().collisionRectangleCount, 1u);
        L2D_REQUIRE(!runtime.setTile(0u, 0u, '#'));

        l2d::TileMapData resized = data;
        L2D_REQUIRE(resized.setDimensions(3u, 2u));
        L2D_REQUIRE_EQUAL(resized.tileAt(0u, 1u, 0u), std::optional<l2d::TileId>(1u));
        L2D_REQUIRE_EQUAL(resized.tileAt(0u, 1u, 1u), std::optional<l2d::TileId>(l2d::EmptyTile));

        l2d::TileMapData legacy;
        L2D_REQUIRE(l2d::AsciiTileMapImporter::importLegacy({"#P", "#"}, {8.f, 8.f}, '#', legacy));
        L2D_REQUIRE_EQUAL(legacy.width(), 2u);
        L2D_REQUIRE_EQUAL(legacy.height(), 2u);
        L2D_REQUIRE(legacy.definition(static_cast<l2d::TileId>('#') + 1u)->collision ==
                    l2d::TileCollisionKind::Solid);
    }

    void testTiledJsonImportsOrthogonalAndIsometricTransactionally()
    {
        const std::string tiled = R"json({
            "width":2,"height":2,"tilewidth":16,"tileheight":8,
            "orientation":"isometric","infinite":false,
            "properties":[{"name":"theme","type":"string","value":"village"}],
            "tilesets":[{"firstgid":1,"tilecount":2,"columns":2,
                "tilewidth":16,"tileheight":8,"image":"terrain.png",
                "tiles":[{"id":1,"properties":[
                    {"name":"solid","type":"bool","value":true},
                    {"name":"movementCost","type":"float","value":2.5}
                ]}]}],
            "layers":[
                {"type":"tilelayer","name":"Ground","width":2,"height":2,
                 "data":[1,2147483650,0,1]},
                {"type":"tilelayer","name":"Collision","width":2,"height":2,
                 "visible":false,"properties":[{"name":"role","value":"collision"}],
                 "data":[0,0,2,0]},
                {"type":"objectgroup","name":"Objects","objects":[
                    {"id":7,"name":"Spawn","class":"PlayerSpawn","x":12,"y":20,
                     "width":4,"height":6,"properties":[{"name":"team","value":"blue"}]}
                ]}
            ]
        })json";

        l2d::TileMapData data;
        std::stringstream input(tiled);
        L2D_REQUIRE(l2d::TiledJsonImporter::load(input, data));
        L2D_REQUIRE(data.orientation() == l2d::TileMapOrientation::Isometric);
        L2D_REQUIRE_EQUAL(data.layers().size(), 3u);
        L2D_REQUIRE_EQUAL(data.objects().size(), 1u);
        L2D_REQUIRE(data.objects()[0].type == "PlayerSpawn");
        L2D_REQUIRE(l2d::hasFlag(data.layers()[0].flipFlags[1], l2d::TileFlipFlags::Horizontal));
        L2D_REQUIRE_APPROX(data.definition(2u)->movementCost, 2.5f, 0.0001f);

        l2d::Scene scene;
        l2d::TileMap runtime;
        L2D_REQUIRE(!runtime.loadFromData(scene, data));
        L2D_REQUIRE_EQUAL(scene.gameObjectCount(), 0u);

        l2d::AssetManager assets;
        L2D_REQUIRE(assets.storeTexture("terrain.png",
                                        l2d::TextureHandle(std::make_shared<sf::Texture>())));
        L2D_REQUIRE(runtime.loadFromData(scene, data, assets));
        L2D_REQUIRE_EQUAL(runtime.buildStats().texturedTileCount, 3u);
        L2D_REQUIRE_EQUAL(runtime.buildStats().collisionRectangleCount, 2u);

        l2d::TileMapData unchanged = data;
        std::stringstream invalid(R"({"width":999999999999,"height":2})");
        L2D_REQUIRE(!l2d::TiledJsonImporter::load(invalid, data));
        L2D_REQUIRE_EQUAL(data.width(), unchanged.width());
        L2D_REQUIRE_EQUAL(data.layers().size(), unchanged.layers().size());

        std::stringstream wrongTypes(
            R"({"width":{},"height":2,"tilewidth":16,"tileheight":16,"orientation":"orthogonal"})");
        L2D_REQUIRE(!l2d::TiledJsonImporter::load(wrongTypes, data));
        L2D_REQUIRE_EQUAL(data.width(), unchanged.width());
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
    runTest("animated tilesets select frames transactionally",
            testAnimatedTileDefinitionsSelectFramesTransactionally, failures);
    runTest("tilemap edits one chunk and streams regions",
            testTileMapEditsOneChunkAndStreamsResidentRegions, failures);
    runTest("mapped tile edits avoid collision rebuilds", testMappedTileEditAvoidsCollisionRebuild,
            failures);
    runTest("empty loaded tilemaps create their first chunk",
            testEmptyLoadedMapCanCreateItsFirstRenderChunk, failures);
    runTest("layered tile data keeps properties and legacy behavior",
            testLayeredTileDataPropertiesAndLegacyImport, failures);
    runTest("Tiled JSON imports isometric maps transactionally",
            testTiledJsonImportsOrthogonalAndIsometricTransactionally, failures);

    if (failures != 0)
    {
        std::cerr << failures << " tilemap test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D tilemap tests passed.\n";
    return 0;
}

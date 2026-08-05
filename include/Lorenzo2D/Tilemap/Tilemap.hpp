#pragma once

#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace sf
{
    class View;
}

namespace l2d
{
    class Scene;

    // Describes the currently loaded geometry, or zeros when unloaded.
    struct TileMapBuildStats
    {
        std::size_t solidTileCount = 0;
        std::size_t renderedTileCount = 0;
        std::size_t texturedTileCount = 0;
        std::size_t renderChunkCount = 0;
        std::size_t collisionRectangleCount = 0;
    };

    // Describes the chunk work submitted for a view. A tile map batches each
    // non-empty chunk into one draw call and skips chunks outside the view.
    struct TileMapRenderStats
    {
        std::size_t chunkCount = 0;
        std::size_t visibleChunkCount = 0;
        std::size_t culledChunkCount = 0;
        std::size_t solidTileCount = 0;
        std::size_t submittedTileCount = 0;
        std::size_t submittedVertexCount = 0;
        std::size_t drawCallCount = 0;
    };

    class TileMap
    {
      public:
        using Layout = std::vector<std::string>;

      public:
        TileMap();
        ~TileMap();

        TileMap(const TileMap&) = delete;
        TileMap& operator=(const TileMap&) = delete;
        TileMap(TileMap&& other) noexcept;
        TileMap& operator=(TileMap&& other) noexcept;

        // Configuration changes are applied by the next successful load.
        // Existing geometry keeps the size used when it was loaded. Invalid
        // dimensions become one; positive dimensions below 0.0001 are clamped.
        void setTileSize(sf::Vector2f tileSize);
        // Returns the configuration for the next load.
        const sf::Vector2f& tileSize() const;
        // Returns the size used by the current layout, or zero when unloaded.
        const sf::Vector2f& loadedTileSize() const;

        // Render chunks are measured in tiles. Configuration changes apply to
        // the next successful load and zero dimensions are sanitized to one.
        void setRenderChunkSize(sf::Vector2u chunkSize);
        const sf::Vector2u& renderChunkSize() const;
        // Returns the chunk size used by the current layout, or zero when
        // unloaded.
        const sf::Vector2u& loadedRenderChunkSize() const;

        // Configuration changes are applied by the next successful load.
        // Existing render geometry keeps the color used when it was loaded.
        void setSolidTileColor(sf::Color color);
        // Returns the configuration for the next load.
        sf::Color solidTileColor() const;

        // Atlas mappings are snapshot with the next successful load. Mapped
        // characters render with texture coordinates; an unmapped solid cell
        // retains the configured flat-color fallback.
        void setTileSet(TileSet tileSet);
        const TileSet& tileSet() const;
        const TileSet& loadedTileSet() const;

        void loadFromLayout(Scene& scene, const Layout& layout, char solidChar = '#',
                            const std::string& objectPrefix = "Tile");

        // Queues generated render and collision objects for destruction in
        // their owning scenes. Expired handles are ignored, so this is safe
        // and idempotent.
        void unload();

        bool loadFromFile(Scene& scene, const std::string& filepath, char solidChar = '#',
                          const std::string& objectPrefix = "Tile");

        bool findFirstTilePosition(char tileChar, sf::Vector2f& outPosition,
                                   bool centered = true) const;

        std::vector<sf::Vector2f> findTilePositions(char tileChar, bool centered = true) const;

        const Layout& layout() const;
        const sf::Vector2f& worldSize() const;

        const TileMapBuildStats& buildStats() const;

        // Computes culling telemetry without drawing or creating a window.
        TileMapRenderStats renderStatsForView(const sf::View& view) const;

        // Returns telemetry from the most recent actual render. It is empty
        // until the current map has been rendered at least once.
        TileMapRenderStats lastRenderStats() const;

      private:
        Layout readLayoutFromFile(const std::string& filepath) const;
        void queueGeneratedObjectsForDestruction(
            const std::vector<GameObjectHandle>& generatedObjects) const;

      private:
        sf::Vector2f m_tileSize;
        sf::Vector2f m_loadedTileSize;
        sf::Vector2u m_renderChunkSize;
        sf::Vector2u m_loadedRenderChunkSize;
        sf::Color m_solidTileColor;
        TileSet m_tileSet;
        TileSet m_loadedTileSet;
        sf::Vector2f m_worldSize;
        TileMapBuildStats m_buildStats;

        Layout m_layout;
        std::vector<GameObjectHandle> m_generatedObjects;
        GameObjectHandle m_renderObject;
    };
}

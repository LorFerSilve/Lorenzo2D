#pragma once

#include <Lorenzo2D/Scene/GameObjectHandle.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <vector>

namespace l2d
{
    class Scene;

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
        // Existing generated tiles keep the size used when they were loaded.
        void setTileSize(sf::Vector2f tileSize);
        // Returns the configuration for the next load.
        const sf::Vector2f& tileSize() const;
        // Returns the size used by the current layout, or zero when unloaded.
        const sf::Vector2f& loadedTileSize() const;

        // Configuration changes are applied by the next successful load.
        // Existing generated tiles keep the color used when they were loaded.
        void setSolidTileColor(sf::Color color);
        // Returns the configuration for the next load.
        sf::Color solidTileColor() const;

        void loadFromLayout(
            Scene& scene,
            const Layout& layout,
            char solidChar = '#',
            const std::string& objectPrefix = "Tile"
        );

        // Queues generated tiles for destruction in their owning scenes. The
        // scenes remove them when they next process their destruction queues.
        // Expired scene/object handles are ignored, so this is safe and idempotent.
        void unload();

        bool loadFromFile(
            Scene& scene,
            const std::string& filepath,
            char solidChar = '#',
            const std::string& objectPrefix = "Tile"
        );

        bool findFirstTilePosition(
            char tileChar,
            sf::Vector2f& outPosition,
            bool centered = true
        ) const;

        std::vector<sf::Vector2f> findTilePositions(
            char tileChar,
            bool centered = true
        ) const;

        const Layout& layout() const;
        const sf::Vector2f& worldSize() const;

    private:
        Layout readLayoutFromFile(const std::string& filepath) const;
        void queueGeneratedTilesForDestruction(
            const std::vector<GameObjectHandle>& generatedTiles
        ) const;

    private:
        sf::Vector2f m_tileSize;
        sf::Vector2f m_loadedTileSize;
        sf::Color m_solidTileColor;
        sf::Vector2f m_worldSize;

        Layout m_layout;
        std::vector<GameObjectHandle> m_generatedTiles;
    };
}

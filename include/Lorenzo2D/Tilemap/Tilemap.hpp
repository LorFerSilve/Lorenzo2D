#pragma once

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

        void setTileSize(sf::Vector2f tileSize);
        const sf::Vector2f& tileSize() const;

        void setSolidTileColor(sf::Color color);
        sf::Color solidTileColor() const;

        void loadFromLayout(
            Scene& scene,
            const Layout& layout,
            char solidChar = '#',
            const std::string& objectPrefix = "Tile"
        );

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

    private:
        sf::Vector2f m_tileSize;
        sf::Color m_solidTileColor;
        sf::Vector2f m_worldSize;

        Layout m_layout;
    };
}
#include <Lorenzo2D/Tilemap/TileMap.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>

namespace l2d
{
    TileMap::TileMap()
        : m_tileSize(40.f, 40.f),
        m_solidTileColor(sf::Color::White),
        m_worldSize(0.f, 0.f)
    {
    }

    void TileMap::setTileSize(sf::Vector2f tileSize)
    {
        if (tileSize.x <= 0.f)
            tileSize.x = 1.f;

        if (tileSize.y <= 0.f)
            tileSize.y = 1.f;

        m_tileSize = tileSize;
    }

    const sf::Vector2f& TileMap::tileSize() const
    {
        return m_tileSize;
    }

    void TileMap::setSolidTileColor(sf::Color color)
    {
        m_solidTileColor = color;
    }

    sf::Color TileMap::solidTileColor() const
    {
        return m_solidTileColor;
    }

    void TileMap::loadFromLayout(
        Scene& scene,
        const Layout& layout,
        char solidChar,
        const std::string& objectPrefix
    )
    {
        m_layout = layout;

        std::size_t maxColumns = 0;

        for (const std::string& row : m_layout)
        {
            maxColumns = std::max(maxColumns, row.size());
        }

        m_worldSize =
        {
            static_cast<float>(maxColumns) * m_tileSize.x,
            static_cast<float>(m_layout.size()) * m_tileSize.y
        };

        int tileIndex = 0;

        for (std::size_t row = 0; row < m_layout.size(); ++row)
        {
            for (std::size_t column = 0; column < m_layout[row].size(); ++column)
            {
                if (m_layout[row][column] != solidChar)
                    continue;

                const sf::Vector2f position =
                {
                    static_cast<float>(column) * m_tileSize.x,
                    static_cast<float>(row) * m_tileSize.y
                };

                GameObject& tile = scene.createGameObject(
                    objectPrefix + "_" + std::to_string(tileIndex)
                );

                tile.transform.setPosition(position);

                tile.addComponent<RectangleRenderer>(
                    m_tileSize,
                    m_solidTileColor
                );

                tile.addComponent<BoxCollider2D>(
                    m_tileSize
                );

                tileIndex++;
            }
        }
    }

    bool TileMap::loadFromFile(
        Scene& scene,
        const std::string& filepath,
        char solidChar,
        const std::string& objectPrefix
    )
    {
        const Layout fileLayout = readLayoutFromFile(filepath);

        if (fileLayout.empty())
            return false;

        loadFromLayout(scene, fileLayout, solidChar, objectPrefix);
        return true;
    }

    bool TileMap::findFirstTilePosition(
        char tileChar,
        sf::Vector2f& outPosition,
        bool centered
    ) const
    {
        for (std::size_t row = 0; row < m_layout.size(); ++row)
        {
            for (std::size_t column = 0; column < m_layout[row].size(); ++column)
            {
                if (m_layout[row][column] != tileChar)
                    continue;

                outPosition =
                {
                    static_cast<float>(column) * m_tileSize.x,
                    static_cast<float>(row) * m_tileSize.y
                };

                if (centered)
                {
                    outPosition += m_tileSize * 0.5f;
                }

                return true;
            }
        }

        return false;
    }

    std::vector<sf::Vector2f> TileMap::findTilePositions(
        char tileChar,
        bool centered
    ) const
    {
        std::vector<sf::Vector2f> positions;

        for (std::size_t row = 0; row < m_layout.size(); ++row)
        {
            for (std::size_t column = 0; column < m_layout[row].size(); ++column)
            {
                if (m_layout[row][column] != tileChar)
                    continue;

                sf::Vector2f position =
                {
                    static_cast<float>(column) * m_tileSize.x,
                    static_cast<float>(row) * m_tileSize.y
                };

                if (centered)
                {
                    position += m_tileSize * 0.5f;
                }

                positions.push_back(position);
            }
        }

        return positions;
    }

    const TileMap::Layout& TileMap::layout() const
    {
        return m_layout;
    }

    const sf::Vector2f& TileMap::worldSize() const
    {
        return m_worldSize;
    }

    TileMap::Layout TileMap::readLayoutFromFile(const std::string& filepath) const
    {
        std::ifstream file(filepath);

        if (!file.is_open())
            return {};

        Layout layout;
        std::string line;

        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (!line.empty())
            {
                layout.push_back(line);
            }
        }

        return layout;
    }
}
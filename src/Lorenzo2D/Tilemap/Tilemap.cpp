#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>

namespace l2d
{
    TileMap::TileMap()
        : m_tileSize(40.f, 40.f),
        m_loadedTileSize(0.f, 0.f),
        m_solidTileColor(sf::Color::White),
        m_worldSize(0.f, 0.f)
    {
    }

    TileMap::~TileMap()
    {
        unload();
    }

    TileMap::TileMap(TileMap&& other) noexcept
        : m_tileSize(other.m_tileSize),
        m_loadedTileSize(other.m_loadedTileSize),
        m_solidTileColor(other.m_solidTileColor),
        m_worldSize(other.m_worldSize),
        m_layout(std::move(other.m_layout)),
        m_generatedTiles(std::move(other.m_generatedTiles))
    {
        other.m_loadedTileSize = { 0.f, 0.f };
        other.m_worldSize = { 0.f, 0.f };
        other.m_layout.clear();
        other.m_generatedTiles.clear();
    }

    TileMap& TileMap::operator=(TileMap&& other) noexcept
    {
        if (this == &other)
            return *this;

        unload();

        m_tileSize = other.m_tileSize;
        m_loadedTileSize = other.m_loadedTileSize;
        m_solidTileColor = other.m_solidTileColor;
        m_worldSize = other.m_worldSize;
        m_layout = std::move(other.m_layout);
        m_generatedTiles = std::move(other.m_generatedTiles);

        other.m_loadedTileSize = { 0.f, 0.f };
        other.m_worldSize = { 0.f, 0.f };
        other.m_layout.clear();
        other.m_generatedTiles.clear();

        return *this;
    }

    void TileMap::setTileSize(sf::Vector2f tileSize)
    {
        if (!std::isfinite(tileSize.x) || tileSize.x <= 0.f)
            tileSize.x = 1.f;

        if (!std::isfinite(tileSize.y) || tileSize.y <= 0.f)
            tileSize.y = 1.f;

        m_tileSize = tileSize;
    }

    const sf::Vector2f& TileMap::tileSize() const
    {
        return m_tileSize;
    }

    const sf::Vector2f& TileMap::loadedTileSize() const
    {
        return m_loadedTileSize;
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
        Layout newLayout = layout;
        const sf::Vector2f newLoadedTileSize = m_tileSize;

        std::size_t maxColumns = 0;
        std::size_t solidTileCount = 0;

        for (const std::string& row : newLayout)
        {
            maxColumns = std::max(maxColumns, row.size());
            solidTileCount += static_cast<std::size_t>(
                std::count(row.begin(), row.end(), solidChar)
            );
        }

        const sf::Vector2f newWorldSize =
        {
            static_cast<float>(maxColumns) * newLoadedTileSize.x,
            static_cast<float>(newLayout.size()) * newLoadedTileSize.y
        };

        std::vector<GameObjectHandle> newGeneratedTiles;
        newGeneratedTiles.reserve(solidTileCount);

        std::size_t tileIndex = 0;

        try
        {
            for (std::size_t row = 0; row < newLayout.size(); ++row)
            {
                for (std::size_t column = 0; column < newLayout[row].size(); ++column)
                {
                    if (newLayout[row][column] != solidChar)
                        continue;

                    const sf::Vector2f position =
                    {
                        static_cast<float>(column) * newLoadedTileSize.x,
                        static_cast<float>(row) * newLoadedTileSize.y
                    };

                    GameObject& tile = scene.createGameObject(
                        objectPrefix + "_" + std::to_string(tileIndex)
                    );

                    newGeneratedTiles.push_back(scene.createHandle(tile));

                    tile.transform.setPosition(position);

                    tile.addComponent<RectangleRenderer>(
                        newLoadedTileSize,
                        m_solidTileColor
                    );

                    tile.addComponent<BoxCollider2D>(
                        newLoadedTileSize
                    );

                    tileIndex++;
                }
            }
        }
        catch (...)
        {
            queueGeneratedTilesForDestruction(newGeneratedTiles);
            throw;
        }

        queueGeneratedTilesForDestruction(m_generatedTiles);

        m_layout = std::move(newLayout);
        m_loadedTileSize = newLoadedTileSize;
        m_worldSize = newWorldSize;
        m_generatedTiles = std::move(newGeneratedTiles);
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

    void TileMap::unload()
    {
        queueGeneratedTilesForDestruction(m_generatedTiles);

        m_generatedTiles.clear();
        m_layout.clear();
        m_loadedTileSize = { 0.f, 0.f };
        m_worldSize = { 0.f, 0.f };
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
                    static_cast<float>(column) * m_loadedTileSize.x,
                    static_cast<float>(row) * m_loadedTileSize.y
                };

                if (centered)
                {
                    outPosition += m_loadedTileSize * 0.5f;
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
                    static_cast<float>(column) * m_loadedTileSize.x,
                    static_cast<float>(row) * m_loadedTileSize.y
                };

                if (centered)
                {
                    position += m_loadedTileSize * 0.5f;
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

            layout.push_back(line);
        }

        if (file.bad())
            return {};

        return layout;
    }

    void TileMap::queueGeneratedTilesForDestruction(
        const std::vector<GameObjectHandle>& generatedTiles
    ) const
    {
        for (const GameObjectHandle& handle : generatedTiles)
        {
            Scene* scene = handle.scene();
            GameObject* tile = handle.get();

            if (scene != nullptr && tile != nullptr)
            {
                scene->destroyGameObject(*tile);
            }
        }
    }
}

#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <cstdint>
#include <limits>
#include <utility>

namespace l2d
{
    bool TileSet::setTexture(TextureHandle texture)
    {
        if (!texture) return false;

        m_texture = std::move(texture);
        return true;
    }

    void TileSet::clearTexture()
    {
        m_texture.reset();
    }

    TextureHandle TileSet::texture() const
    {
        return m_texture;
    }

    bool TileSet::setTile(char tile, sf::IntRect textureRect)
    {
        const int maximum = std::numeric_limits<int>::max();

        if (textureRect.position.x < 0 || textureRect.position.y < 0 || textureRect.size.x <= 0 ||
            textureRect.size.y <= 0 || textureRect.position.x > maximum - textureRect.size.x ||
            textureRect.position.y > maximum - textureRect.size.y)
        {
            return false;
        }

        m_tiles.insert_or_assign(tile, textureRect);
        return true;
    }

    bool TileSet::setTileFromGrid(char tile, sf::Vector2u atlasCell, sf::Vector2u tilePixelSize)
    {
        if (tilePixelSize.x == 0u || tilePixelSize.y == 0u) return false;

        const std::uint64_t left = static_cast<std::uint64_t>(atlasCell.x) * tilePixelSize.x;
        const std::uint64_t top = static_cast<std::uint64_t>(atlasCell.y) * tilePixelSize.y;
        const std::uint64_t maximum = static_cast<std::uint64_t>(std::numeric_limits<int>::max());

        if (left > maximum || top > maximum || tilePixelSize.x > maximum ||
            tilePixelSize.y > maximum)
        {
            return false;
        }

        return setTile(tile,
                       {{static_cast<int>(left), static_cast<int>(top)},
                        {static_cast<int>(tilePixelSize.x), static_cast<int>(tilePixelSize.y)}});
    }

    bool TileSet::removeTile(char tile)
    {
        return m_tiles.erase(tile) > 0u;
    }

    void TileSet::clearTiles()
    {
        m_tiles.clear();
    }

    bool TileSet::contains(char tile) const
    {
        return m_tiles.find(tile) != m_tiles.end();
    }

    std::optional<sf::IntRect> TileSet::textureRect(char tile) const
    {
        const auto iterator = m_tiles.find(tile);
        return iterator == m_tiles.end() ? std::nullopt
                                         : std::optional<sf::IntRect>(iterator->second);
    }

    std::size_t TileSet::tileCount() const
    {
        return m_tiles.size();
    }
}

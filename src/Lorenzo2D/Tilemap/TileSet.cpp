#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        bool isValidTextureRect(const sf::IntRect& textureRect)
        {
            const int maximum = std::numeric_limits<int>::max();
            return textureRect.position.x >= 0 && textureRect.position.y >= 0 &&
                   textureRect.size.x > 0 && textureRect.size.y > 0 &&
                   textureRect.position.x <= maximum - textureRect.size.x &&
                   textureRect.position.y <= maximum - textureRect.size.y;
        }
    }

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
        if (!isValidTextureRect(textureRect)) return false;

        m_tiles.insert_or_assign(tile, textureRect);
        m_animations.erase(tile);
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

    bool TileSet::setAnimatedTile(char tile, std::vector<TileAnimationFrame> frames)
    {
        if (frames.empty()) return false;

        float totalDuration = 0.f;

        for (const TileAnimationFrame& frame : frames)
        {
            if (!isValidTextureRect(frame.textureRect) || !std::isfinite(frame.duration) ||
                frame.duration <= 0.f)
            {
                return false;
            }

            totalDuration += frame.duration;

            if (!std::isfinite(totalDuration)) return false;
        }

        auto updatedTiles = m_tiles;
        auto updatedAnimations = m_animations;
        updatedTiles.insert_or_assign(tile, frames.front().textureRect);
        updatedAnimations.insert_or_assign(tile, std::move(frames));
        m_tiles.swap(updatedTiles);
        m_animations.swap(updatedAnimations);
        return true;
    }

    bool TileSet::removeTile(char tile)
    {
        m_animations.erase(tile);
        return m_tiles.erase(tile) > 0u;
    }

    void TileSet::clearTiles()
    {
        m_tiles.clear();
        m_animations.clear();
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

    std::optional<sf::IntRect> TileSet::textureRect(char tile, float elapsedSeconds) const
    {
        const auto iterator = m_animations.find(tile);

        if (iterator == m_animations.end() || !std::isfinite(elapsedSeconds))
        {
            return textureRect(tile);
        }

        float duration = 0.f;

        for (const TileAnimationFrame& frame : iterator->second)
        {
            duration += frame.duration;
        }

        float time = std::fmod(std::max(0.f, elapsedSeconds), duration);

        for (const TileAnimationFrame& frame : iterator->second)
        {
            if (time < frame.duration) return frame.textureRect;
            time -= frame.duration;
        }

        return iterator->second.back().textureRect;
    }

    const std::vector<TileAnimationFrame>* TileSet::animation(char tile) const
    {
        const auto iterator = m_animations.find(tile);
        return iterator == m_animations.end() ? nullptr : &iterator->second;
    }

    bool TileSet::isAnimated(char tile) const
    {
        return m_animations.find(tile) != m_animations.end();
    }

    std::size_t TileSet::tileCount() const
    {
        return m_tiles.size();
    }

    std::size_t TileSet::animatedTileCount() const
    {
        return m_animations.size();
    }
}

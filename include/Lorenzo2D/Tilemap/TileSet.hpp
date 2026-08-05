#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace l2d
{
    struct TileAnimationFrame
    {
        sf::IntRect textureRect;
        float duration = 0.f;
    };

    // Maps layout characters to texture-atlas rectangles while retaining the
    // texture lease used by tile-map render batches.
    class TileSet
    {
      public:
        bool setTexture(TextureHandle texture);
        void clearTexture();
        TextureHandle texture() const;

        bool setTile(char tile, sf::IntRect textureRect);
        bool setTileFromGrid(char tile, sf::Vector2u atlasCell, sf::Vector2u tilePixelSize);
        bool setAnimatedTile(char tile, std::vector<TileAnimationFrame> frames);
        bool removeTile(char tile);
        void clearTiles();

        bool contains(char tile) const;
        std::optional<sf::IntRect> textureRect(char tile) const;
        std::optional<sf::IntRect> textureRect(char tile, float elapsedSeconds) const;
        const std::vector<TileAnimationFrame>* animation(char tile) const;
        bool isAnimated(char tile) const;
        std::size_t tileCount() const;
        std::size_t animatedTileCount() const;

      private:
        TextureHandle m_texture;
        std::unordered_map<char, sf::IntRect> m_tiles;
        std::unordered_map<char, std::vector<TileAnimationFrame>> m_animations;
    };
}

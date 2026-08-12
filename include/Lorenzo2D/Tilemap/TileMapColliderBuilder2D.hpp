#pragma once

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

namespace l2d
{
    struct TileMapCollisionRectangle2D
    {
        std::size_t firstColumn = 0u;
        std::size_t firstRow = 0u;
        std::size_t columnCount = 0u;
        std::size_t rowCount = 0u;
        sf::Vector2f position;
        sf::Vector2f size;
    };

    class TileMapColliderBuilder2D
    {
      public:
        // Collision-role layers and definitions marked Solid are unioned before
        // deterministic greedy rectangle merging.
        static std::vector<TileMapCollisionRectangle2D> build(const TileMapData& data);
    };
}

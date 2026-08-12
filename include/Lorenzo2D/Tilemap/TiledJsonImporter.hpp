#pragma once

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <iosfwd>
#include <string>

namespace l2d
{
    class TiledJsonImporter
    {
      public:
        // Supports finite orthogonal/isometric maps with inline tilesets,
        // tile layers and object layers. Output changes only after full validation.
        static bool load(std::istream& input, TileMapData& output);
        static bool loadFromFile(const std::string& filepath, TileMapData& output);
    };
}

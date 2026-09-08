#pragma once

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <cstddef>
#include <iosfwd>
#include <string>

namespace l2d
{
    struct TiledJsonImportLimits
    {
        std::size_t maxInputBytes = 64u * 1024u * 1024u;
    };

    class TiledJsonImporter
    {
      public:
        // Supports finite orthogonal/isometric maps with inline tilesets,
        // tile layers and object layers. Output changes only after full validation.
        static bool load(std::istream& input, TileMapData& output);
        static bool load(std::istream& input, TileMapData& output, TiledJsonImportLimits limits);
        static bool loadFromFile(const std::string& filepath, TileMapData& output);
        static bool loadFromFile(const std::string& filepath, TileMapData& output,
                                 TiledJsonImportLimits limits);
    };
}

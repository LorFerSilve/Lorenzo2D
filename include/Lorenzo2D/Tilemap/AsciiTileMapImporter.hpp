#pragma once

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/System/Vector2.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace l2d
{
    class AsciiTileMapImporter
    {
      public:
        using Layout = std::vector<std::string>;

        void setEmptyCharacter(char character);
        char emptyCharacter() const;

        // EmptyTile is a valid mapping and behaves like the empty character.
        bool mapCharacter(char character, TileId tile);
        bool unmapCharacter(char character);
        void clearMappings();

        bool import(const Layout& layout, sf::Vector2f tileSize, TileMapData& output,
                    std::string layerName = "Ground",
                    TileMapLayerRole role = TileMapLayerRole::Ground) const;

        // Compatibility importer: every byte gets a deterministic ID and the
        // selected solid byte receives collision metadata.
        static bool importLegacy(const Layout& layout, sf::Vector2f tileSize, char solidCharacter,
                                 TileMapData& output);

      private:
        char m_emptyCharacter = '.';
        std::unordered_map<char, TileId> m_mappings;
    };
}

#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace l2d
{
    void AsciiTileMapImporter::setEmptyCharacter(char character)
    {
        m_emptyCharacter = character;
    }

    char AsciiTileMapImporter::emptyCharacter() const
    {
        return m_emptyCharacter;
    }

    bool AsciiTileMapImporter::mapCharacter(char character, TileId tile)
    {
        m_mappings.insert_or_assign(character, tile);
        return true;
    }

    bool AsciiTileMapImporter::unmapCharacter(char character)
    {
        return m_mappings.erase(character) != 0u;
    }

    void AsciiTileMapImporter::clearMappings()
    {
        m_mappings.clear();
    }

    bool AsciiTileMapImporter::import(const Layout& layout, sf::Vector2f tileSize,
                                      TileMapData& output, std::string layerName,
                                      TileMapLayerRole role) const
    {
        if (layout.empty()) return false;

        std::size_t width = 0u;
        for (const std::string& row : layout)
            width = std::max(width, row.size());
        if (width == 0u) return false;

        TileMapData parsed;
        if (!parsed.setDimensions(width, layout.size()) || !parsed.setTileSize(tileSize))
            return false;

        TileMapLayer layer;
        layer.name = std::move(layerName);
        layer.role = role;
        layer.tiles.resize(parsed.cellCount(), EmptyTile);

        std::unordered_map<TileId, bool> referenced;

        for (std::size_t row = 0; row < layout.size(); ++row)
        {
            for (std::size_t column = 0; column < layout[row].size(); ++column)
            {
                const char character = layout[row][column];
                if (character == m_emptyCharacter) continue;

                const auto mapping = m_mappings.find(character);
                if (mapping == m_mappings.end()) return false;

                layer.tiles[row * width + column] = mapping->second;
                if (mapping->second != EmptyTile) referenced.emplace(mapping->second, true);
            }
        }

        for (const auto& reference : referenced)
        {
            TileDefinition definition;
            definition.id = reference.first;
            if (!parsed.setDefinition(std::move(definition))) return false;
        }

        if (!parsed.addLayer(std::move(layer)) || !parsed.isValid()) return false;

        output = std::move(parsed);
        return true;
    }

    bool AsciiTileMapImporter::importLegacy(const Layout& layout, sf::Vector2f tileSize,
                                            char solidCharacter, TileMapData& output)
    {
        AsciiTileMapImporter importer;
        importer.setEmptyCharacter('\0');

        for (const std::string& row : layout)
        {
            for (const unsigned char character : row)
            {
                const TileId id = static_cast<TileId>(character) + 1u;
                importer.mapCharacter(static_cast<char>(character), id);
            }
        }

        if (!importer.import(layout, tileSize, output)) return false;

        TileDefinition solid;
        solid.id = static_cast<TileId>(static_cast<unsigned char>(solidCharacter)) + 1u;
        solid.collision = TileCollisionKind::Solid;
        return output.setDefinition(std::move(solid));
    }
}

#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool validPropertyMap(const PropertyMap& properties)
        {
            for (const auto& entry : properties)
            {
                if (entry.first.empty()) return false;

                if (const double* number = std::get_if<double>(&entry.second);
                    number != nullptr && !std::isfinite(*number))
                {
                    return false;
                }
            }

            return true;
        }

        bool validRect(sf::IntRect rectangle)
        {
            return rectangle.position.x >= 0 && rectangle.position.y >= 0 && rectangle.size.x > 0 &&
                   rectangle.size.y > 0 &&
                   rectangle.position.x <= std::numeric_limits<int>::max() - rectangle.size.x &&
                   rectangle.position.y <= std::numeric_limits<int>::max() - rectangle.size.y;
        }

        bool validRole(TileMapLayerRole role)
        {
            return role == TileMapLayerRole::Ground || role == TileMapLayerRole::Decoration ||
                   role == TileMapLayerRole::Collision || role == TileMapLayerRole::Trigger ||
                   role == TileMapLayerRole::Navigation || role == TileMapLayerRole::Object;
        }

        bool validFlipFlags(TileFlipFlags flags)
        {
            constexpr std::uint8_t validMask =
                static_cast<std::uint8_t>(TileFlipFlags::Horizontal) |
                static_cast<std::uint8_t>(TileFlipFlags::Vertical) |
                static_cast<std::uint8_t>(TileFlipFlags::Diagonal);
            return (static_cast<std::uint8_t>(flags) & ~validMask) == 0u;
        }
    }

    TileFlipFlags operator|(TileFlipFlags left, TileFlipFlags right)
    {
        return static_cast<TileFlipFlags>(static_cast<std::uint8_t>(left) |
                                          static_cast<std::uint8_t>(right));
    }

    bool hasFlag(TileFlipFlags value, TileFlipFlags flag)
    {
        return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0u;
    }

    bool isValidTileDefinition(const TileDefinition& definition)
    {
        if (definition.id == EmptyTile ||
            (definition.collision != TileCollisionKind::None &&
             definition.collision != TileCollisionKind::Solid) ||
            !std::isfinite(definition.movementCost) || definition.movementCost <= 0.f ||
            !validPropertyMap(definition.properties))
        {
            return false;
        }

        if ((!definition.texture.empty() && !validRect(definition.textureRect)) ||
            (definition.texture.empty() && !definition.animation.empty()))
            return false;

        float duration = 0.f;

        for (const TileAnimationFrame& frame : definition.animation)
        {
            if (!validRect(frame.textureRect) || !std::isfinite(frame.duration) ||
                frame.duration <= 0.f)
            {
                return false;
            }

            duration += frame.duration;
            if (!std::isfinite(duration)) return false;
        }

        return true;
    }

    bool TileMapData::setDimensions(std::size_t width, std::size_t height)
    {
        if (width == 0u || height == 0u || width > MaximumCellCount || height > MaximumCellCount ||
            width > MaximumCellCount / height)
        {
            return false;
        }

        const std::size_t nextCellCount = width * height;
        std::vector<TileMapLayer> updatedLayers = m_layers;

        for (TileMapLayer& layer : updatedLayers)
        {
            std::vector<TileId> tiles(nextCellCount, EmptyTile);
            const std::size_t copiedRows = std::min(m_height, height);
            const std::size_t copiedColumns = std::min(m_width, width);
            for (std::size_t row = 0u; row < copiedRows; ++row)
                for (std::size_t column = 0u; column < copiedColumns; ++column)
                    tiles[row * width + column] = layer.tiles[row * m_width + column];
            layer.tiles.swap(tiles);

            if (!layer.flipFlags.empty())
            {
                std::vector<TileFlipFlags> flags(nextCellCount, TileFlipFlags::None);
                for (std::size_t row = 0u; row < copiedRows; ++row)
                    for (std::size_t column = 0u; column < copiedColumns; ++column)
                        flags[row * width + column] = layer.flipFlags[row * m_width + column];
                layer.flipFlags.swap(flags);
            }
        }

        m_width = width;
        m_height = height;
        m_layers.swap(updatedLayers);
        return true;
    }

    std::size_t TileMapData::width() const
    {
        return m_width;
    }

    std::size_t TileMapData::height() const
    {
        return m_height;
    }

    std::size_t TileMapData::cellCount() const
    {
        return m_width * m_height;
    }

    bool TileMapData::setTileSize(sf::Vector2f tileSize)
    {
        if (!finite(tileSize) || tileSize.x <= 0.f || tileSize.y <= 0.f) return false;

        m_tileSize = tileSize;
        return true;
    }

    sf::Vector2f TileMapData::tileSize() const
    {
        return m_tileSize;
    }

    void TileMapData::setOrientation(TileMapOrientation orientation)
    {
        if (orientation == TileMapOrientation::Orthogonal ||
            orientation == TileMapOrientation::Isometric)
        {
            m_orientation = orientation;
        }
    }

    TileMapOrientation TileMapData::orientation() const
    {
        return m_orientation;
    }

    bool TileMapData::setDefinition(TileDefinition definition)
    {
        if (!isValidTileDefinition(definition)) return false;

        m_definitions.insert_or_assign(definition.id, std::move(definition));
        return true;
    }

    bool TileMapData::removeDefinition(TileId id)
    {
        for (const TileMapLayer& layer : m_layers)
            if (std::find(layer.tiles.begin(), layer.tiles.end(), id) != layer.tiles.end())
                return false;
        return m_definitions.erase(id) != 0u;
    }

    const TileDefinition* TileMapData::definition(TileId id) const
    {
        const auto iterator = m_definitions.find(id);
        return iterator == m_definitions.end() ? nullptr : &iterator->second;
    }

    const std::unordered_map<TileId, TileDefinition>& TileMapData::definitions() const
    {
        return m_definitions;
    }

    bool TileMapData::addLayer(TileMapLayer layer)
    {
        if (m_layers.size() >= MaximumLayerCount || !validLayer(layer)) return false;

        m_layers.push_back(std::move(layer));
        return true;
    }

    bool TileMapData::replaceLayer(std::size_t index, TileMapLayer layer)
    {
        if (index >= m_layers.size() || !validLayer(layer)) return false;

        m_layers[index] = std::move(layer);
        return true;
    }

    TileMapLayer* TileMapData::layer(std::size_t index)
    {
        return index < m_layers.size() ? &m_layers[index] : nullptr;
    }

    const TileMapLayer* TileMapData::layer(std::size_t index) const
    {
        return index < m_layers.size() ? &m_layers[index] : nullptr;
    }

    std::vector<TileMapLayer>& TileMapData::layers()
    {
        return m_layers;
    }

    const std::vector<TileMapLayer>& TileMapData::layers() const
    {
        return m_layers;
    }

    bool TileMapData::addObject(TileMapObject object)
    {
        if (m_objects.size() >= MaximumObjectCount || !finite(object.position) ||
            !finite(object.size) || object.size.x < 0.f || object.size.y < 0.f ||
            !std::isfinite(object.rotation) || !validPropertyMap(object.properties))
        {
            return false;
        }

        m_objects.push_back(std::move(object));
        return true;
    }

    const std::vector<TileMapObject>& TileMapData::objects() const
    {
        return m_objects;
    }

    std::optional<TileId> TileMapData::tileAt(std::size_t layerIndex, std::size_t row,
                                              std::size_t column) const
    {
        const std::optional<std::size_t> index = indexOf(row, column);

        if (!index || layerIndex >= m_layers.size()) return std::nullopt;

        return m_layers[layerIndex].tiles[*index];
    }

    bool TileMapData::setTile(std::size_t layerIndex, std::size_t row, std::size_t column,
                              TileId tile, TileFlipFlags flags)
    {
        const std::optional<std::size_t> index = indexOf(row, column);

        if (!index || layerIndex >= m_layers.size()) return false;

        TileMapLayer& target = m_layers[layerIndex];
        if (tile != EmptyTile && definition(tile) == nullptr) return false;
        target.tiles[*index] = tile;

        if (flags != TileFlipFlags::None && target.flipFlags.empty())
            target.flipFlags.resize(cellCount(), TileFlipFlags::None);
        if (!target.flipFlags.empty()) target.flipFlags[*index] = flags;
        return true;
    }

    PropertyMap& TileMapData::properties()
    {
        return m_properties;
    }

    const PropertyMap& TileMapData::properties() const
    {
        return m_properties;
    }

    bool TileMapData::isValid() const
    {
        if (m_width == 0u || m_height == 0u || cellCount() > MaximumCellCount ||
            !finite(m_tileSize) || m_tileSize.x <= 0.f || m_tileSize.y <= 0.f ||
            (m_orientation != TileMapOrientation::Orthogonal &&
             m_orientation != TileMapOrientation::Isometric) ||
            m_layers.size() > MaximumLayerCount || m_objects.size() > MaximumObjectCount ||
            !validPropertyMap(m_properties))
        {
            return false;
        }

        for (const auto& definition : m_definitions)
            if (definition.first != definition.second.id ||
                !isValidTileDefinition(definition.second))
                return false;
        for (const TileMapLayer& layer : m_layers)
        {
            if (!validLayer(layer)) return false;
            for (const TileId tile : layer.tiles)
                if (tile != EmptyTile && definition(tile) == nullptr) return false;
        }
        for (const TileMapObject& object : m_objects)
            if (!finite(object.position) || !finite(object.size) || object.size.x < 0.f ||
                object.size.y < 0.f || !std::isfinite(object.rotation) ||
                !validPropertyMap(object.properties))
                return false;

        return true;
    }

    void TileMapData::clear()
    {
        *this = TileMapData{};
    }

    bool TileMapData::validLayer(const TileMapLayer& layer) const
    {
        return validRole(layer.role) && layer.tiles.size() == cellCount() &&
               (layer.flipFlags.empty() || layer.flipFlags.size() == cellCount()) &&
               std::isfinite(layer.opacity) && layer.opacity >= 0.f && layer.opacity <= 1.f &&
               validPropertyMap(layer.properties) &&
               std::all_of(layer.flipFlags.begin(), layer.flipFlags.end(), validFlipFlags);
    }

    std::optional<std::size_t> TileMapData::indexOf(std::size_t row, std::size_t column) const
    {
        if (row >= m_height || column >= m_width) return std::nullopt;

        return row * m_width + column;
    }
}

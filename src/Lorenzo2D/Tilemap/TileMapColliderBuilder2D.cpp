#include <Lorenzo2D/Tilemap/TileMapColliderBuilder2D.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace l2d
{
    namespace
    {
        float extent(std::size_t cells, float tileSize)
        {
            const long double value = static_cast<long double>(cells) * tileSize;
            if (value > std::numeric_limits<float>::max())
                throw std::overflow_error("Tile-map collision extent is not representable.");
            return static_cast<float>(value);
        }
    }

    std::vector<TileMapCollisionRectangle2D> TileMapColliderBuilder2D::build(
        const TileMapData& data)
    {
        if (!data.isValid()) throw std::invalid_argument("Cannot build invalid tile-map data.");

        std::vector<bool> solid(data.cellCount(), false);

        for (const TileMapLayer& layer : data.layers())
        {
            if (layer.role == TileMapLayerRole::Trigger ||
                layer.role == TileMapLayerRole::Navigation ||
                layer.role == TileMapLayerRole::Object)
                continue;

            for (std::size_t index = 0; index < layer.tiles.size(); ++index)
            {
                const TileId tile = layer.tiles[index];
                if (tile == EmptyTile) continue;

                const TileDefinition* definition = data.definition(tile);
                if (layer.role == TileMapLayerRole::Collision ||
                    (definition != nullptr && definition->collision == TileCollisionKind::Solid))
                {
                    solid[index] = true;
                }
            }
        }

        std::vector<bool> consumed(data.cellCount(), false);
        std::vector<TileMapCollisionRectangle2D> result;

        for (std::size_t row = 0; row < data.height(); ++row)
        {
            for (std::size_t column = 0; column < data.width(); ++column)
            {
                const std::size_t first = row * data.width() + column;
                if (!solid[first] || consumed[first]) continue;

                std::size_t width = 0u;
                while (column + width < data.width() && solid[first + width] &&
                       !consumed[first + width])
                    ++width;

                std::size_t height = 1u;
                while (row + height < data.height())
                {
                    bool extend = true;
                    for (std::size_t offset = 0; offset < width; ++offset)
                    {
                        const std::size_t candidate =
                            (row + height) * data.width() + column + offset;
                        if (!solid[candidate] || consumed[candidate])
                        {
                            extend = false;
                            break;
                        }
                    }
                    if (!extend) break;
                    ++height;
                }

                for (std::size_t mergedRow = row; mergedRow < row + height; ++mergedRow)
                    for (std::size_t mergedColumn = column; mergedColumn < column + width;
                         ++mergedColumn)
                        consumed[mergedRow * data.width() + mergedColumn] = true;

                TileMapCollisionRectangle2D rectangle;
                rectangle.firstColumn = column;
                rectangle.firstRow = row;
                rectangle.columnCount = width;
                rectangle.rowCount = height;
                rectangle.position = {extent(column, data.tileSize().x),
                                      extent(row, data.tileSize().y)};
                rectangle.size = {extent(width, data.tileSize().x),
                                  extent(height, data.tileSize().y)};
                result.push_back(rectangle);
            }
        }

        return result;
    }
}

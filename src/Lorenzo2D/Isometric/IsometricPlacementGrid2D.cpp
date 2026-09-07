#include <Lorenzo2D/Isometric/IsometricPlacementGrid2D.hpp>

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

        bool representableExtent(float origin, unsigned int count, float dimension)
        {
            const long double end = static_cast<long double>(origin) +
                                    static_cast<long double>(count) *
                                        static_cast<long double>(dimension);
            const long double maximum =
                static_cast<long double>(std::numeric_limits<float>::max());
            return std::isfinite(end) && std::fabs(end) <= maximum;
        }
    }

    IsometricPlacementGrid2D::IsometricPlacementGrid2D()
    {
        (void)reset({});
    }

    IsometricPlacementGrid2D::IsometricPlacementGrid2D(IsometricPlacementGridConfig2D config)
    {
        (void)reset(config);
    }

    bool IsometricPlacementGrid2D::isValidConfig(
        const IsometricPlacementGridConfig2D& config)
    {
        if (config.size.x == 0u || config.size.y == 0u || !finite(config.cellSize) ||
            !finite(config.worldOrigin) || config.cellSize.x < MinimumCellDimension ||
            config.cellSize.y < MinimumCellDimension)
        {
            return false;
        }

        const std::size_t width = static_cast<std::size_t>(config.size.x);
        const std::size_t height = static_cast<std::size_t>(config.size.y);
        if (width > MaximumCellCount / height) return false;
        if (!representableExtent(config.worldOrigin.x, config.size.x, config.cellSize.x) ||
            !representableExtent(config.worldOrigin.y, config.size.y, config.cellSize.y))
        {
            return false;
        }

        return true;
    }

    bool IsometricPlacementGrid2D::reset(IsometricPlacementGridConfig2D config)
    {
        if (!isValidConfig(config)) return false;

        const std::size_t count = static_cast<std::size_t>(config.size.x) *
                                  static_cast<std::size_t>(config.size.y);
        std::vector<std::uint8_t> cells(count, 0u);
        m_config = config;
        m_cells = std::move(cells);
        return true;
    }

    bool IsometricPlacementGrid2D::resetFromTileMap(const TileMapData& data,
                                                    sf::Vector2f worldOrigin,
                                                    bool blockCollisionCells)
    {
        if (!data.isValid() || data.orientation() != TileMapOrientation::Isometric ||
            data.width() == 0u || data.height() == 0u ||
            data.width() > std::numeric_limits<unsigned int>::max() ||
            data.height() > std::numeric_limits<unsigned int>::max())
        {
            return false;
        }

        IsometricPlacementGridConfig2D config;
        config.size = {static_cast<unsigned int>(data.width()),
                       static_cast<unsigned int>(data.height())};
        config.cellSize = data.tileSize();
        config.worldOrigin = worldOrigin;

        IsometricPlacementGrid2D candidate;
        if (!candidate.reset(config)) return false;

        if (blockCollisionCells)
        {
            for (const TileMapLayer& layer : data.layers())
            {
                for (std::size_t index = 0u; index < layer.tiles.size(); ++index)
                {
                    const TileId tile = layer.tiles[index];
                    if (tile == EmptyTile) continue;

                    const TileDefinition* definition = data.definition(tile);
                    const bool blocked =
                        layer.role == TileMapLayerRole::Collision ||
                        (definition != nullptr &&
                         definition->collision == TileCollisionKind::Solid);
                    if (!blocked) continue;

                    candidate.m_cells[index] |= BlockedBit;
                }
            }
        }

        *this = std::move(candidate);
        return true;
    }

    const IsometricPlacementGridConfig2D& IsometricPlacementGrid2D::config() const
    {
        return m_config;
    }

    std::size_t IsometricPlacementGrid2D::cellCount() const
    {
        return m_cells.size();
    }

    bool IsometricPlacementGrid2D::contains(sf::Vector2u cell) const
    {
        return cell.x < m_config.size.x && cell.y < m_config.size.y;
    }

    std::optional<sf::Vector2u> IsometricPlacementGrid2D::worldToCell(
        sf::Vector2f worldPosition) const
    {
        if (!finite(worldPosition)) return std::nullopt;

        const double column =
            (static_cast<double>(worldPosition.x) - m_config.worldOrigin.x) /
            static_cast<double>(m_config.cellSize.x);
        const double row =
            (static_cast<double>(worldPosition.y) - m_config.worldOrigin.y) /
            static_cast<double>(m_config.cellSize.y);
        if (!std::isfinite(column) || !std::isfinite(row) || column < 0.0 || row < 0.0)
            return std::nullopt;

        const double flooredColumn = std::floor(column);
        const double flooredRow = std::floor(row);
        if (flooredColumn >= static_cast<double>(m_config.size.x) ||
            flooredRow >= static_cast<double>(m_config.size.y))
        {
            return std::nullopt;
        }

        return sf::Vector2u{static_cast<unsigned int>(flooredColumn),
                            static_cast<unsigned int>(flooredRow)};
    }

    std::optional<sf::Vector2u> IsometricPlacementGrid2D::pickCell(
        sf::Vector2f renderPosition, const CoordinateProjection2D& projection) const
    {
        if (!finite(renderPosition)) return std::nullopt;
        const sf::Vector2f worldPosition = projection.renderToWorld(renderPosition);
        return worldToCell(worldPosition);
    }

    std::optional<sf::Vector2f> IsometricPlacementGrid2D::cellWorldOrigin(
        sf::Vector2u cell) const
    {
        if (!contains(cell)) return std::nullopt;

        const double x = static_cast<double>(m_config.worldOrigin.x) +
                         static_cast<double>(cell.x) * m_config.cellSize.x;
        const double y = static_cast<double>(m_config.worldOrigin.y) +
                         static_cast<double>(cell.y) * m_config.cellSize.y;
        return sf::Vector2f{static_cast<float>(x), static_cast<float>(y)};
    }

    std::optional<sf::Vector2f> IsometricPlacementGrid2D::cellWorldCenter(
        sf::Vector2u cell) const
    {
        const std::optional<sf::Vector2f> origin = cellWorldOrigin(cell);
        if (!origin) return std::nullopt;
        return *origin + m_config.cellSize * 0.5f;
    }

    bool IsometricPlacementGrid2D::blocked(sf::Vector2u cell) const
    {
        const std::optional<std::size_t> index = indexOf(cell);
        return index && (m_cells[*index] & BlockedBit) != 0u;
    }

    bool IsometricPlacementGrid2D::occupied(sf::Vector2u cell) const
    {
        const std::optional<std::size_t> index = indexOf(cell);
        return index && (m_cells[*index] & OccupiedBit) != 0u;
    }

    bool IsometricPlacementGrid2D::available(sf::Vector2u cell) const
    {
        const std::optional<std::size_t> index = indexOf(cell);
        return index && m_cells[*index] == 0u;
    }

    bool IsometricPlacementGrid2D::setBlocked(sf::Vector2u cell, bool isBlocked)
    {
        const std::optional<std::size_t> index = indexOf(cell);
        if (!index) return false;

        if (isBlocked)
            m_cells[*index] |= BlockedBit;
        else
            m_cells[*index] &= static_cast<std::uint8_t>(~BlockedBit);
        return true;
    }

    bool IsometricPlacementGrid2D::setOccupied(sf::Vector2u cell, bool isOccupied)
    {
        const std::optional<std::size_t> index = indexOf(cell);
        if (!index) return false;
        if (isOccupied && (m_cells[*index] & BlockedBit) != 0u) return false;

        if (isOccupied)
            m_cells[*index] |= OccupiedBit;
        else
            m_cells[*index] &= static_cast<std::uint8_t>(~OccupiedBit);
        return true;
    }

    bool IsometricPlacementGrid2D::canPlace(sf::Vector2u firstCell,
                                            sf::Vector2u footprint) const
    {
        if (!validFootprint(firstCell, footprint)) return false;

        for (unsigned int y = 0u; y < footprint.y; ++y)
        {
            for (unsigned int x = 0u; x < footprint.x; ++x)
            {
                const sf::Vector2u cell{firstCell.x + x, firstCell.y + y};
                if (!available(cell)) return false;
            }
        }

        return true;
    }

    bool IsometricPlacementGrid2D::place(sf::Vector2u firstCell, sf::Vector2u footprint)
    {
        if (!canPlace(firstCell, footprint)) return false;

        for (unsigned int y = 0u; y < footprint.y; ++y)
            for (unsigned int x = 0u; x < footprint.x; ++x)
                (void)setOccupied({firstCell.x + x, firstCell.y + y}, true);
        return true;
    }

    bool IsometricPlacementGrid2D::remove(sf::Vector2u firstCell, sf::Vector2u footprint)
    {
        if (!validFootprint(firstCell, footprint)) return false;

        for (unsigned int y = 0u; y < footprint.y; ++y)
            for (unsigned int x = 0u; x < footprint.x; ++x)
                (void)setOccupied({firstCell.x + x, firstCell.y + y}, false);
        return true;
    }

    void IsometricPlacementGrid2D::clearPlacements()
    {
        for (std::uint8_t& cell : m_cells)
            cell &= static_cast<std::uint8_t>(~OccupiedBit);
    }

    std::optional<std::size_t> IsometricPlacementGrid2D::indexOf(sf::Vector2u cell) const
    {
        if (!contains(cell)) return std::nullopt;
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(m_config.size.x) +
               static_cast<std::size_t>(cell.x);
    }

    bool IsometricPlacementGrid2D::validFootprint(sf::Vector2u firstCell,
                                                  sf::Vector2u footprint) const
    {
        if (footprint.x == 0u || footprint.y == 0u || !contains(firstCell)) return false;

        return footprint.x <= m_config.size.x - firstCell.x &&
               footprint.y <= m_config.size.y - firstCell.y;
    }
}

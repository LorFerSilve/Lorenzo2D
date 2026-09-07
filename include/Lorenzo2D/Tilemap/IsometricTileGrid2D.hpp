#pragma once

#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>

namespace l2d
{
    // Bridges an isometric presentation back to the Cartesian tile grid used by
    // TileMap, physics and navigation. The visible-region result can be passed
    // directly to TileMap::setStreamRegion() for projected-view chunk culling.
    class IsometricTileGrid2D
    {
      public:
        IsometricTileGrid2D(std::size_t width, std::size_t height,
                            IsometricProjection2D projection)
            : m_width(width), m_height(height), m_projection(std::move(projection))
        {
            if (width == 0u || height == 0u)
                throw std::invalid_argument("Isometric tile grids must have non-zero dimensions.");
        }

        explicit IsometricTileGrid2D(const TileMapData& data, sf::Vector2f renderCellSize,
                                     sf::Vector2f renderOrigin = {0.f, 0.f})
            : IsometricTileGrid2D(
                  data.width(), data.height(),
                  IsometricProjection2D(data.tileSize(), renderCellSize, renderOrigin))
        {
            if (!data.isValid() || data.orientation() != TileMapOrientation::Isometric)
                throw std::invalid_argument("IsometricTileGrid2D requires valid isometric tile data.");
        }

        std::size_t width() const
        {
            return m_width;
        }

        std::size_t height() const
        {
            return m_height;
        }

        const IsometricProjection2D& projection() const
        {
            return m_projection;
        }

        bool contains(TileMapCell cell) const
        {
            return cell.column < m_width && cell.row < m_height;
        }

        std::optional<TileMapCell> pick(sf::Vector2f renderPosition) const
        {
            const std::optional<sf::Vector2i> cell = m_projection.renderToCell(renderPosition);
            if (!cell || cell->x < 0 || cell->y < 0) return std::nullopt;

            const TileMapCell result{static_cast<std::size_t>(cell->x),
                                     static_cast<std::size_t>(cell->y)};
            return contains(result) ? std::optional<TileMapCell>(result) : std::nullopt;
        }

        std::optional<sf::Vector2f> worldPosition(TileMapCell cell, bool centered = true) const
        {
            if (!contains(cell) || cell.column > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
                cell.row > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                return std::nullopt;

            return m_projection.cellToWorld(
                {static_cast<int>(cell.column), static_cast<int>(cell.row)}, centered);
        }

        std::optional<sf::Vector2f> renderPosition(TileMapCell cell, bool centered = true) const
        {
            const std::optional<sf::Vector2f> world = worldPosition(cell, centered);
            return world ? std::optional<sf::Vector2f>(m_projection.worldToRender(*world))
                         : std::nullopt;
        }

        bool projectedBounds(TileMapRegion region, sf::Vector2f& renderMinimum,
                             sf::Vector2f& renderMaximum) const
        {
            if (!validRegion(region)) return false;

            const sf::Vector2f worldCell = m_projection.worldCellSize();
            const sf::Vector2f worldMinimum = {
                static_cast<float>(region.firstColumn) * worldCell.x,
                static_cast<float>(region.firstRow) * worldCell.y};
            const sf::Vector2f worldMaximum = {
                static_cast<float>(region.firstColumn + region.columnCount) * worldCell.x,
                static_cast<float>(region.firstRow + region.rowCount) * worldCell.y};
            return m_projection.projectBounds(worldMinimum, worldMaximum, renderMinimum,
                                              renderMaximum);
        }

        // Returns a conservative Cartesian tile region covering a possibly
        // rotated render-space view. Padding is applied after clamping and is
        // useful for sprites that extend beyond their logical foot cells.
        std::optional<TileMapRegion> visibleRegionForView(const sf::View& view,
                                                          std::size_t paddingCells = 1u) const
        {
            const sf::Vector2f center = view.getCenter();
            const sf::Vector2f size = view.getSize();
            const float rotation = view.getRotation().asRadians();
            if (!finite(center) || !finite(size) || !std::isfinite(rotation) || size.x <= 0.f ||
                size.y <= 0.f)
                return std::nullopt;

            const float halfWidth = size.x * 0.5f;
            const float halfHeight = size.y * 0.5f;
            const float cosine = std::cos(rotation);
            const float sine = std::sin(rotation);
            const std::array<sf::Vector2f, 4u> local = {
                sf::Vector2f{-halfWidth, -halfHeight}, sf::Vector2f{halfWidth, -halfHeight},
                sf::Vector2f{halfWidth, halfHeight}, sf::Vector2f{-halfWidth, halfHeight}};

            double minimumColumn = std::numeric_limits<double>::infinity();
            double minimumRow = std::numeric_limits<double>::infinity();
            double maximumColumn = -std::numeric_limits<double>::infinity();
            double maximumRow = -std::numeric_limits<double>::infinity();
            const sf::Vector2f worldCell = m_projection.worldCellSize();

            for (const sf::Vector2f corner : local)
            {
                const sf::Vector2f rotated = {corner.x * cosine - corner.y * sine,
                                              corner.x * sine + corner.y * cosine};
                const sf::Vector2f world = m_projection.renderToWorld(center + rotated);
                if (!finite(world)) return std::nullopt;
                const double column = static_cast<double>(world.x) / worldCell.x;
                const double row = static_cast<double>(world.y) / worldCell.y;
                minimumColumn = std::min(minimumColumn, column);
                minimumRow = std::min(minimumRow, row);
                maximumColumn = std::max(maximumColumn, column);
                maximumRow = std::max(maximumRow, row);
            }

            const double firstColumnDouble = std::floor(minimumColumn);
            const double firstRowDouble = std::floor(minimumRow);
            const double lastColumnDouble = std::ceil(maximumColumn);
            const double lastRowDouble = std::ceil(maximumRow);
            const double widthDouble = static_cast<double>(m_width);
            const double heightDouble = static_cast<double>(m_height);

            const std::size_t firstColumn = static_cast<std::size_t>(
                std::clamp(firstColumnDouble, 0.0, widthDouble));
            const std::size_t firstRow =
                static_cast<std::size_t>(std::clamp(firstRowDouble, 0.0, heightDouble));
            const std::size_t lastColumn = static_cast<std::size_t>(
                std::clamp(lastColumnDouble, 0.0, widthDouble));
            const std::size_t lastRow =
                static_cast<std::size_t>(std::clamp(lastRowDouble, 0.0, heightDouble));

            if (firstColumn >= lastColumn || firstRow >= lastRow) return std::nullopt;

            TileMapRegion region;
            region.firstColumn = firstColumn > paddingCells ? firstColumn - paddingCells : 0u;
            region.firstRow = firstRow > paddingCells ? firstRow - paddingCells : 0u;
            const std::size_t paddedLastColumn =
                std::min(m_width, saturatingAdd(lastColumn, paddingCells));
            const std::size_t paddedLastRow =
                std::min(m_height, saturatingAdd(lastRow, paddingCells));
            region.columnCount = paddedLastColumn - region.firstColumn;
            region.rowCount = paddedLastRow - region.firstRow;
            return region;
        }

      private:
        bool validRegion(TileMapRegion region) const
        {
            return region.columnCount > 0u && region.rowCount > 0u && region.firstColumn < m_width &&
                   region.firstRow < m_height && region.columnCount <= m_width - region.firstColumn &&
                   region.rowCount <= m_height - region.firstRow;
        }

        static bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        static std::size_t saturatingAdd(std::size_t value, std::size_t increment)
        {
            return increment > std::numeric_limits<std::size_t>::max() - value
                       ? std::numeric_limits<std::size_t>::max()
                       : value + increment;
        }

      private:
        std::size_t m_width = 0u;
        std::size_t m_height = 0u;
        IsometricProjection2D m_projection;
    };
}

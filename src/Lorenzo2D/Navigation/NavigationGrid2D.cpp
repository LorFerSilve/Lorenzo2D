#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>

#include <algorithm>
#include <array>
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

        bool equalCell(const NavigationCell2D& left, const NavigationCell2D& right)
        {
            return left.walkable == right.walkable && left.traversalCost == right.traversalCost;
        }

        bool isDiagonal(sf::Vector2i direction)
        {
            return direction.x != 0 && direction.y != 0;
        }
    }

    NavigationGrid2D::NavigationGrid2D()
    {
        (void)reset({}, {});
    }

    NavigationGrid2D::NavigationGrid2D(NavigationGridConfig2D config, NavigationCell2D defaultCell)
    {
        (void)reset(std::move(config), defaultCell);
    }

    bool NavigationGrid2D::isValidConfig(const NavigationGridConfig2D& config)
    {
        if (config.size.x == 0u || config.size.y == 0u || !finite(config.cellSize) ||
            config.cellSize.x <= 0.f || config.cellSize.y <= 0.f || !finite(config.origin) ||
            config.size.x > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
            config.size.y > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
            (config.connectivity != NavigationConnectivity2D::FourWay &&
             config.connectivity != NavigationConnectivity2D::EightWay))
            return false;

        const std::size_t width = static_cast<std::size_t>(config.size.x);
        const std::size_t height = static_cast<std::size_t>(config.size.y);
        const double maximumX = static_cast<double>(config.origin.x) +
                                static_cast<double>(config.size.x) * config.cellSize.x;
        const double maximumY = static_cast<double>(config.origin.y) +
                                static_cast<double>(config.size.y) * config.cellSize.y;
        return width <= MaximumCellCount / height && std::isfinite(maximumX) &&
               std::isfinite(maximumY) && std::abs(maximumX) <= std::numeric_limits<float>::max() &&
               std::abs(maximumY) <= std::numeric_limits<float>::max();
    }

    bool NavigationGrid2D::isValidCell(const NavigationCell2D& cell)
    {
        return std::isfinite(cell.traversalCost) && cell.traversalCost > 0.f;
    }

    bool NavigationGrid2D::reset(NavigationGridConfig2D config, NavigationCell2D defaultCell)
    {
        if (!isValidConfig(config) || !isValidCell(defaultCell)) return false;

        const std::size_t count =
            static_cast<std::size_t>(config.size.x) * static_cast<std::size_t>(config.size.y);
        m_config = std::move(config);
        m_cells.assign(count, defaultCell);
        advanceRevision();
        return true;
    }

    const NavigationGridConfig2D& NavigationGrid2D::config() const
    {
        return m_config;
    }

    std::size_t NavigationGrid2D::cellCount() const
    {
        return m_cells.size();
    }

    std::uint64_t NavigationGrid2D::revision() const
    {
        return m_revision;
    }

    bool NavigationGrid2D::contains(sf::Vector2i cellPosition) const
    {
        return cellPosition.x >= 0 && cellPosition.y >= 0 &&
               static_cast<unsigned int>(cellPosition.x) < m_config.size.x &&
               static_cast<unsigned int>(cellPosition.y) < m_config.size.y;
    }

    std::optional<std::size_t> NavigationGrid2D::indexOf(sf::Vector2i cellPosition) const
    {
        if (!contains(cellPosition)) return std::nullopt;
        return static_cast<std::size_t>(cellPosition.y) *
                   static_cast<std::size_t>(m_config.size.x) +
               static_cast<std::size_t>(cellPosition.x);
    }

    std::optional<sf::Vector2i> NavigationGrid2D::cellAt(std::size_t index) const
    {
        if (index >= m_cells.size()) return std::nullopt;
        const std::size_t width = static_cast<std::size_t>(m_config.size.x);
        return sf::Vector2i{static_cast<int>(index % width), static_cast<int>(index / width)};
    }

    std::optional<NavigationCell2D> NavigationGrid2D::cell(sf::Vector2i position) const
    {
        const auto index = indexOf(position);
        if (!index) return std::nullopt;
        return m_cells[*index];
    }

    bool NavigationGrid2D::setCell(sf::Vector2i position, NavigationCell2D value)
    {
        const auto index = indexOf(position);
        if (!index || !isValidCell(value)) return false;
        if (!equalCell(m_cells[*index], value))
        {
            m_cells[*index] = value;
            advanceRevision();
        }
        return true;
    }

    bool NavigationGrid2D::setWalkable(sf::Vector2i position, bool walkable)
    {
        auto value = cell(position);
        if (!value) return false;
        value->walkable = walkable;
        return setCell(position, *value);
    }

    bool NavigationGrid2D::setTraversalCost(sf::Vector2i position, float traversalCost)
    {
        auto value = cell(position);
        if (!value) return false;
        value->traversalCost = traversalCost;
        return setCell(position, *value);
    }

    sf::Vector2f NavigationGrid2D::cellCenter(sf::Vector2i cellPosition) const
    {
        return {
            m_config.origin.x + (static_cast<float>(cellPosition.x) + 0.5f) * m_config.cellSize.x,
            m_config.origin.y + (static_cast<float>(cellPosition.y) + 0.5f) * m_config.cellSize.y};
    }

    std::optional<sf::Vector2i> NavigationGrid2D::worldToCell(sf::Vector2f worldPosition) const
    {
        if (!finite(worldPosition) || !isValidConfig(m_config)) return std::nullopt;
        const sf::Vector2f local = worldPosition - m_config.origin;
        const double cellX = std::floor(static_cast<double>(local.x) / m_config.cellSize.x);
        const double cellY = std::floor(static_cast<double>(local.y) / m_config.cellSize.y);
        if (cellX < 0.0 || cellY < 0.0 || cellX >= static_cast<double>(m_config.size.x) ||
            cellY >= static_cast<double>(m_config.size.y))
            return std::nullopt;
        const sf::Vector2i result{static_cast<int>(cellX), static_cast<int>(cellY)};
        return contains(result) ? std::optional<sf::Vector2i>{result} : std::nullopt;
    }

    std::vector<sf::Vector2i> NavigationGrid2D::neighbors(sf::Vector2i position) const
    {
        static constexpr std::array<sf::Vector2i, 8u> directions = {
            sf::Vector2i{0, -1},  sf::Vector2i{-1, 0}, sf::Vector2i{1, 0},  sf::Vector2i{0, 1},
            sf::Vector2i{-1, -1}, sf::Vector2i{1, -1}, sf::Vector2i{-1, 1}, sf::Vector2i{1, 1}};

        std::vector<sf::Vector2i> result;
        result.reserve(m_config.connectivity == NavigationConnectivity2D::FourWay ? 4u : 8u);
        const std::size_t count =
            m_config.connectivity == NavigationConnectivity2D::FourWay ? 4u : directions.size();
        for (std::size_t index = 0u; index < count; ++index)
        {
            const sf::Vector2i candidate = position + directions[index];
            const auto candidateCell = cell(candidate);
            if (!candidateCell || !candidateCell->walkable) continue;

            if (isDiagonal(directions[index]) && !m_config.allowDiagonalCornerCutting)
            {
                const auto horizontal = cell(position + sf::Vector2i{directions[index].x, 0});
                const auto vertical = cell(position + sf::Vector2i{0, directions[index].y});
                if (!horizontal || !vertical || !horizontal->walkable || !vertical->walkable)
                    continue;
            }
            result.push_back(candidate);
        }
        return result;
    }

    float NavigationGrid2D::minimumTraversalCost() const
    {
        float result = std::numeric_limits<float>::max();
        for (const NavigationCell2D& value : m_cells)
            if (value.walkable) result = std::min(result, value.traversalCost);
        return result == std::numeric_limits<float>::max() ? 1.f : result;
    }

    std::size_t NavigationGrid2D::bakeObstacles(const PhysicsQueryContext2D& queries,
                                                float agentRadius,
                                                const PhysicsQueryFilter2D& filter)
    {
        if (!std::isfinite(agentRadius) || agentRadius < 0.f) return 0u;

        std::size_t blocked = 0u;
        bool changed = false;
        const sf::Vector2f footprint =
            m_config.cellSize + sf::Vector2f{agentRadius * 2.f, agentRadius * 2.f};
        if (!finite(footprint)) return 0u;
        for (std::size_t index = 0u; index < m_cells.size(); ++index)
        {
            const auto position = cellAt(index);
            const bool obstacle =
                !queries.overlapBox(cellCenter(*position), footprint, 0.f, filter).empty();
            if (obstacle) ++blocked;
            if (obstacle && m_cells[index].walkable)
            {
                m_cells[index].walkable = false;
                changed = true;
            }
        }
        if (changed) advanceRevision();
        return blocked;
    }

    void NavigationGrid2D::advanceRevision()
    {
        ++m_revision;
        if (m_revision == 0u) m_revision = 1u;
    }

    std::optional<NavigationGrid2D> navigationGridFromTileMap(
        const TileMapData& tileMap, const NavigationTileMapOptions2D& options)
    {
        if (!tileMap.isValid() || tileMap.width() > std::numeric_limits<unsigned int>::max() ||
            tileMap.height() > std::numeric_limits<unsigned int>::max())
            return std::nullopt;

        NavigationGridConfig2D config;
        config.size = {static_cast<unsigned int>(tileMap.width()),
                       static_cast<unsigned int>(tileMap.height())};
        config.cellSize = tileMap.tileSize();
        config.origin = options.origin;
        config.connectivity = options.connectivity;
        config.allowDiagonalCornerCutting = options.allowDiagonalCornerCutting;
        if (!NavigationGrid2D::isValidConfig(config)) return std::nullopt;

        NavigationGrid2D result(config);
        for (std::size_t row = 0u; row < tileMap.height(); ++row)
        {
            for (std::size_t column = 0u; column < tileMap.width(); ++column)
            {
                NavigationCell2D combined;
                for (const TileMapLayer& layer : tileMap.layers())
                {
                    if (!options.includeHiddenLayers && !layer.visible) continue;
                    const std::size_t index = row * tileMap.width() + column;
                    const TileId tile = layer.tiles[index];
                    if (tile == EmptyTile) continue;
                    const TileDefinition* definition = tileMap.definition(tile);
                    if (definition == nullptr) return std::nullopt;
                    combined.walkable = combined.walkable && definition->navigable;
                    combined.traversalCost =
                        std::max(combined.traversalCost, definition->movementCost);
                }
                (void)result.setCell({static_cast<int>(column), static_cast<int>(row)}, combined);
            }
        }
        return result;
    }
}

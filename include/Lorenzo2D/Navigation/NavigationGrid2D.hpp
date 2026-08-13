#pragma once

#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace l2d
{
    enum class NavigationConnectivity2D
    {
        FourWay,
        EightWay
    };

    struct NavigationGridConfig2D
    {
        sf::Vector2u size = {1u, 1u};
        sf::Vector2f cellSize = {32.f, 32.f};
        sf::Vector2f origin = {0.f, 0.f};
        NavigationConnectivity2D connectivity = NavigationConnectivity2D::FourWay;
        bool allowDiagonalCornerCutting = false;
    };

    struct NavigationCell2D
    {
        bool walkable = true;
        float traversalCost = 1.f;
    };

    struct NavigationTileMapOptions2D
    {
        sf::Vector2f origin = {0.f, 0.f};
        NavigationConnectivity2D connectivity = NavigationConnectivity2D::FourWay;
        bool allowDiagonalCornerCutting = false;
        bool includeHiddenLayers = false;
    };

    // A data-only, Cartesian navigation grid. Rendering projections do not
    // change its coordinates, so the same grid can drive orthogonal and
    // isometric presentation.
    class NavigationGrid2D
    {
      public:
        static constexpr std::size_t MaximumCellCount = TileMapData::MaximumCellCount;

        NavigationGrid2D();
        explicit NavigationGrid2D(NavigationGridConfig2D config, NavigationCell2D defaultCell = {});

        static bool isValidConfig(const NavigationGridConfig2D& config);
        static bool isValidCell(const NavigationCell2D& cell);

        bool reset(NavigationGridConfig2D config, NavigationCell2D defaultCell = {});
        const NavigationGridConfig2D& config() const;
        std::size_t cellCount() const;
        std::uint64_t revision() const;

        bool contains(sf::Vector2i cell) const;
        std::optional<std::size_t> indexOf(sf::Vector2i cell) const;
        std::optional<sf::Vector2i> cellAt(std::size_t index) const;
        std::optional<NavigationCell2D> cell(sf::Vector2i position) const;

        bool setCell(sf::Vector2i position, NavigationCell2D cell);
        bool setWalkable(sf::Vector2i position, bool walkable);
        bool setTraversalCost(sf::Vector2i position, float traversalCost);

        sf::Vector2f cellCenter(sf::Vector2i cell) const;
        std::optional<sf::Vector2i> worldToCell(sf::Vector2f worldPosition) const;
        std::vector<sf::Vector2i> neighbors(sf::Vector2i cell) const;
        float minimumTraversalCost() const;

        // Conservatively blocks every cell whose area plus agentRadius
        // overlaps a query collider. Existing traversal costs are preserved.
        // Returns the number of cells overlapped by query colliders.
        std::size_t bakeObstacles(const PhysicsQueryContext2D& queries, float agentRadius = 0.f,
                                  const PhysicsQueryFilter2D& filter = {});

      private:
        void advanceRevision();

        NavigationGridConfig2D m_config;
        std::vector<NavigationCell2D> m_cells;
        std::uint64_t m_revision = 0u;
    };

    // Combines visible tile layers deterministically. Any non-navigable tile
    // blocks a cell; otherwise the greatest declared movement cost wins.
    std::optional<NavigationGrid2D> navigationGridFromTileMap(
        const TileMapData& tileMap, const NavigationTileMapOptions2D& options = {});
}

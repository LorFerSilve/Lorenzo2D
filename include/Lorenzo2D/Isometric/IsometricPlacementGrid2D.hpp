#pragma once

#include <Lorenzo2D/Renderer/CoordinateProjection2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace l2d
{
    struct IsometricPlacementGridConfig2D
    {
        sf::Vector2u size = {1u, 1u};
        sf::Vector2f cellSize = {1.f, 1.f};
        sf::Vector2f worldOrigin = {0.f, 0.f};
    };

    class IsometricPlacementGrid2D
    {
      public:
        static constexpr std::size_t MaximumCellCount = TileMapData::MaximumCellCount;
        static constexpr float MinimumCellDimension = 0.0001f;

        IsometricPlacementGrid2D();
        explicit IsometricPlacementGrid2D(IsometricPlacementGridConfig2D config);

        static bool isValidConfig(const IsometricPlacementGridConfig2D& config);
        // Reset is transactional. Allocation failure throws without changing the old grid.
        bool reset(IsometricPlacementGridConfig2D config);
        // Builds a placement grid from finite isometric tile data. Collision-role
        // cells and definitions marked Solid become blocked when requested.
        bool resetFromTileMap(const TileMapData& data, sf::Vector2f worldOrigin = {0.f, 0.f},
                              bool blockCollisionCells = true);

        const IsometricPlacementGridConfig2D& config() const;
        std::size_t cellCount() const;

        bool contains(sf::Vector2u cell) const;
        std::optional<sf::Vector2u> worldToCell(sf::Vector2f worldPosition) const;
        std::optional<sf::Vector2u> pickCell(
            sf::Vector2f renderPosition, const CoordinateProjection2D& projection) const;
        std::optional<sf::Vector2f> cellWorldOrigin(sf::Vector2u cell) const;
        std::optional<sf::Vector2f> cellWorldCenter(sf::Vector2u cell) const;

        bool blocked(sf::Vector2u cell) const;
        bool occupied(sf::Vector2u cell) const;
        bool available(sf::Vector2u cell) const;
        bool setBlocked(sf::Vector2u cell, bool blocked);
        bool setOccupied(sf::Vector2u cell, bool occupied);

        bool canPlace(sf::Vector2u firstCell, sf::Vector2u footprint = {1u, 1u}) const;
        bool place(sf::Vector2u firstCell, sf::Vector2u footprint = {1u, 1u});
        // Removes dynamic occupancy while preserving static blocked cells.
        bool remove(sf::Vector2u firstCell, sf::Vector2u footprint = {1u, 1u});
        void clearPlacements();

      private:
        static constexpr std::uint8_t BlockedBit = 1u << 0u;
        static constexpr std::uint8_t OccupiedBit = 1u << 1u;

        std::optional<std::size_t> indexOf(sf::Vector2u cell) const;
        bool validFootprint(sf::Vector2u firstCell, sf::Vector2u footprint) const;

        IsometricPlacementGridConfig2D m_config;
        std::vector<std::uint8_t> m_cells;
    };
}

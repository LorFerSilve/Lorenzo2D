#pragma once

#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace l2d
{
    enum class NavigationPathStatus2D
    {
        Succeeded,
        InvalidGrid,
        InvalidStart,
        InvalidGoal,
        NoPath,
        SearchLimitReached
    };

    struct NavigationPathOptions2D
    {
        std::size_t maximumVisitedNodes = 100000u;
        bool collapseCollinearPoints = true;
    };

    struct NavigationPath2D
    {
        NavigationPathStatus2D status = NavigationPathStatus2D::InvalidGrid;
        std::vector<sf::Vector2i> cells;
        std::vector<sf::Vector2f> points;
        float totalCost = 0.f;
        std::size_t visitedNodes = 0u;
        std::uint64_t gridRevision = 0u;

        bool succeeded() const;
    };

    // Stateless deterministic A*. Equal-cost candidates are resolved by
    // heuristic and row-major node index, independent of allocator behavior.
    class AStarPathfinder2D
    {
      public:
        NavigationPath2D findPath(const NavigationGrid2D& grid, sf::Vector2i start,
                                  sf::Vector2i goal,
                                  const NavigationPathOptions2D& options = {}) const;
        NavigationPath2D findWorldPath(const NavigationGrid2D& grid, sf::Vector2f start,
                                       sf::Vector2f goal,
                                       const NavigationPathOptions2D& options = {}) const;
    };
}

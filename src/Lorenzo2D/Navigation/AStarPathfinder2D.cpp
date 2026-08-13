#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace l2d
{
    namespace
    {
        constexpr std::size_t NoParent = std::numeric_limits<std::size_t>::max();
        constexpr float DiagonalCost = 1.41421356237f;

        struct OpenNode
        {
            float score = 0.f;
            float heuristic = 0.f;
            float pathCost = 0.f;
            std::size_t index = 0u;
        };

        struct WorseOpenNode
        {
            bool operator()(const OpenNode& left, const OpenNode& right) const
            {
                if (left.score != right.score) return left.score > right.score;
                if (left.heuristic != right.heuristic) return left.heuristic > right.heuristic;
                return left.index > right.index;
            }
        };

        float heuristic(const NavigationGrid2D& grid, sf::Vector2i from, sf::Vector2i to,
                        float minimumTraversalCost)
        {
            const float dx = static_cast<float>(std::abs(from.x - to.x));
            const float dy = static_cast<float>(std::abs(from.y - to.y));
            const float distance = grid.config().connectivity == NavigationConnectivity2D::FourWay
                                       ? dx + dy
                                       : std::max(dx, dy) + (DiagonalCost - 1.f) * std::min(dx, dy);
            return distance * minimumTraversalCost;
        }

        bool diagonal(sf::Vector2i from, sf::Vector2i to)
        {
            return from.x != to.x && from.y != to.y;
        }

        std::vector<sf::Vector2f> collapsedPoints(const NavigationGrid2D& grid,
                                                  const std::vector<sf::Vector2i>& cells,
                                                  bool collapse)
        {
            std::vector<sf::Vector2f> result;
            if (cells.empty()) return result;
            result.reserve(cells.size());
            result.push_back(grid.cellCenter(cells.front()));
            for (std::size_t index = 1u; index + 1u < cells.size(); ++index)
            {
                const sf::Vector2i before = cells[index] - cells[index - 1u];
                const sf::Vector2i after = cells[index + 1u] - cells[index];
                if (!collapse || before != after) result.push_back(grid.cellCenter(cells[index]));
            }
            if (cells.size() > 1u) result.push_back(grid.cellCenter(cells.back()));
            return result;
        }
    }

    bool NavigationPath2D::succeeded() const
    {
        return status == NavigationPathStatus2D::Succeeded;
    }

    NavigationPath2D AStarPathfinder2D::findPath(const NavigationGrid2D& grid, sf::Vector2i start,
                                                 sf::Vector2i goal,
                                                 const NavigationPathOptions2D& options) const
    {
        NavigationPath2D result;
        result.gridRevision = grid.revision();
        if (!NavigationGrid2D::isValidConfig(grid.config()) || grid.cellCount() == 0u)
            return result;

        const auto startCell = grid.cell(start);
        if (!startCell || !startCell->walkable)
        {
            result.status = NavigationPathStatus2D::InvalidStart;
            return result;
        }
        const auto goalCell = grid.cell(goal);
        if (!goalCell || !goalCell->walkable)
        {
            result.status = NavigationPathStatus2D::InvalidGoal;
            return result;
        }
        if (options.maximumVisitedNodes == 0u)
        {
            result.status = NavigationPathStatus2D::SearchLimitReached;
            return result;
        }

        const std::size_t startIndex = *grid.indexOf(start);
        const std::size_t goalIndex = *grid.indexOf(goal);
        const float minimumTraversalCost = grid.minimumTraversalCost();
        std::vector<float> costs(grid.cellCount(), std::numeric_limits<float>::infinity());
        std::vector<std::size_t> parents(grid.cellCount(), NoParent);
        std::vector<bool> closed(grid.cellCount(), false);
        std::priority_queue<OpenNode, std::vector<OpenNode>, WorseOpenNode> open;

        costs[startIndex] = 0.f;
        const float startHeuristic = heuristic(grid, start, goal, minimumTraversalCost);
        open.push({startHeuristic, startHeuristic, 0.f, startIndex});

        bool found = false;
        while (!open.empty())
        {
            const OpenNode current = open.top();
            open.pop();
            if (closed[current.index] || current.pathCost != costs[current.index]) continue;
            if (result.visitedNodes >= options.maximumVisitedNodes)
            {
                result.status = NavigationPathStatus2D::SearchLimitReached;
                return result;
            }

            closed[current.index] = true;
            ++result.visitedNodes;
            if (current.index == goalIndex)
            {
                found = true;
                break;
            }

            const sf::Vector2i currentCell = *grid.cellAt(current.index);
            for (const sf::Vector2i neighbor : grid.neighbors(currentCell))
            {
                const std::size_t neighborIndex = *grid.indexOf(neighbor);
                if (closed[neighborIndex]) continue;
                const float stepCost = grid.cell(neighbor)->traversalCost *
                                       (diagonal(currentCell, neighbor) ? DiagonalCost : 1.f);
                const float candidate = current.pathCost + stepCost;
                if (candidate >= costs[neighborIndex]) continue;

                costs[neighborIndex] = candidate;
                parents[neighborIndex] = current.index;
                const float remaining = heuristic(grid, neighbor, goal, minimumTraversalCost);
                open.push({candidate + remaining, remaining, candidate, neighborIndex});
            }
        }

        if (!found)
        {
            result.status = NavigationPathStatus2D::NoPath;
            return result;
        }

        for (std::size_t index = goalIndex; index != NoParent; index = parents[index])
            result.cells.push_back(*grid.cellAt(index));
        std::reverse(result.cells.begin(), result.cells.end());
        result.points = collapsedPoints(grid, result.cells, options.collapseCollinearPoints);
        result.totalCost = costs[goalIndex];
        result.status = NavigationPathStatus2D::Succeeded;
        return result;
    }

    NavigationPath2D AStarPathfinder2D::findWorldPath(const NavigationGrid2D& grid,
                                                      sf::Vector2f start, sf::Vector2f goal,
                                                      const NavigationPathOptions2D& options) const
    {
        const auto startCell = grid.worldToCell(start);
        const auto goalCell = grid.worldToCell(goal);
        if (!startCell)
        {
            NavigationPath2D result;
            result.status = NavigationPathStatus2D::InvalidStart;
            result.gridRevision = grid.revision();
            return result;
        }
        if (!goalCell)
        {
            NavigationPath2D result;
            result.status = NavigationPathStatus2D::InvalidGoal;
            result.gridRevision = grid.revision();
            return result;
        }

        NavigationPath2D result = findPath(grid, *startCell, *goalCell, options);
        if (!result.succeeded()) return result;
        if (*startCell == *goalCell)
            result.points = start == goal ? std::vector<sf::Vector2f>{goal}
                                          : std::vector<sf::Vector2f>{start, goal};
        else
        {
            result.points.front() = start;
            result.points.back() = goal;
        }
        return result;
    }
}

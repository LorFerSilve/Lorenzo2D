#include "UniformGridBroadPhase2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace l2d
{
    namespace detail
    {
        namespace
        {
            struct GridCell2D
            {
                std::int64_t x = 0;
                std::int64_t y = 0;

                bool operator==(const GridCell2D& other) const
                {
                    return x == other.x && y == other.y;
                }
            };

            struct GridCellHash2D
            {
                std::size_t operator()(const GridCell2D& cell) const
                {
                    const std::size_t xHash =
                        std::hash<std::int64_t>{}(cell.x);
                    const std::size_t yHash =
                        std::hash<std::int64_t>{}(cell.y);

                    return xHash ^ (yHash << 1u);
                }
            };

            bool pairLess(
                const BroadPhasePair2D& left,
                const BroadPhasePair2D& right
            )
            {
                if (left.first != right.first)
                    return left.first < right.first;

                return left.second < right.second;
            }

            bool samePair(
                const BroadPhasePair2D& left,
                const BroadPhasePair2D& right
            )
            {
                return left.first == right.first &&
                    left.second == right.second;
            }

            bool isEligiblePair(
                const std::vector<BroadPhaseProxy2D>& proxies,
                std::size_t first,
                std::size_t second
            )
            {
                return first != second &&
                    (proxies[first].moving || proxies[second].moving);
            }

            bool boundsOverlap(
                const BroadPhaseProxy2D& first,
                const BroadPhaseProxy2D& second
            )
            {
                if (!first.boundsValid || !second.boundsValid)
                    return true;

                return first.minimum.x <= second.maximum.x &&
                    first.maximum.x >= second.minimum.x &&
                    first.minimum.y <= second.maximum.y &&
                    first.maximum.y >= second.minimum.y;
            }

            void appendPair(
                const std::vector<BroadPhaseProxy2D>& proxies,
                std::size_t first,
                std::size_t second,
                std::vector<BroadPhasePair2D>& pairs
            )
            {
                if (!isEligiblePair(proxies, first, second))
                    return;

                if (second < first)
                    std::swap(first, second);

                pairs.push_back({ first, second });
            }

            void appendSpatialPair(
                const std::vector<BroadPhaseProxy2D>& proxies,
                std::size_t first,
                std::size_t second,
                std::vector<BroadPhasePair2D>& pairs
            )
            {
                if (
                    first == second ||
                    !boundsOverlap(proxies[first], proxies[second])
                )
                {
                    return;
                }

                appendPair(proxies, first, second, pairs);
            }

            void sortAndDeduplicate(std::vector<BroadPhasePair2D>& pairs)
            {
                std::sort(pairs.begin(), pairs.end(), pairLess);
                pairs.erase(
                    std::unique(pairs.begin(), pairs.end(), samePair),
                    pairs.end()
                );
            }

            bool toCellCoordinate(
                float coordinate,
                float cellSize,
                std::int64_t& result
            )
            {
                if (!std::isfinite(coordinate))
                    return false;

                const long double cell = std::floor(
                    static_cast<long double>(coordinate) /
                    static_cast<long double>(cellSize)
                );
                const long double coordinateLimit = std::ldexp(1.0L, 63);

                if (
                    !std::isfinite(cell) ||
                    cell < -coordinateLimit ||
                    cell >= coordinateLimit
                )
                {
                    return false;
                }

                result = static_cast<std::int64_t>(cell);
                return true;
            }

            bool inclusiveSpan(
                std::int64_t minimum,
                std::int64_t maximum,
                std::uint64_t& result
            )
            {
                if (maximum < minimum)
                    return false;

                const std::uint64_t difference =
                    static_cast<std::uint64_t>(maximum) -
                    static_cast<std::uint64_t>(minimum);

                if (difference == std::numeric_limits<std::uint64_t>::max())
                    return false;

                result = difference + 1u;
                return true;
            }

            bool gridRange(
                const BroadPhaseProxy2D& proxy,
                float cellSize,
                std::uint32_t maxCellsPerProxy,
                GridCell2D& minimum,
                GridCell2D& maximum
            )
            {
                if (!proxy.boundsValid)
                    return false;

                if (
                    proxy.minimum.x > proxy.maximum.x ||
                    proxy.minimum.y > proxy.maximum.y
                )
                {
                    return false;
                }

                if (
                    !toCellCoordinate(proxy.minimum.x, cellSize, minimum.x) ||
                    !toCellCoordinate(proxy.minimum.y, cellSize, minimum.y) ||
                    !toCellCoordinate(proxy.maximum.x, cellSize, maximum.x) ||
                    !toCellCoordinate(proxy.maximum.y, cellSize, maximum.y)
                )
                {
                    return false;
                }

                std::uint64_t columns = 0;
                std::uint64_t rows = 0;

                if (
                    !inclusiveSpan(minimum.x, maximum.x, columns) ||
                    !inclusiveSpan(minimum.y, maximum.y, rows)
                )
                {
                    return false;
                }

                const std::uint64_t maximumCells = maxCellsPerProxy;

                return columns <= maximumCells &&
                    rows <= maximumCells / columns;
            }

            std::size_t addSaturated(std::size_t left, std::size_t right)
            {
                const std::size_t maximum =
                    std::numeric_limits<std::size_t>::max();

                if (right > maximum - left)
                    return maximum;

                return left + right;
            }

            std::size_t multiplySaturated(
                std::size_t left,
                std::size_t right
            )
            {
                const std::size_t maximum =
                    std::numeric_limits<std::size_t>::max();

                if (left != 0 && right > maximum / left)
                    return maximum;

                return left * right;
            }

            std::size_t pairCombinationCount(std::size_t count)
            {
                if (count < 2)
                    return 0;

                if (count % 2 == 0)
                    return multiplySaturated(count / 2, count - 1);

                return multiplySaturated(count, (count - 1) / 2);
            }
        }

        std::size_t countBruteForcePairs(
            const std::vector<BroadPhaseProxy2D>& proxies
        )
        {
            std::size_t movingCount = 0;

            for (const BroadPhaseProxy2D& proxy : proxies)
            {
                if (proxy.moving)
                    ++movingCount;
            }

            const std::size_t staticCount = proxies.size() - movingCount;

            return addSaturated(
                pairCombinationCount(movingCount),
                multiplySaturated(movingCount, staticCount)
            );
        }

        BroadPhaseBuildResult2D buildBruteForcePairs(
            const std::vector<BroadPhaseProxy2D>& proxies
        )
        {
            BroadPhaseBuildResult2D result;
            result.pairs.reserve(countBruteForcePairs(proxies));

            std::vector<std::size_t> movingProxies;
            std::vector<std::size_t> staticProxies;

            movingProxies.reserve(proxies.size());
            staticProxies.reserve(proxies.size());

            for (std::size_t index = 0; index < proxies.size(); ++index)
            {
                if (proxies[index].moving)
                    movingProxies.push_back(index);
                else
                    staticProxies.push_back(index);
            }

            for (
                std::size_t first = 0;
                first < movingProxies.size();
                ++first
            )
            {
                for (std::size_t second = first + 1;
                    second < movingProxies.size();
                    ++second)
                {
                    appendPair(
                        proxies,
                        movingProxies[first],
                        movingProxies[second],
                        result.pairs
                    );
                }

                for (const std::size_t staticProxy : staticProxies)
                {
                    appendPair(
                        proxies,
                        movingProxies[first],
                        staticProxy,
                        result.pairs
                    );
                }
            }

            sortAndDeduplicate(result.pairs);
            return result;
        }

        BroadPhaseBuildResult2D buildUniformGridPairs(
            const std::vector<BroadPhaseProxy2D>& proxies,
            float cellSize,
            std::uint32_t maxCellsPerProxy
        )
        {
            if (
                !std::isfinite(cellSize) ||
                cellSize <= 0.f ||
                maxCellsPerProxy == 0
            )
            {
                BroadPhaseBuildResult2D result =
                    buildBruteForcePairs(proxies);
                result.fallbackProxyCount = proxies.size();
                return result;
            }

            using CellMap = std::unordered_map<
                GridCell2D,
                std::vector<std::size_t>,
                GridCellHash2D
            >;

            CellMap cells;
            cells.reserve(proxies.size());

            std::vector<std::size_t> fallbackProxies;
            std::vector<std::size_t> movingProxies;
            std::vector<GridCell2D> minimumCells(proxies.size());
            std::vector<GridCell2D> maximumCells(proxies.size());
            std::vector<bool> inserted(proxies.size(), false);

            for (std::size_t index = 0; index < proxies.size(); ++index)
            {
                if (proxies[index].moving)
                    movingProxies.push_back(index);

                GridCell2D minimum;
                GridCell2D maximum;

                if (!gridRange(
                    proxies[index],
                    cellSize,
                    maxCellsPerProxy,
                    minimum,
                    maximum
                ))
                {
                    fallbackProxies.push_back(index);
                    continue;
                }

                minimumCells[index] = minimum;
                maximumCells[index] = maximum;
                inserted[index] = true;

                for (std::int64_t y = minimum.y;; ++y)
                {
                    for (std::int64_t x = minimum.x;; ++x)
                    {
                        cells[{ x, y }].push_back(index);

                        if (x == maximum.x)
                            break;
                    }

                    if (y == maximum.y)
                        break;
                }
            }

            BroadPhaseBuildResult2D result;
            result.occupiedCellCount = cells.size();
            result.fallbackProxyCount = fallbackProxies.size();

            for (std::size_t index = 0; index < proxies.size(); ++index)
            {
                if (!proxies[index].moving || !inserted[index])
                    continue;

                const GridCell2D minimum = minimumCells[index];
                const GridCell2D maximum = maximumCells[index];

                for (std::int64_t y = minimum.y;; ++y)
                {
                    for (std::int64_t x = minimum.x;; ++x)
                    {
                        const auto cell = cells.find({ x, y });

                        if (cell != cells.end())
                        {
                            for (const std::size_t occupant : cell->second)
                            {
                                appendSpatialPair(
                                    proxies,
                                    index,
                                    occupant,
                                    result.pairs
                                );
                            }
                        }

                        if (x == maximum.x)
                            break;
                    }

                    if (y == maximum.y)
                        break;
                }
            }

            for (const std::size_t fallback : fallbackProxies)
            {
                if (proxies[fallback].moving)
                {
                    for (
                        std::size_t other = 0;
                        other < proxies.size();
                        ++other
                    )
                    {
                        appendSpatialPair(
                            proxies,
                            fallback,
                            other,
                            result.pairs
                        );
                    }
                }
                else
                {
                    for (const std::size_t moving : movingProxies)
                    {
                        appendSpatialPair(
                            proxies,
                            fallback,
                            moving,
                            result.pairs
                        );
                    }
                }
            }

            sortAndDeduplicate(result.pairs);
            return result;
        }
    }
}

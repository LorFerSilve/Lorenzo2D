#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace l2d
{
    namespace detail
    {
        struct BroadPhaseProxy2D
        {
            sf::Vector2f minimum = {0.f, 0.f};
            sf::Vector2f maximum = {0.f, 0.f};
            bool boundsValid = false;
            bool moving = false;
        };

        struct BroadPhasePair2D
        {
            std::size_t first = 0;
            std::size_t second = 0;
        };

        struct BroadPhaseBuildResult2D
        {
            std::vector<BroadPhasePair2D> pairs;
            std::size_t occupiedCellCount = 0;
            std::size_t fallbackProxyCount = 0;
        };

        std::size_t countBruteForcePairs(const std::vector<BroadPhaseProxy2D>& proxies);

        BroadPhaseBuildResult2D buildBruteForcePairs(const std::vector<BroadPhaseProxy2D>& proxies);

        BroadPhaseBuildResult2D buildUniformGridPairs(const std::vector<BroadPhaseProxy2D>& proxies,
                                                      float cellSize,
                                                      std::uint32_t maxCellsPerProxy);
    }
}

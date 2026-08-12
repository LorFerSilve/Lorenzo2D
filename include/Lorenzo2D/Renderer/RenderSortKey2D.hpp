#pragma once

#include <cstdint>

namespace l2d
{
    enum class RenderDepthMode2D
    {
        Explicit,
        WorldY,
        ProjectedY,
        Fixed
    };

    struct RenderSortKey2D
    {
        std::int32_t layer = 0;
        float depth = 0.f;
        std::int32_t order = 0;
        std::uint64_t insertionOrder = 0;
    };

    bool operator<(const RenderSortKey2D& left, const RenderSortKey2D& right) noexcept;
}

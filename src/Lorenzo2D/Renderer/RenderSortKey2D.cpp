#include <Lorenzo2D/Renderer/RenderSortKey2D.hpp>

#include <cmath>

namespace l2d
{
    namespace
    {
        float finiteDepth(float depth)
        {
            return std::isfinite(depth) ? depth : 0.f;
        }
    }

    bool operator<(const RenderSortKey2D& left, const RenderSortKey2D& right) noexcept
    {
        if (left.layer != right.layer) return left.layer < right.layer;

        const float leftDepth = finiteDepth(left.depth);
        const float rightDepth = finiteDepth(right.depth);

        if (leftDepth != rightDepth) return leftDepth < rightDepth;
        if (left.order != right.order) return left.order < right.order;

        return left.insertionOrder < right.insertionOrder;
    }
}

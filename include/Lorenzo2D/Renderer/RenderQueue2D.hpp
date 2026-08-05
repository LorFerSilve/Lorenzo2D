#pragma once

#include <Lorenzo2D/Scene/GameObjectHandle.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class Scene;

    struct RenderQueueEntry2D
    {
        GameObjectHandle gameObject;
        std::int32_t zOrder = 0;
        std::size_t insertionOrder = 0;
    };

    // A stable per-pass snapshot. Rebuilding applies z-order changes while
    // excluding objects created during the active render pass.
    class RenderQueue2D
    {
      public:
        void build(Scene& scene);
        void clear();

        std::size_t size() const;
        bool empty() const;
        const std::vector<RenderQueueEntry2D>& entries() const;

        void render(sf::RenderWindow& window, float interpolationAlpha = 1.f) const;

      private:
        std::vector<RenderQueueEntry2D> m_entries;
    };
}

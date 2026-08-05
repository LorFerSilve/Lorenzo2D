#pragma once

#include <array>
#include <cstddef>

namespace l2d
{
    enum class RenderLayer2D
    {
        World = 0,
        PhysicsDebug,
        UI,

        Count
    };

    class RenderLayerStack2D
    {
      public:
        RenderLayerStack2D();

        void setLayerEnabled(RenderLayer2D layer, bool enabled);
        bool isLayerEnabled(RenderLayer2D layer) const;

        void enableAll();
        void disableAll();

      private:
        static std::size_t layerToIndex(RenderLayer2D layer);

      private:
        std::array<bool, static_cast<std::size_t>(RenderLayer2D::Count)> m_enabledLayers;
    };
}

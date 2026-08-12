#pragma once

#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include <array>
#include <cstddef>

namespace l2d
{
    // Kept as a source-compatible name. These values are render passes;
    // RenderOrder2D::layer() is the free integer layer within a pass.
    using RenderLayer2D = RenderPass2D;

    class RenderLayerStack2D
    {
      public:
        RenderLayerStack2D();

        void setLayerEnabled(RenderLayer2D layer, bool enabled);
        bool isLayerEnabled(RenderLayer2D layer) const;
        void setPassEnabled(RenderPass2D pass, bool enabled);
        bool isPassEnabled(RenderPass2D pass) const;

        void enableAll();
        void disableAll();

      private:
        static std::size_t layerToIndex(RenderLayer2D layer);

      private:
        std::array<bool, static_cast<std::size_t>(RenderLayer2D::Count)> m_enabledLayers;
    };
}

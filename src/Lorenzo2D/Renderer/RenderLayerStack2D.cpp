#include <Lorenzo2D/Renderer/RenderLayerStack2D.hpp>

namespace l2d
{
    RenderLayerStack2D::RenderLayerStack2D()
        : m_enabledLayers{}
    {
        enableAll();
    }

    void RenderLayerStack2D::setLayerEnabled(RenderLayer2D layer, bool enabled)
    {
        const std::size_t index = layerToIndex(layer);

        if (index >= m_enabledLayers.size())
            return;

        m_enabledLayers[index] = enabled;
    }

    bool RenderLayerStack2D::isLayerEnabled(RenderLayer2D layer) const
    {
        const std::size_t index = layerToIndex(layer);

        if (index >= m_enabledLayers.size())
            return false;

        return m_enabledLayers[index];
    }

    void RenderLayerStack2D::enableAll()
    {
        for (bool& enabled : m_enabledLayers)
        {
            enabled = true;
        }
    }

    void RenderLayerStack2D::disableAll()
    {
        for (bool& enabled : m_enabledLayers)
        {
            enabled = false;
        }
    }

    std::size_t RenderLayerStack2D::layerToIndex(RenderLayer2D layer)
    {
        return static_cast<std::size_t>(layer);
    }
}
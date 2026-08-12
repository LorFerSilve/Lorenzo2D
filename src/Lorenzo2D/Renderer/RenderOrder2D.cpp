#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>

#include <cmath>

namespace l2d
{
    RenderOrder2D::RenderOrder2D(RenderDepthMode2D depthMode)
    {
        setDepthMode(depthMode);
    }

    void RenderOrder2D::setPass(RenderPass2D pass)
    {
        const auto index = static_cast<std::uint32_t>(pass);

        if (index >= static_cast<std::uint32_t>(RenderPass2D::Count)) return;

        m_pass = pass;
    }

    RenderPass2D RenderOrder2D::pass() const
    {
        return m_pass;
    }

    void RenderOrder2D::setLayer(std::int32_t layer)
    {
        m_layer = layer;
    }

    std::int32_t RenderOrder2D::layer() const
    {
        return m_layer;
    }

    void RenderOrder2D::setDepthMode(RenderDepthMode2D depthMode)
    {
        switch (depthMode)
        {
        case RenderDepthMode2D::Explicit:
        case RenderDepthMode2D::WorldY:
        case RenderDepthMode2D::ProjectedY:
        case RenderDepthMode2D::Fixed:
            break;
        default:
            return;
        }

        m_depthMode = depthMode;
    }

    RenderDepthMode2D RenderOrder2D::depthMode() const
    {
        return m_depthMode;
    }

    bool RenderOrder2D::setExplicitDepth(float depth)
    {
        if (!std::isfinite(depth)) return false;

        m_explicitDepth = depth;
        return true;
    }

    float RenderOrder2D::explicitDepth() const
    {
        return m_explicitDepth;
    }

    void RenderOrder2D::setOrder(std::int32_t order)
    {
        m_order = order;
    }

    void RenderOrder2D::clearOrder()
    {
        m_order.reset();
    }

    bool RenderOrder2D::hasOrder() const
    {
        return m_order.has_value();
    }

    std::int32_t RenderOrder2D::orderOr(std::int32_t fallback) const
    {
        return m_order.value_or(fallback);
    }

    bool RenderOrder2D::setLocalFootPoint(sf::Vector2f localFootPoint)
    {
        if (!std::isfinite(localFootPoint.x) || !std::isfinite(localFootPoint.y)) return false;

        m_localFootPoint = localFootPoint;
        return true;
    }

    void RenderOrder2D::clearLocalFootPoint()
    {
        m_localFootPoint.reset();
    }

    const std::optional<sf::Vector2f>& RenderOrder2D::localFootPoint() const
    {
        return m_localFootPoint;
    }
}

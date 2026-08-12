#pragma once

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderSortKey2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <optional>

namespace l2d
{
    // Declarative ordering metadata. It has no update or drawing behaviour.
    class RenderOrder2D final : public Component
    {
      public:
        explicit RenderOrder2D(RenderDepthMode2D depthMode = RenderDepthMode2D::Fixed);

        void setPass(RenderPass2D pass);
        RenderPass2D pass() const;

        void setLayer(std::int32_t layer);
        std::int32_t layer() const;

        void setDepthMode(RenderDepthMode2D depthMode);
        RenderDepthMode2D depthMode() const;

        // Nonfinite values are rejected without changing the current value.
        bool setExplicitDepth(float depth);
        float explicitDepth() const;

        void setOrder(std::int32_t order);
        void clearOrder();
        bool hasOrder() const;
        std::int32_t orderOr(std::int32_t fallback) const;

        // Overrides the default sprite bottom-centre or Transform position.
        bool setLocalFootPoint(sf::Vector2f localFootPoint);
        void clearLocalFootPoint();
        const std::optional<sf::Vector2f>& localFootPoint() const;

      private:
        RenderPass2D m_pass = RenderPass2D::World;
        std::int32_t m_layer = 0;
        RenderDepthMode2D m_depthMode = RenderDepthMode2D::Fixed;
        float m_explicitDepth = 0.f;
        std::optional<std::int32_t> m_order;
        std::optional<sf::Vector2f> m_localFootPoint;
    };
}

#pragma once

#include <Lorenzo2D/Renderer/CoordinateProjection2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace l2d
{
    enum class RenderPass2D
    {
        World = 0,
        PhysicsDebug,
        UI,

        Count
    };

    struct RenderLayerRange2D
    {
        std::int32_t minimum = std::numeric_limits<std::int32_t>::min();
        std::int32_t maximum = std::numeric_limits<std::int32_t>::max();

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return minimum <= maximum;
        }

        [[nodiscard]] constexpr bool contains(std::int32_t layer) const noexcept
        {
            return isValid() && layer >= minimum && layer <= maximum;
        }
    };

    struct RenderContext2D
    {
        float interpolationAlpha = 1.f;
        const CoordinateProjection2D* projection = nullptr;
        RenderPass2D pass = RenderPass2D::World;
        RenderLayerRange2D layers;

        [[nodiscard]] std::optional<sf::Vector2f> tryWorldToRender(sf::Vector2f position) const
        {
            if (!isFinite(position)) return std::nullopt;
            if (pass == RenderPass2D::UI || projection == nullptr) return position;

            const sf::Vector2f projected = projection->worldToRender(position);
            return isFinite(projected) ? std::optional<sf::Vector2f>(projected) : std::nullopt;
        }

        [[nodiscard]] std::optional<sf::Vector2f> tryRenderToWorld(sf::Vector2f position) const
        {
            if (!isFinite(position)) return std::nullopt;
            if (pass == RenderPass2D::UI || projection == nullptr) return position;

            const sf::Vector2f world = projection->renderToWorld(position);
            return isFinite(world) ? std::optional<sf::Vector2f>(world) : std::nullopt;
        }

        [[nodiscard]] std::optional<float> tryDepthFor(sf::Vector2f worldFootPoint) const
        {
            if (!isFinite(worldFootPoint)) return std::nullopt;
            if (pass == RenderPass2D::UI || projection == nullptr) return worldFootPoint.y;

            const float depth = projection->depthFor(worldFootPoint);
            return std::isfinite(depth) ? std::optional<float>(depth) : std::nullopt;
        }

        // Compatibility adapters preserve the pre-1.1 fallback behavior.
        // New code that must distinguish projection failure should use try*().
        sf::Vector2f worldToRender(sf::Vector2f position) const
        {
            const auto projected = tryWorldToRender(position);
            return projected ? *projected : position;
        }

        sf::Vector2f renderToWorld(sf::Vector2f position) const
        {
            const auto world = tryRenderToWorld(position);
            return world ? *world : position;
        }

        float depthFor(sf::Vector2f worldFootPoint) const
        {
            if (pass == RenderPass2D::UI || projection == nullptr) return worldFootPoint.y;

            const auto depth = tryDepthFor(worldFootPoint);
            return depth ? *depth : 0.f;
        }

      private:
        static bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
    };
}

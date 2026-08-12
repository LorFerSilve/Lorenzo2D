#pragma once

#include <Lorenzo2D/Renderer/CoordinateProjection2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cmath>

namespace l2d
{
    enum class RenderPass2D
    {
        World = 0,
        PhysicsDebug,
        UI,

        Count
    };

    struct RenderContext2D
    {
        float interpolationAlpha = 1.f;
        const CoordinateProjection2D* projection = nullptr;
        RenderPass2D pass = RenderPass2D::World;

        sf::Vector2f worldToRender(sf::Vector2f position) const
        {
            if (pass == RenderPass2D::UI || projection == nullptr) return position;

            const sf::Vector2f projected = projection->worldToRender(position);
            return isFinite(projected) ? projected : position;
        }

        sf::Vector2f renderToWorld(sf::Vector2f position) const
        {
            if (pass == RenderPass2D::UI || projection == nullptr) return position;

            const sf::Vector2f world = projection->renderToWorld(position);
            return isFinite(world) ? world : position;
        }

        float depthFor(sf::Vector2f worldFootPoint) const
        {
            if (pass == RenderPass2D::UI || projection == nullptr) return worldFootPoint.y;

            const float depth = projection->depthFor(worldFootPoint);
            return std::isfinite(depth) ? depth : 0.f;
        }

      private:
        static bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
    };
}

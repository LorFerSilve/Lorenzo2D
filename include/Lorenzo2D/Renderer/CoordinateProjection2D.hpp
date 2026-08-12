#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class CoordinateProjection2D
    {
      public:
        virtual ~CoordinateProjection2D() = default;

        virtual sf::Vector2f worldToRender(sf::Vector2f worldPosition) const = 0;
        virtual sf::Vector2f renderToWorld(sf::Vector2f renderPosition) const = 0;
        virtual float depthFor(sf::Vector2f worldFootPoint) const = 0;

        // Identity projections can keep already-built world-space geometry.
        virtual bool isIdentity() const
        {
            return false;
        }
    };

    class OrthogonalProjection2D final : public CoordinateProjection2D
    {
      public:
        sf::Vector2f worldToRender(sf::Vector2f worldPosition) const override
        {
            return worldPosition;
        }

        sf::Vector2f renderToWorld(sf::Vector2f renderPosition) const override
        {
            return renderPosition;
        }

        float depthFor(sf::Vector2f worldFootPoint) const override
        {
            return worldFootPoint.y;
        }

        bool isIdentity() const override
        {
            return true;
        }
    };
}

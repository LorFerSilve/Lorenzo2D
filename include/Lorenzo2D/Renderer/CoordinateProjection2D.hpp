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

        // Projects an axis-aligned world rectangle into a conservative
        // axis-aligned render-space rectangle. Returns false when the
        // projection cannot provide a safe bound; callers must then avoid
        // projected culling rather than risk dropping visible geometry.
        virtual bool projectBounds(sf::Vector2f worldMinimum, sf::Vector2f worldMaximum,
                                   sf::Vector2f& renderMinimum,
                                   sf::Vector2f& renderMaximum) const
        {
            (void)worldMinimum;
            (void)worldMaximum;
            (void)renderMinimum;
            (void)renderMaximum;
            return false;
        }

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

        bool projectBounds(sf::Vector2f worldMinimum, sf::Vector2f worldMaximum,
                           sf::Vector2f& renderMinimum,
                           sf::Vector2f& renderMaximum) const override
        {
            renderMinimum = worldMinimum;
            renderMaximum = worldMaximum;
            return true;
        }

        bool isIdentity() const override
        {
            return true;
        }
    };
}

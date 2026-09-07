#pragma once

#include <Lorenzo2D/Renderer/CoordinateProjection2D.hpp>

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    struct IsometricProjectionConfig2D
    {
        // Logical Cartesian size of one gameplay/grid cell.
        sf::Vector2f worldCellSize = {1.f, 1.f};
        // Render-space width/height of one projected diamond.
        sf::Vector2f renderTileSize = {2.f, 1.f};
        // Render-space location of logical world origin (0, 0).
        sf::Vector2f renderOrigin = {0.f, 0.f};
    };

    class IsometricProjection2D final : public CoordinateProjection2D
    {
      public:
        static constexpr float MinimumDimension = 0.0001f;

        IsometricProjection2D() = default;
        explicit IsometricProjection2D(IsometricProjectionConfig2D config);

        static bool isValidConfig(const IsometricProjectionConfig2D& config);
        // Invalid configurations are rejected without changing the active one.
        bool setConfig(IsometricProjectionConfig2D config);
        const IsometricProjectionConfig2D& config() const;

        sf::Vector2f worldToRender(sf::Vector2f worldPosition) const override;
        sf::Vector2f renderToWorld(sf::Vector2f renderPosition) const override;
        float depthFor(sf::Vector2f worldFootPoint) const override;
        bool projectBounds(sf::Vector2f worldMinimum, sf::Vector2f worldMaximum,
                           sf::Vector2f& renderMinimum,
                           sf::Vector2f& renderMaximum) const override;

      private:
        IsometricProjectionConfig2D m_config;
    };
}

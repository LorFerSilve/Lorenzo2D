#include <Lorenzo2D/Isometric/IsometricProjection2D.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        sf::Vector2f safeVector(double x, double y)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());
            if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > maximum ||
                std::fabs(y) > maximum)
            {
                const float invalid = std::numeric_limits<float>::quiet_NaN();
                return {invalid, invalid};
            }

            return {static_cast<float>(x), static_cast<float>(y)};
        }
    }

    IsometricProjection2D::IsometricProjection2D(IsometricProjectionConfig2D config)
    {
        (void)setConfig(config);
    }

    bool IsometricProjection2D::isValidConfig(const IsometricProjectionConfig2D& config)
    {
        return finite(config.worldCellSize) && finite(config.renderTileSize) &&
               finite(config.renderOrigin) &&
               config.worldCellSize.x >= MinimumDimension &&
               config.worldCellSize.y >= MinimumDimension &&
               config.renderTileSize.x >= MinimumDimension &&
               config.renderTileSize.y >= MinimumDimension;
    }

    bool IsometricProjection2D::setConfig(IsometricProjectionConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        m_config = config;
        return true;
    }

    const IsometricProjectionConfig2D& IsometricProjection2D::config() const
    {
        return m_config;
    }

    sf::Vector2f IsometricProjection2D::worldToRender(sf::Vector2f worldPosition) const
    {
        if (!finite(worldPosition)) return safeVector(std::numeric_limits<double>::quiet_NaN(), 0.0);

        const double gridX =
            static_cast<double>(worldPosition.x) / static_cast<double>(m_config.worldCellSize.x);
        const double gridY =
            static_cast<double>(worldPosition.y) / static_cast<double>(m_config.worldCellSize.y);
        const double halfWidth = static_cast<double>(m_config.renderTileSize.x) * 0.5;
        const double halfHeight = static_cast<double>(m_config.renderTileSize.y) * 0.5;
        const double renderX = static_cast<double>(m_config.renderOrigin.x) +
                               (gridX - gridY) * halfWidth;
        const double renderY = static_cast<double>(m_config.renderOrigin.y) +
                               (gridX + gridY) * halfHeight;
        return safeVector(renderX, renderY);
    }

    sf::Vector2f IsometricProjection2D::renderToWorld(sf::Vector2f renderPosition) const
    {
        if (!finite(renderPosition))
            return safeVector(std::numeric_limits<double>::quiet_NaN(), 0.0);

        const double halfWidth = static_cast<double>(m_config.renderTileSize.x) * 0.5;
        const double halfHeight = static_cast<double>(m_config.renderTileSize.y) * 0.5;
        const double normalizedX =
            (static_cast<double>(renderPosition.x) - m_config.renderOrigin.x) / halfWidth;
        const double normalizedY =
            (static_cast<double>(renderPosition.y) - m_config.renderOrigin.y) / halfHeight;
        const double gridX = (normalizedX + normalizedY) * 0.5;
        const double gridY = (normalizedY - normalizedX) * 0.5;

        return safeVector(gridX * static_cast<double>(m_config.worldCellSize.x),
                          gridY * static_cast<double>(m_config.worldCellSize.y));
    }

    float IsometricProjection2D::depthFor(sf::Vector2f worldFootPoint) const
    {
        const sf::Vector2f projected = worldToRender(worldFootPoint);
        return std::isfinite(projected.y) ? projected.y : 0.f;
    }

    bool IsometricProjection2D::projectBounds(sf::Vector2f worldMinimum,
                                              sf::Vector2f worldMaximum,
                                              sf::Vector2f& renderMinimum,
                                              sf::Vector2f& renderMaximum) const
    {
        if (!finite(worldMinimum) || !finite(worldMaximum) ||
            worldMinimum.x > worldMaximum.x || worldMinimum.y > worldMaximum.y)
        {
            return false;
        }

        const std::array<sf::Vector2f, 4u> corners = {
            worldToRender(worldMinimum),
            worldToRender({worldMaximum.x, worldMinimum.y}),
            worldToRender(worldMaximum),
            worldToRender({worldMinimum.x, worldMaximum.y})};

        for (const sf::Vector2f corner : corners)
            if (!finite(corner)) return false;

        renderMinimum = corners[0u];
        renderMaximum = corners[0u];
        for (std::size_t index = 1u; index < corners.size(); ++index)
        {
            renderMinimum.x = std::min(renderMinimum.x, corners[index].x);
            renderMinimum.y = std::min(renderMinimum.y, corners[index].y);
            renderMaximum.x = std::max(renderMaximum.x, corners[index].x);
            renderMaximum.y = std::max(renderMaximum.y, corners[index].y);
        }

        return true;
    }
}

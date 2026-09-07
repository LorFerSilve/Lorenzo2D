#pragma once

#include <Lorenzo2D/Renderer/CoordinateProjection2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace l2d
{
    // Affine diamond-isometric projection over a Cartesian logical world.
    // worldCellSize controls the logical cell dimensions; renderCellSize
    // controls the projected diamond width/height. Physics and navigation stay
    // in world coordinates while rendering, picking and placement use this
    // reversible presentation transform.
    class IsometricProjection2D final : public CoordinateProjection2D
    {
      public:
        explicit IsometricProjection2D(sf::Vector2f worldCellSize = {1.f, 1.f},
                                       sf::Vector2f renderCellSize = {2.f, 1.f},
                                       sf::Vector2f renderOrigin = {0.f, 0.f})
            : m_worldCellSize(worldCellSize), m_renderCellSize(renderCellSize),
              m_renderOrigin(renderOrigin)
        {
            if (!isValidCellSize(worldCellSize) || !isValidCellSize(renderCellSize) ||
                !isFinite(renderOrigin))
            {
                throw std::invalid_argument("Isometric projection parameters must be finite and positive.");
            }
        }

        static bool isValidCellSize(sf::Vector2f size)
        {
            return isFinite(size) && size.x > 0.f && size.y > 0.f;
        }

        sf::Vector2f worldCellSize() const
        {
            return m_worldCellSize;
        }

        sf::Vector2f renderCellSize() const
        {
            return m_renderCellSize;
        }

        sf::Vector2f renderOrigin() const
        {
            return m_renderOrigin;
        }

        sf::Vector2f worldToRender(sf::Vector2f worldPosition) const override
        {
            if (!isFinite(worldPosition)) return invalidVector();

            const double column = static_cast<double>(worldPosition.x) / m_worldCellSize.x;
            const double row = static_cast<double>(worldPosition.y) / m_worldCellSize.y;
            const double halfWidth = static_cast<double>(m_renderCellSize.x) * 0.5;
            const double halfHeight = static_cast<double>(m_renderCellSize.y) * 0.5;
            return checkedVector(static_cast<double>(m_renderOrigin.x) + (column - row) * halfWidth,
                                 static_cast<double>(m_renderOrigin.y) + (column + row) * halfHeight);
        }

        sf::Vector2f renderToWorld(sf::Vector2f renderPosition) const override
        {
            if (!isFinite(renderPosition)) return invalidVector();

            const double horizontal =
                (static_cast<double>(renderPosition.x) - m_renderOrigin.x) /
                (static_cast<double>(m_renderCellSize.x) * 0.5);
            const double vertical =
                (static_cast<double>(renderPosition.y) - m_renderOrigin.y) /
                (static_cast<double>(m_renderCellSize.y) * 0.5);
            const double column = (horizontal + vertical) * 0.5;
            const double row = (vertical - horizontal) * 0.5;
            return checkedVector(column * m_worldCellSize.x, row * m_worldCellSize.y);
        }

        float depthFor(sf::Vector2f worldFootPoint) const override
        {
            const sf::Vector2f projected = worldToRender(worldFootPoint);
            return std::isfinite(projected.y) ? projected.y : 0.f;
        }

        bool projectBounds(sf::Vector2f worldMinimum, sf::Vector2f worldMaximum,
                           sf::Vector2f& renderMinimum,
                           sf::Vector2f& renderMaximum) const override
        {
            if (!isFinite(worldMinimum) || !isFinite(worldMaximum) ||
                worldMaximum.x < worldMinimum.x || worldMaximum.y < worldMinimum.y)
            {
                return false;
            }

            const sf::Vector2f corners[4] = {
                worldToRender(worldMinimum),
                worldToRender({worldMinimum.x, worldMaximum.y}),
                worldToRender(worldMaximum),
                worldToRender({worldMaximum.x, worldMinimum.y})};

            for (const sf::Vector2f corner : corners)
                if (!isFinite(corner)) return false;

            renderMinimum = corners[0];
            renderMaximum = corners[0];
            for (std::size_t index = 1u; index < 4u; ++index)
            {
                renderMinimum.x = std::min(renderMinimum.x, corners[index].x);
                renderMinimum.y = std::min(renderMinimum.y, corners[index].y);
                renderMaximum.x = std::max(renderMaximum.x, corners[index].x);
                renderMaximum.y = std::max(renderMaximum.y, corners[index].y);
            }
            return true;
        }

        sf::Vector2f cellToWorld(sf::Vector2i cell, bool centered = true) const
        {
            const double offset = centered ? 0.5 : 0.0;
            return checkedVector((static_cast<double>(cell.x) + offset) * m_worldCellSize.x,
                                 (static_cast<double>(cell.y) + offset) * m_worldCellSize.y);
        }

        sf::Vector2f cellToRender(sf::Vector2i cell, bool centered = true) const
        {
            return worldToRender(cellToWorld(cell, centered));
        }

        std::optional<sf::Vector2i> worldToCell(sf::Vector2f worldPosition) const
        {
            if (!isFinite(worldPosition)) return std::nullopt;
            const double column = std::floor(static_cast<double>(worldPosition.x) / m_worldCellSize.x);
            const double row = std::floor(static_cast<double>(worldPosition.y) / m_worldCellSize.y);
            const double minimum = static_cast<double>(std::numeric_limits<int>::min());
            const double maximum = static_cast<double>(std::numeric_limits<int>::max());
            if (column < minimum || column > maximum || row < minimum || row > maximum)
                return std::nullopt;
            return sf::Vector2i{static_cast<int>(column), static_cast<int>(row)};
        }

        std::optional<sf::Vector2i> renderToCell(sf::Vector2f renderPosition) const
        {
            return worldToCell(renderToWorld(renderPosition));
        }

      private:
        static bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        static sf::Vector2f invalidVector()
        {
            const float invalid = std::numeric_limits<float>::quiet_NaN();
            return {invalid, invalid};
        }

        static sf::Vector2f checkedVector(double x, double y)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());
            if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > maximum ||
                std::fabs(y) > maximum)
                return invalidVector();
            return {static_cast<float>(x), static_cast<float>(y)};
        }

      private:
        sf::Vector2f m_worldCellSize;
        sf::Vector2f m_renderCellSize;
        sf::Vector2f m_renderOrigin;
    };
}

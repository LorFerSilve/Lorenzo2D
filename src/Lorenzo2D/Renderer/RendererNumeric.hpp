#pragma once

#include <Lorenzo2D/ECS/Transform.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cmath>
#include <limits>

namespace l2d::renderer_detail
{
    inline float maximumSafeViewExtent()
    {
        static const float value = std::sqrt(std::numeric_limits<float>::max()) / 4.f;

        return value;
    }

    inline bool isFinite(sf::Vector2f value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    inline bool isSafeCameraCoordinate(float value)
    {
        return std::isfinite(value) && std::abs(value) <= maximumSafeViewExtent();
    }

    inline bool isSafeCameraPosition(sf::Vector2f value)
    {
        return isSafeCameraCoordinate(value.x) && isSafeCameraCoordinate(value.y);
    }

    inline bool isSafeDrawableCoordinate(double value)
    {
        return std::isfinite(value) &&
               std::abs(value) <= static_cast<double>(maximumSafeViewExtent());
    }

    inline bool isSafeDrawablePosition(sf::Vector2f value)
    {
        return isSafeDrawableCoordinate(static_cast<double>(value.x)) &&
               isSafeDrawableCoordinate(static_cast<double>(value.y));
    }

    inline bool hasSafeAxisAlignedBounds(sf::Vector2f position, sf::Vector2f size,
                                         float padding = 0.f)
    {
        if (!isSafeDrawablePosition(position) || !isFinite(size) || size.x < 0.f || size.y < 0.f ||
            !std::isfinite(padding) || padding < 0.f || !isSafeDrawableCoordinate(size.x) ||
            !isSafeDrawableCoordinate(size.y) || !isSafeDrawableCoordinate(padding))
        {
            return false;
        }

        const double widenedPadding = static_cast<double>(padding);
        const double minimumX = static_cast<double>(position.x) - widenedPadding;
        const double minimumY = static_cast<double>(position.y) - widenedPadding;
        const double maximumX =
            static_cast<double>(position.x) + static_cast<double>(size.x) + widenedPadding;
        const double maximumY =
            static_cast<double>(position.y) + static_cast<double>(size.y) + widenedPadding;

        return isSafeDrawableCoordinate(minimumX) && isSafeDrawableCoordinate(minimumY) &&
               isSafeDrawableCoordinate(maximumX) && isSafeDrawableCoordinate(maximumY);
    }

    inline float sanitizeNonNegative(float value)
    {
        if (!std::isfinite(value) || value < 0.f) return 0.f;

        return value;
    }

    inline bool isFinite(const TransformState& state)
    {
        return isFinite(state.position) && std::isfinite(state.rotation) && isFinite(state.scale);
    }

    inline float normalizedRotationDegrees(float rotation)
    {
        if (!std::isfinite(rotation)) return 0.f;

        return static_cast<float>(std::remainder(static_cast<double>(rotation), 360.0));
    }

    inline bool hasSafeTransformedBounds(const sf::FloatRect& localBounds,
                                         const TransformState& state)
    {
        if (!isFinite(state) || !isSafeDrawablePosition(state.position)) return false;

        const double left = static_cast<double>(localBounds.position.x);
        const double top = static_cast<double>(localBounds.position.y);
        const double right = left + static_cast<double>(localBounds.size.x);
        const double bottom = top + static_cast<double>(localBounds.size.y);

        if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) ||
            !std::isfinite(bottom))
        {
            return false;
        }

        constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
        const double angle =
            static_cast<double>(normalizedRotationDegrees(state.rotation)) * degreesToRadians;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double xCoordinates[] = {left, right};
        const double yCoordinates[] = {top, bottom};

        for (const double x : xCoordinates)
        {
            for (const double y : yCoordinates)
            {
                const double scaledX = x * static_cast<double>(state.scale.x);
                const double scaledY = y * static_cast<double>(state.scale.y);

                if (!isSafeDrawableCoordinate(scaledX) || !isSafeDrawableCoordinate(scaledY))
                {
                    return false;
                }

                const double transformedX =
                    cosine * scaledX - sine * scaledY + static_cast<double>(state.position.x);
                const double transformedY =
                    sine * scaledX + cosine * scaledY + static_cast<double>(state.position.y);

                if (!isSafeDrawableCoordinate(transformedX) ||
                    !isSafeDrawableCoordinate(transformedY))
                {
                    return false;
                }
            }
        }

        return true;
    }
}

#include <Lorenzo2D/Renderer/Camera2D.hpp>

#include "RendererNumeric.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    namespace
    {
        constexpr float MINIMUM_CAMERA_SIZE = 0.01f;
        constexpr float MINIMUM_CAMERA_ZOOM = 0.01f;

        float maximumBaseExtent(float zoom)
        {
            return static_cast<float>(
                static_cast<double>(renderer_detail::maximumSafeViewExtent()) /
                static_cast<double>(zoom));
        }

        float sanitizeBaseExtent(float extent, float zoom)
        {
            const float maximum = maximumBaseExtent(zoom);

            if (!std::isfinite(extent) || extent < MINIMUM_CAMERA_SIZE) return MINIMUM_CAMERA_SIZE;

            return std::min(extent, maximum);
        }

        sf::Vector2f sanitizeBaseSize(sf::Vector2f size, float zoom)
        {
            return {sanitizeBaseExtent(size.x, zoom), sanitizeBaseExtent(size.y, zoom)};
        }

        float maximumZoomForSize(sf::Vector2f size)
        {
            const double maximumExtent =
                static_cast<double>(renderer_detail::maximumSafeViewExtent());
            const double maximumZoom = std::min(maximumExtent / static_cast<double>(size.x),
                                                maximumExtent / static_cast<double>(size.y));

            return static_cast<float>(maximumZoom);
        }

        float sanitizeZoom(float zoom, sf::Vector2f size)
        {
            if (!std::isfinite(zoom) || zoom < MINIMUM_CAMERA_ZOOM) return MINIMUM_CAMERA_ZOOM;

            return std::min(zoom, maximumZoomForSize(size));
        }

        float safeViewExtent(float baseExtent, float zoom)
        {
            const double extent = static_cast<double>(baseExtent) * static_cast<double>(zoom);
            const double maximum = static_cast<double>(renderer_detail::maximumSafeViewExtent());

            return static_cast<float>(std::min(extent, maximum));
        }

        bool checkedCameraSum(float left, float right, float& result)
        {
            if (!std::isfinite(left) || !std::isfinite(right)) return false;

            const double sum = static_cast<double>(left) + static_cast<double>(right);

            if (!std::isfinite(sum) ||
                std::abs(sum) > static_cast<double>(renderer_detail::maximumSafeViewExtent()))
            {
                return false;
            }

            result = static_cast<float>(sum);
            return true;
        }

        float midpoint(float minimum, float maximum)
        {
            return static_cast<float>(static_cast<double>(minimum) * 0.5 +
                                      static_cast<double>(maximum) * 0.5);
        }
    }

    Camera2D::Camera2D(sf::Vector2f size)
        : m_center(0.f, 0.f), m_baseSize(sanitizeBaseSize(size, 1.f)), m_zoom(1.f),
          m_followSmoothness(6.f), m_hasBounds(false), m_boundsMin(0.f, 0.f),
          m_boundsMax(m_baseSize)
    {
        m_center = m_baseSize * 0.5f;
        m_view.setCenter(m_center);
        updateViewSize();
    }

    void Camera2D::setCenter(sf::Vector2f center)
    {
        if (!renderer_detail::isSafeCameraPosition(center)) return;

        m_center = clampedCenter(center);
        m_view.setCenter(m_center);
    }

    const sf::Vector2f& Camera2D::center() const
    {
        return m_center;
    }

    void Camera2D::move(sf::Vector2f offset)
    {
        sf::Vector2f nextCenter;

        if (!checkedCameraSum(m_center.x, offset.x, nextCenter.x) ||
            !checkedCameraSum(m_center.y, offset.y, nextCenter.y))
        {
            return;
        }

        setCenter(nextCenter);
    }

    void Camera2D::setSize(sf::Vector2f size)
    {
        m_baseSize = sanitizeBaseSize(size, m_zoom);

        updateViewSize();
        setCenter(m_center);
    }

    const sf::Vector2f& Camera2D::size() const
    {
        return m_baseSize;
    }

    void Camera2D::setZoom(float zoom)
    {
        m_zoom = sanitizeZoom(zoom, m_baseSize);

        updateViewSize();
        setCenter(m_center);
    }

    float Camera2D::zoom() const
    {
        return m_zoom;
    }

    void Camera2D::setFollowSmoothness(float smoothness)
    {
        if (!std::isfinite(smoothness) || smoothness < 0.f) smoothness = 0.f;

        m_followSmoothness = smoothness;
    }

    float Camera2D::followSmoothness() const
    {
        return m_followSmoothness;
    }

    void Camera2D::follow(sf::Vector2f target, float deltaTime)
    {
        if (!renderer_detail::isSafeCameraPosition(target) || !std::isfinite(deltaTime) ||
            deltaTime <= 0.f)
        {
            return;
        }

        if (m_followSmoothness <= 0.f)
        {
            setCenter(target);
            return;
        }

        const double exponent =
            static_cast<double>(m_followSmoothness) * static_cast<double>(deltaTime);
        const double t = std::clamp(-std::expm1(-exponent), 0.0, 1.0);

        const sf::Vector2f newCenter = {
            static_cast<float>(static_cast<double>(m_center.x) +
                               (static_cast<double>(target.x) - static_cast<double>(m_center.x)) *
                                   t),
            static_cast<float>(static_cast<double>(m_center.y) +
                               (static_cast<double>(target.y) - static_cast<double>(m_center.y)) *
                                   t)};

        setCenter(newCenter);
    }

    void Camera2D::setBounds(sf::Vector2f min, sf::Vector2f max)
    {
        if (!renderer_detail::isSafeCameraPosition(min) ||
            !renderer_detail::isSafeCameraPosition(max))
        {
            return;
        }

        m_boundsMin = {std::min(min.x, max.x), std::min(min.y, max.y)};

        m_boundsMax = {std::max(min.x, max.x), std::max(min.y, max.y)};

        m_hasBounds = true;

        setCenter(m_center);
    }

    void Camera2D::clearBounds()
    {
        m_hasBounds = false;
    }

    bool Camera2D::hasBounds() const
    {
        return m_hasBounds;
    }

    const sf::Vector2f& Camera2D::boundsMin() const
    {
        return m_boundsMin;
    }

    const sf::Vector2f& Camera2D::boundsMax() const
    {
        return m_boundsMax;
    }

    void Camera2D::applyTo(sf::RenderWindow& window) const
    {
        window.setView(m_view);
    }

    const sf::View& Camera2D::view() const
    {
        return m_view;
    }

    void Camera2D::updateViewSize()
    {
        m_view.setSize(
            {safeViewExtent(m_baseSize.x, m_zoom), safeViewExtent(m_baseSize.y, m_zoom)});
    }

    sf::Vector2f Camera2D::clampedCenter(sf::Vector2f center) const
    {
        if (!m_hasBounds) return center;

        const sf::Vector2f viewSize = m_view.getSize();
        const sf::Vector2f halfViewSize = viewSize * 0.5f;

        const float minCenterX = m_boundsMin.x + halfViewSize.x;
        const float maxCenterX = m_boundsMax.x - halfViewSize.x;

        const float minCenterY = m_boundsMin.y + halfViewSize.y;
        const float maxCenterY = m_boundsMax.y - halfViewSize.y;

        if (minCenterX > maxCenterX)
            center.x = midpoint(m_boundsMin.x, m_boundsMax.x);
        else
            center.x = std::clamp(center.x, minCenterX, maxCenterX);

        if (minCenterY > maxCenterY)
            center.y = midpoint(m_boundsMin.y, m_boundsMax.y);
        else
            center.y = std::clamp(center.y, minCenterY, maxCenterY);

        return center;
    }
}

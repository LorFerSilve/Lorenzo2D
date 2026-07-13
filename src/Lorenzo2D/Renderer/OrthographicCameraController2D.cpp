#include <Lorenzo2D/Renderer/OrthographicCameraController2D.hpp>

#include <Lorenzo2D/Core/WindowEvents.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>

#include "RendererNumeric.hpp"

#include <algorithm>
#include <cmath>

namespace l2d
{
    namespace
    {
        constexpr float MINIMUM_ZOOM = 0.01f;
        constexpr float DEFAULT_ZOOM_IN_FACTOR = 0.90f;
        constexpr float DEFAULT_ZOOM_OUT_FACTOR = 1.10f;

        float sanitizeZoomLimit(float zoom)
        {
            if (!std::isfinite(zoom) || zoom < MINIMUM_ZOOM)
                return MINIMUM_ZOOM;

            return zoom;
        }

        float maximumSupportedZoom(const Camera2D& camera)
        {
            const sf::Vector2f size = camera.size();
            const double maximumExtent = static_cast<double>(
                renderer_detail::maximumSafeViewExtent()
            );
            const double maximumZoom = std::min(
                maximumExtent / static_cast<double>(size.x),
                maximumExtent / static_cast<double>(size.y)
            );

            return std::max(
                MINIMUM_ZOOM,
                static_cast<float>(maximumZoom)
            );
        }
    }

    OrthographicCameraController2D::OrthographicCameraController2D(Camera2D& camera)
        : m_camera(&camera),
        m_followEnabled(true),
        m_hasFollowTarget(false),
        m_followTarget(0.f, 0.f),
        m_zoomEnabled(true),
        m_minZoom(0.5f),
        m_maxZoom(2.f),
        m_zoomInFactor(0.90f),
        m_zoomOutFactor(1.10f),
        m_resizeEnabled(true)
    {
    }

    void OrthographicCameraController2D::setFollowEnabled(bool enabled)
    {
        m_followEnabled = enabled;
    }

    bool OrthographicCameraController2D::isFollowEnabled() const
    {
        return m_followEnabled;
    }

    void OrthographicCameraController2D::setFollowTarget(sf::Vector2f target)
    {
        if (!renderer_detail::isSafeCameraPosition(target))
            return;

        m_followTarget = target;
        m_hasFollowTarget = true;
    }

    void OrthographicCameraController2D::clearFollowTarget()
    {
        m_hasFollowTarget = false;
    }

    bool OrthographicCameraController2D::hasFollowTarget() const
    {
        return m_hasFollowTarget;
    }

    void OrthographicCameraController2D::setZoomEnabled(bool enabled)
    {
        m_zoomEnabled = enabled;
    }

    bool OrthographicCameraController2D::isZoomEnabled() const
    {
        return m_zoomEnabled;
    }

    void OrthographicCameraController2D::setZoomLimits(float minZoom, float maxZoom)
    {
        minZoom = sanitizeZoomLimit(minZoom);
        maxZoom = sanitizeZoomLimit(maxZoom);

        m_minZoom = std::min(minZoom, maxZoom);
        m_maxZoom = std::max(minZoom, maxZoom);

        m_camera->setZoom(std::clamp(
            m_camera->zoom(),
            this->minZoom(),
            this->maxZoom()
        ));
    }

    float OrthographicCameraController2D::minZoom() const
    {
        return std::min(m_minZoom, maximumSupportedZoom(*m_camera));
    }

    float OrthographicCameraController2D::maxZoom() const
    {
        return std::max(
            minZoom(),
            std::min(m_maxZoom, maximumSupportedZoom(*m_camera))
        );
    }

    void OrthographicCameraController2D::setZoomStepFactors(
        float zoomInFactor,
        float zoomOutFactor
    )
    {
        if (!std::isfinite(zoomInFactor) || zoomInFactor <= 0.f)
            zoomInFactor = DEFAULT_ZOOM_IN_FACTOR;

        if (!std::isfinite(zoomOutFactor) || zoomOutFactor <= 0.f)
            zoomOutFactor = DEFAULT_ZOOM_OUT_FACTOR;

        m_zoomInFactor = zoomInFactor;
        m_zoomOutFactor = zoomOutFactor;
    }

    float OrthographicCameraController2D::zoomInFactor() const
    {
        return m_zoomInFactor;
    }

    float OrthographicCameraController2D::zoomOutFactor() const
    {
        return m_zoomOutFactor;
    }

    void OrthographicCameraController2D::setResizeEnabled(bool enabled)
    {
        m_resizeEnabled = enabled;
    }

    bool OrthographicCameraController2D::isResizeEnabled() const
    {
        return m_resizeEnabled;
    }

    void OrthographicCameraController2D::update(float deltaTime)
    {
        updateResize();
        updateFollow(deltaTime);
        updateZoom();
    }

    void OrthographicCameraController2D::updateResize()
    {
        if (m_camera == nullptr)
            return;

        if (!m_resizeEnabled)
            return;

        if (!WindowEvents::wasResized())
            return;

        const sf::Vector2u newSize = WindowEvents::resizedSize();

        if (newSize.x == 0 || newSize.y == 0)
            return;

        m_camera->setSize(
            {
                static_cast<float>(newSize.x),
                static_cast<float>(newSize.y)
            }
        );

    }

    void OrthographicCameraController2D::updateFollow(float deltaTime)
    {
        if (m_camera == nullptr)
            return;

        if (!m_followEnabled)
            return;

        if (!m_hasFollowTarget)
            return;

        m_camera->follow(m_followTarget, deltaTime);
    }

    void OrthographicCameraController2D::updateZoom()
    {
        if (m_camera == nullptr)
            return;

        if (!m_zoomEnabled)
            return;

        if (!WindowEvents::mouseWheelScrolled())
            return;

        if (WindowEvents::mouseWheel() != MouseWheel::Vertical)
            return;

        const float wheelDelta = WindowEvents::mouseWheelDelta();

        if (!std::isfinite(wheelDelta))
            return;

        double zoom = static_cast<double>(m_camera->zoom());

        if (wheelDelta > 0.f)
        {
            zoom *= static_cast<double>(m_zoomInFactor);
        }
        else if (wheelDelta < 0.f)
        {
            zoom *= static_cast<double>(m_zoomOutFactor);
        }

        zoom = std::clamp(
            zoom,
            static_cast<double>(minZoom()),
            static_cast<double>(maxZoom())
        );

        m_camera->setZoom(static_cast<float>(zoom));
    }
}

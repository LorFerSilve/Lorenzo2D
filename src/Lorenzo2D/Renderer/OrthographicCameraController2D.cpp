#include <Lorenzo2D/Renderer/OrthographicCameraController2D.hpp>

#include <Lorenzo2D/Core/WindowEvents.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>

#include <algorithm>

namespace l2d
{
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
        if (minZoom <= 0.f)
            minZoom = 0.01f;

        if (maxZoom <= 0.f)
            maxZoom = 0.01f;

        m_minZoom = std::min(minZoom, maxZoom);
        m_maxZoom = std::max(minZoom, maxZoom);

        if (m_camera != nullptr)
        {
            const float clampedZoom =
                std::clamp(m_camera->zoom(), m_minZoom, m_maxZoom);

            m_camera->setZoom(clampedZoom);
        }
    }

    float OrthographicCameraController2D::minZoom() const
    {
        return m_minZoom;
    }

    float OrthographicCameraController2D::maxZoom() const
    {
        return m_maxZoom;
    }

    void OrthographicCameraController2D::setZoomStepFactors(
        float zoomInFactor,
        float zoomOutFactor
    )
    {
        if (zoomInFactor <= 0.f)
            zoomInFactor = 0.90f;

        if (zoomOutFactor <= 0.f)
            zoomOutFactor = 1.10f;

        m_zoomInFactor = zoomInFactor;
        m_zoomOutFactor = zoomOutFactor;
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

        m_camera->setCenter(m_camera->center());
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

        float zoom = m_camera->zoom();

        if (WindowEvents::mouseWheelDelta() > 0.f)
        {
            zoom *= m_zoomInFactor;
        }
        else if (WindowEvents::mouseWheelDelta() < 0.f)
        {
            zoom *= m_zoomOutFactor;
        }

        zoom = std::clamp(zoom, m_minZoom, m_maxZoom);

        m_camera->setZoom(zoom);
    }
}
#include <Lorenzo2D/Renderer/Camera2D.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    Camera2D::Camera2D(sf::Vector2f size)
        : m_center(size.x * 0.5f, size.y * 0.5f),
        m_baseSize(size),
        m_zoom(1.f),
        m_followSmoothness(6.f),
        m_hasBounds(false),
        m_boundsMin(0.f, 0.f),
        m_boundsMax(size)
    {
        m_view.setCenter(m_center);
        updateViewSize();
    }

    void Camera2D::setCenter(sf::Vector2f center)
    {
        m_center = clampedCenter(center);
        m_view.setCenter(m_center);
    }

    const sf::Vector2f& Camera2D::center() const
    {
        return m_center;
    }

    void Camera2D::move(sf::Vector2f offset)
    {
        setCenter(m_center + offset);
    }

    void Camera2D::setSize(sf::Vector2f size)
    {
        m_baseSize = size;

        updateViewSize();
        setCenter(m_center);
    }

    const sf::Vector2f& Camera2D::size() const
    {
        return m_baseSize;
    }

    void Camera2D::setZoom(float zoom)
    {
        if (zoom <= 0.f)
            zoom = 0.01f;

        m_zoom = zoom;

        updateViewSize();
        setCenter(m_center);
    }

    float Camera2D::zoom() const
    {
        return m_zoom;
    }

    void Camera2D::setFollowSmoothness(float smoothness)
    {
        if (smoothness < 0.f)
            smoothness = 0.f;

        m_followSmoothness = smoothness;
    }

    float Camera2D::followSmoothness() const
    {
        return m_followSmoothness;
    }

    void Camera2D::follow(sf::Vector2f target, float deltaTime)
    {
        if (m_followSmoothness <= 0.f)
        {
            setCenter(target);
            return;
        }

        const float t = 1.f - std::exp(-m_followSmoothness * deltaTime);

        const sf::Vector2f newCenter =
            m_center + (target - m_center) * t;

        setCenter(newCenter);
    }

    void Camera2D::setBounds(sf::Vector2f min, sf::Vector2f max)
    {
        m_boundsMin =
        {
            std::min(min.x, max.x),
            std::min(min.y, max.y)
        };

        m_boundsMax =
        {
            std::max(min.x, max.x),
            std::max(min.y, max.y)
        };

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
        m_view.setSize(m_baseSize * m_zoom);
    }

    sf::Vector2f Camera2D::clampedCenter(sf::Vector2f center) const
    {
        if (!m_hasBounds)
            return center;

        const sf::Vector2f viewSize = m_baseSize * m_zoom;
        const sf::Vector2f halfViewSize = viewSize * 0.5f;

        const float minCenterX = m_boundsMin.x + halfViewSize.x;
        const float maxCenterX = m_boundsMax.x - halfViewSize.x;

        const float minCenterY = m_boundsMin.y + halfViewSize.y;
        const float maxCenterY = m_boundsMax.y - halfViewSize.y;

        if (minCenterX > maxCenterX)
            center.x = (m_boundsMin.x + m_boundsMax.x) * 0.5f;
        else
            center.x = std::clamp(center.x, minCenterX, maxCenterX);

        if (minCenterY > maxCenterY)
            center.y = (m_boundsMin.y + m_boundsMax.y) * 0.5f;
        else
            center.y = std::clamp(center.y, minCenterY, maxCenterY);

        return center;
    }
}
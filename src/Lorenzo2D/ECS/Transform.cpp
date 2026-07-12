#include <Lorenzo2D/ECS/Transform.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    Transform::Transform()
        : m_current(),
        m_previous(m_current)
    {
    }

    Transform::Transform(sf::Vector2f position)
        : m_current{ position, 0.f, { 1.f, 1.f } },
        m_previous(m_current)
    {
    }

    const sf::Vector2f& Transform::position() const
    {
        return m_current.position;
    }

    void Transform::setPosition(sf::Vector2f position)
    {
        m_current.position = position;
        synchronizePreviousBeforeFirstSnapshot();
    }

    void Transform::move(sf::Vector2f offset)
    {
        m_current.position += offset;
        synchronizePreviousBeforeFirstSnapshot();
    }

    float Transform::rotation() const
    {
        return m_current.rotation;
    }

    void Transform::setRotation(float rotation)
    {
        m_current.rotation = rotation;
        synchronizePreviousBeforeFirstSnapshot();
    }

    void Transform::rotate(float angle)
    {
        m_current.rotation += angle;
        synchronizePreviousBeforeFirstSnapshot();
    }

    const sf::Vector2f& Transform::scale() const
    {
        return m_current.scale;
    }

    void Transform::setScale(sf::Vector2f scale)
    {
        m_current.scale = scale;
        synchronizePreviousBeforeFirstSnapshot();
    }

    TransformState Transform::interpolated(float alpha) const
    {
        if (!m_hasHistory)
            return m_current;

        if (std::isnan(alpha))
            alpha = 1.f;

        alpha = std::clamp(alpha, 0.f, 1.f);

        if (alpha <= 0.f)
            return m_previous;

        if (alpha >= 1.f)
            return m_current;

        const float rotationDelta = std::remainder(
            m_current.rotation - m_previous.rotation,
            360.f
        );

        return {
            m_previous.position +
                (m_current.position - m_previous.position) * alpha,
            m_previous.rotation + rotationDelta * alpha,
            m_previous.scale +
                (m_current.scale - m_previous.scale) * alpha
        };
    }

    void Transform::resetInterpolation()
    {
        m_previous = m_current;
        m_hasHistory = true;
    }

    void Transform::capturePrevious()
    {
        m_previous = m_current;
        m_hasHistory = true;
    }

    void Transform::synchronizePreviousBeforeFirstSnapshot()
    {
        if (!m_hasHistory)
            m_previous = m_current;
    }
}

#include <Lorenzo2D/ECS/Transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool checkedFloat(double value, float& result)
        {
            const double maximum = static_cast<double>(
                std::numeric_limits<float>::max()
            );

            if (!std::isfinite(value) || value < -maximum || value > maximum)
                return false;

            result = static_cast<float>(value);
            return true;
        }

        float finiteFloat(double value)
        {
            const double maximum = static_cast<double>(
                std::numeric_limits<float>::max()
            );

            value = std::clamp(value, -maximum, maximum);
            return static_cast<float>(value);
        }

        float interpolate(float previous, float current, double alpha)
        {
            return finiteFloat(
                static_cast<double>(previous) +
                (static_cast<double>(current) -
                    static_cast<double>(previous)) * alpha
            );
        }

        sf::Vector2f initialPosition(sf::Vector2f position)
        {
            if (!isFinite(position))
                return { 0.f, 0.f };

            return position;
        }
    }

    Transform::Transform()
        : m_current(),
        m_previous(m_current)
    {
    }

    Transform::Transform(sf::Vector2f position)
        : m_current{ initialPosition(position), 0.f, { 1.f, 1.f } },
        m_previous(m_current)
    {
    }

    const sf::Vector2f& Transform::position() const
    {
        return m_current.position;
    }

    void Transform::setPosition(sf::Vector2f position)
    {
        if (!isFinite(position))
            return;

        m_current.position = position;
        synchronizePreviousBeforeFirstSnapshot();
    }

    void Transform::move(sf::Vector2f offset)
    {
        if (!isFinite(offset))
            return;

        sf::Vector2f nextPosition;

        if (
            !checkedFloat(
                static_cast<double>(m_current.position.x) +
                    static_cast<double>(offset.x),
                nextPosition.x
            ) ||
            !checkedFloat(
                static_cast<double>(m_current.position.y) +
                    static_cast<double>(offset.y),
                nextPosition.y
            )
        )
        {
            return;
        }

        m_current.position = nextPosition;
        synchronizePreviousBeforeFirstSnapshot();
    }

    float Transform::rotation() const
    {
        return m_current.rotation;
    }

    void Transform::setRotation(float rotation)
    {
        if (!std::isfinite(rotation))
            return;

        m_current.rotation = rotation;
        synchronizePreviousBeforeFirstSnapshot();
    }

    void Transform::rotate(float angle)
    {
        if (!std::isfinite(angle))
            return;

        float nextRotation = 0.f;

        if (!checkedFloat(
            static_cast<double>(m_current.rotation) +
                static_cast<double>(angle),
            nextRotation
        ))
        {
            return;
        }

        m_current.rotation = nextRotation;
        synchronizePreviousBeforeFirstSnapshot();
    }

    const sf::Vector2f& Transform::scale() const
    {
        return m_current.scale;
    }

    void Transform::setScale(sf::Vector2f scale)
    {
        if (!isFinite(scale))
            return;

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

        const double interpolationAlpha = static_cast<double>(alpha);
        const double rotationDelta = std::remainder(
            static_cast<double>(m_current.rotation) -
                static_cast<double>(m_previous.rotation),
            360.0
        );

        return {
            {
                interpolate(
                    m_previous.position.x,
                    m_current.position.x,
                    interpolationAlpha
                ),
                interpolate(
                    m_previous.position.y,
                    m_current.position.y,
                    interpolationAlpha
                )
            },
            finiteFloat(
                static_cast<double>(m_previous.rotation) +
                rotationDelta * interpolationAlpha
            ),
            {
                interpolate(
                    m_previous.scale.x,
                    m_current.scale.x,
                    interpolationAlpha
                ),
                interpolate(
                    m_previous.scale.y,
                    m_current.scale.y,
                    interpolationAlpha
                )
            }
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

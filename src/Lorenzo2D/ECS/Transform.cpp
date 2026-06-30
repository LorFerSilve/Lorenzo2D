#include <Lorenzo2D/ECS/Transform.hpp>

namespace l2d
{
    Transform::Transform()
        : m_position(0.f, 0.f),
        m_rotation(0.f),
        m_scale(1.f, 1.f)
    {
    }

    Transform::Transform(sf::Vector2f position)
        : m_position(position),
        m_rotation(0.f),
        m_scale(1.f, 1.f)
    {
    }

    const sf::Vector2f& Transform::position() const
    {
        return m_position;
    }

    void Transform::setPosition(sf::Vector2f position)
    {
        m_position = position;
    }

    void Transform::move(sf::Vector2f offset)
    {
        m_position += offset;
    }

    float Transform::rotation() const
    {
        return m_rotation;
    }

    void Transform::setRotation(float rotation)
    {
        m_rotation = rotation;
    }

    void Transform::rotate(float angle)
    {
        m_rotation += angle;
    }

    const sf::Vector2f& Transform::scale() const
    {
        return m_scale;
    }

    void Transform::setScale(sf::Vector2f scale)
    {
        m_scale = scale;
    }
}
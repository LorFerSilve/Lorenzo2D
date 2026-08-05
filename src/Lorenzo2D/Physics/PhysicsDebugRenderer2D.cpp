#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "../Renderer/RendererNumeric.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

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
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(value) || value < -maximum || value > maximum) return false;

            result = static_cast<float>(value);
            return true;
        }

        bool checkedAdd(sf::Vector2f left, sf::Vector2f right, sf::Vector2f& result)
        {
            return isFinite(left) && isFinite(right) &&
                   checkedFloat(static_cast<double>(left.x) + static_cast<double>(right.x),
                                result.x) &&
                   checkedFloat(static_cast<double>(left.y) + static_cast<double>(right.y),
                                result.y);
        }
    }

    PhysicsDebugRenderer2D::PhysicsDebugRenderer2D()
        : m_enabled(true), m_outlineThickness(2.f), m_defaultColor(sf::Color::Cyan),
          m_collidingColor(sf::Color::Red), m_sensorColor(sf::Color::Yellow)
    {
    }

    void PhysicsDebugRenderer2D::setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    bool PhysicsDebugRenderer2D::isEnabled() const
    {
        return m_enabled;
    }

    void PhysicsDebugRenderer2D::setOutlineThickness(float thickness)
    {
        if (!std::isfinite(thickness) || thickness < 0.f) thickness = 0.f;

        m_outlineThickness = thickness;
    }

    float PhysicsDebugRenderer2D::outlineThickness() const
    {
        return m_outlineThickness;
    }

    void PhysicsDebugRenderer2D::setDefaultColor(sf::Color color)
    {
        m_defaultColor = color;
    }

    sf::Color PhysicsDebugRenderer2D::defaultColor() const
    {
        return m_defaultColor;
    }

    void PhysicsDebugRenderer2D::setCollidingColor(sf::Color color)
    {
        m_collidingColor = color;
    }

    sf::Color PhysicsDebugRenderer2D::collidingColor() const
    {
        return m_collidingColor;
    }

    void PhysicsDebugRenderer2D::setSensorColor(sf::Color color)
    {
        m_sensorColor = color;
    }

    sf::Color PhysicsDebugRenderer2D::sensorColor() const
    {
        return m_sensorColor;
    }

    void PhysicsDebugRenderer2D::render(Scene& scene, sf::RenderWindow& window) const
    {
        render(scene, window, 1.f);
    }

    void PhysicsDebugRenderer2D::render(Scene& scene, sf::RenderWindow& window,
                                        float interpolationAlpha) const
    {
        if (!m_enabled) return;

        for (const auto& gameObject : scene.gameObjects())
        {
            if (gameObject == nullptr) continue;

            if (!gameObject->isActive()) continue;

            renderGameObject(*gameObject, window, interpolationAlpha);
        }
    }

    void PhysicsDebugRenderer2D::renderGameObject(GameObject& gameObject, sf::RenderWindow& window,
                                                  float interpolationAlpha) const
    {
        const sf::Vector2f ownerPosition =
            gameObject.transform.interpolated(interpolationAlpha).position;
        const Collider2D* collider = gameObject.getComponent<Collider2D>();

        if (collider == nullptr || !collider->isActive()) return;

        if (collider->type() == ColliderType::Box)
        {
            if (const auto* box = dynamic_cast<const BoxCollider2D*>(collider))
            {
                renderBoxCollider(*box, ownerPosition, window);
            }

            return;
        }

        if (const auto* circle = dynamic_cast<const CircleCollider2D*>(collider))
        {
            renderCircleCollider(*circle, ownerPosition, window);
        }
    }

    void PhysicsDebugRenderer2D::renderBoxCollider(const BoxCollider2D& collider,
                                                   sf::Vector2f ownerPosition,
                                                   sf::RenderWindow& window) const
    {
        sf::Vector2f position;
        const sf::Vector2f size = collider.size();

        if (!checkedAdd(ownerPosition, collider.offset(), position) ||
            !renderer_detail::hasSafeAxisAlignedBounds(position, size, m_outlineThickness))
        {
            return;
        }

        sf::RectangleShape shape;

        shape.setPosition(position);
        shape.setSize(size);

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);

        if (collider.isSensor())
            shape.setOutlineColor(m_sensorColor);
        else if (collider.isColliding())
            shape.setOutlineColor(m_collidingColor);
        else
            shape.setOutlineColor(m_defaultColor);

        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderCircleCollider(const CircleCollider2D& collider,
                                                      sf::Vector2f ownerPosition,
                                                      sf::RenderWindow& window) const
    {
        const float radius = collider.radius();
        sf::Vector2f center;
        sf::Vector2f position;
        sf::Vector2f size;

        if (!std::isfinite(radius) || radius < 0.f ||
            !checkedAdd(ownerPosition, collider.offset(), center) ||
            !checkedFloat(static_cast<double>(center.x) - static_cast<double>(radius),
                          position.x) ||
            !checkedFloat(static_cast<double>(center.y) - static_cast<double>(radius),
                          position.y) ||
            !checkedFloat(static_cast<double>(radius) * 2.0, size.x) ||
            !checkedFloat(static_cast<double>(radius) * 2.0, size.y) ||
            !renderer_detail::hasSafeAxisAlignedBounds(position, size, m_outlineThickness))
        {
            return;
        }

        sf::CircleShape shape(radius);
        shape.setPosition(position);

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);

        if (collider.isSensor())
            shape.setOutlineColor(m_sensorColor);
        else if (collider.isColliding())
            shape.setOutlineColor(m_collidingColor);
        else
            shape.setOutlineColor(m_defaultColor);

        window.draw(shape);
    }
}

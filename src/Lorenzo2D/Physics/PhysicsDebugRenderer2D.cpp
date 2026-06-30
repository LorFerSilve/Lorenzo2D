#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace l2d
{
    PhysicsDebugRenderer2D::PhysicsDebugRenderer2D()
        : m_enabled(true),
        m_outlineThickness(2.f),
        m_defaultColor(sf::Color::Cyan),
        m_collidingColor(sf::Color::Red)
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
        if (thickness < 0.f)
            thickness = 0.f;

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

    void PhysicsDebugRenderer2D::render(Scene& scene, sf::RenderWindow& window) const
    {
        if (!m_enabled)
            return;

        for (const auto& gameObject : scene.gameObjects())
        {
            if (gameObject == nullptr)
                continue;

            if (!gameObject->isActive())
                continue;

            renderGameObject(*gameObject, window);
        }
    }

    void PhysicsDebugRenderer2D::renderGameObject(GameObject& gameObject, sf::RenderWindow& window) const
    {
        if (const BoxCollider2D* boxCollider = gameObject.getComponent<BoxCollider2D>())
        {
            if (boxCollider->isActive())
            {
                renderBoxCollider(*boxCollider, window);
            }
        }

        if (const CircleCollider2D* circleCollider = gameObject.getComponent<CircleCollider2D>())
        {
            if (circleCollider->isActive())
            {
                renderCircleCollider(*circleCollider, window);
            }
        }
    }

    void PhysicsDebugRenderer2D::renderBoxCollider(
        const BoxCollider2D& collider,
        sf::RenderWindow& window
    ) const
    {
        sf::RectangleShape shape;

        shape.setPosition(collider.min());
        shape.setSize(collider.size());

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);

        if (collider.isColliding())
            shape.setOutlineColor(m_collidingColor);
        else
            shape.setOutlineColor(m_defaultColor);

        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderCircleCollider(
        const CircleCollider2D& collider,
        sf::RenderWindow& window
    ) const
    {
        const float radius = collider.radius();
        const sf::Vector2f center = collider.center();

        sf::CircleShape shape(radius);

        shape.setPosition(
            {
                center.x - radius,
                center.y - radius
            }
        );

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);

        if (collider.isColliding())
            shape.setOutlineColor(m_collidingColor);
        else
            shape.setOutlineColor(m_defaultColor);

        window.draw(shape);
    }
}
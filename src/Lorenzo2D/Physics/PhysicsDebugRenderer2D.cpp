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
#include <SFML/System/Angle.hpp>

#include <cmath>

namespace l2d
{
    namespace
    {
        sf::Color colliderColor(const Collider2D& collider, sf::Color defaultColor,
                                sf::Color collidingColor, sf::Color sensorColor)
        {
            if (collider.isSensor()) return sensorColor;
            if (collider.isColliding()) return collidingColor;

            return defaultColor;
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
        m_outlineThickness = std::isfinite(thickness) && thickness >= 0.f ? thickness : 0.f;
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
            if (gameObject != nullptr && gameObject->isActive())
            {
                renderGameObject(*gameObject, window, interpolationAlpha);
            }
        }
    }

    void PhysicsDebugRenderer2D::renderGameObject(GameObject& gameObject, sf::RenderWindow& window,
                                                  float interpolationAlpha) const
    {
        const TransformState transform = gameObject.transform.interpolated(interpolationAlpha);

        for (const Collider2D* collider : gameObject.getComponents<Collider2D>())
        {
            if (collider == nullptr || !collider->isActive()) continue;

            if (const auto* box = dynamic_cast<const BoxCollider2D*>(collider))
            {
                renderBoxCollider(*box, transform, window);
            }
            else if (const auto* circle = dynamic_cast<const CircleCollider2D*>(collider))
            {
                renderCircleCollider(*circle, transform, window);
            }
        }
    }

    void PhysicsDebugRenderer2D::renderBoxCollider(const BoxCollider2D& collider,
                                                   const TransformState& ownerTransform,
                                                   sf::RenderWindow& window) const
    {
        const sf::Vector2f size = collider.size();
        const sf::Vector2f halfSize = size * 0.5f;
        const sf::FloatRect localBounds(collider.offset() - halfSize, size);

        if (!renderer_detail::hasSafeTransformedBounds(localBounds, ownerTransform)) return;

        sf::RectangleShape shape(size);
        shape.setOrigin(halfSize - collider.offset());
        shape.setPosition(ownerTransform.position);
        shape.setRotation(
            sf::degrees(renderer_detail::normalizedRotationDegrees(ownerTransform.rotation)));
        shape.setScale(ownerTransform.scale);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderCircleCollider(const CircleCollider2D& collider,
                                                      const TransformState& ownerTransform,
                                                      sf::RenderWindow& window) const
    {
        const float radius = collider.radius();
        const sf::Vector2f diameter{radius * 2.f, radius * 2.f};
        const sf::FloatRect localBounds(collider.offset() - sf::Vector2f{radius, radius}, diameter);

        if (!renderer_detail::hasSafeTransformedBounds(localBounds, ownerTransform)) return;

        const float uniformScale =
            std::max(std::fabs(ownerTransform.scale.x), std::fabs(ownerTransform.scale.y));
        sf::CircleShape shape(radius);
        shape.setOrigin(sf::Vector2f{radius, radius} - collider.offset());
        shape.setPosition(ownerTransform.position);
        shape.setRotation(
            sf::degrees(renderer_detail::normalizedRotationDegrees(ownerTransform.rotation)));
        shape.setScale({uniformScale, uniformScale});
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }
}

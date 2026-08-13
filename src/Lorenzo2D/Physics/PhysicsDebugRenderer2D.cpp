#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "../Renderer/RendererNumeric.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

#include <cmath>
#include <limits>

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

        constexpr double Pi = 3.14159265358979323846;

        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        sf::Vector2f transformedPoint(sf::Vector2f local, const TransformState& transform)
        {
            const double radians = static_cast<double>(transform.rotation) * Pi / 180.0;
            const double cosine = std::cos(radians);
            const double sine = std::sin(radians);
            const double x = static_cast<double>(local.x) * transform.scale.x;
            const double y = static_cast<double>(local.y) * transform.scale.y;
            const double worldX = transform.position.x + x * cosine - y * sine;
            const double worldY = transform.position.y + x * sine + y * cosine;
            const double maximum = std::numeric_limits<float>::max();
            if (!std::isfinite(worldX) || !std::isfinite(worldY) || std::fabs(worldX) > maximum ||
                std::fabs(worldY) > maximum)
            {
                const float invalid = std::numeric_limits<float>::quiet_NaN();
                return {invalid, invalid};
            }
            return {static_cast<float>(worldX), static_cast<float>(worldY)};
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
        render(scene, window,
               RenderContext2D{interpolationAlpha, nullptr, RenderPass2D::PhysicsDebug});
    }

    void PhysicsDebugRenderer2D::render(Scene& scene, sf::RenderWindow& window,
                                        const RenderContext2D& context) const
    {
        if (!m_enabled) return;

        for (const auto& gameObject : scene.gameObjects())
        {
            if (gameObject != nullptr && gameObject->isActive())
            {
                renderGameObject(*gameObject, window, context);
            }
        }
    }

    void PhysicsDebugRenderer2D::renderGameObject(GameObject& gameObject, sf::RenderWindow& window,
                                                  const RenderContext2D& context) const
    {
        const TransformState transform =
            gameObject.transform.interpolated(context.interpolationAlpha);

        for (const Collider2D* collider : gameObject.getComponents<Collider2D>())
        {
            if (collider == nullptr || !collider->isActive()) continue;

            if (const auto* box = dynamic_cast<const BoxCollider2D*>(collider))
            {
                renderBoxCollider(*box, transform, window, context);
            }
            else if (const auto* circle = dynamic_cast<const CircleCollider2D*>(collider))
            {
                renderCircleCollider(*circle, transform, window, context);
            }
            else if (const auto* capsule = dynamic_cast<const CapsuleCollider2D*>(collider))
            {
                renderCapsuleCollider(*capsule, transform, window, context);
            }
            else if (const auto* polygon = dynamic_cast<const ConvexPolygonCollider2D*>(collider))
            {
                renderConvexPolygonCollider(*polygon, transform, window, context);
            }
        }
    }

    void PhysicsDebugRenderer2D::renderBoxCollider(const BoxCollider2D& collider,
                                                   const TransformState& ownerTransform,
                                                   sf::RenderWindow& window,
                                                   const RenderContext2D& context) const
    {
        const sf::Vector2f size = collider.size();
        const sf::Vector2f halfSize = size * 0.5f;
        const sf::FloatRect localBounds(collider.offset() - halfSize, size);

        if (!renderer_detail::hasSafeTransformedBounds(localBounds, ownerTransform)) return;

        if (context.projection == nullptr || context.projection->isIdentity())
        {
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
            return;
        }

        const sf::Vector2f minimum = collider.offset() - halfSize;
        const sf::Vector2f maximum = collider.offset() + halfSize;
        const sf::Vector2f localCorners[] = {
            {minimum.x, minimum.y},
            {maximum.x, minimum.y},
            {maximum.x, maximum.y},
            {minimum.x, maximum.y},
        };
        sf::ConvexShape shape(4u);

        for (std::size_t index = 0; index < 4u; ++index)
        {
            shape.setPoint(index, context.worldToRender(
                                      transformedPoint(localCorners[index], ownerTransform)));
        }

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderCircleCollider(const CircleCollider2D& collider,
                                                      const TransformState& ownerTransform,
                                                      sf::RenderWindow& window,
                                                      const RenderContext2D& context) const
    {
        const float radius = collider.radius();
        const sf::Vector2f diameter{radius * 2.f, radius * 2.f};
        const sf::FloatRect localBounds(collider.offset() - sf::Vector2f{radius, radius}, diameter);

        if (!renderer_detail::hasSafeTransformedBounds(localBounds, ownerTransform)) return;

        if (context.projection == nullptr || context.projection->isIdentity())
        {
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
            return;
        }

        constexpr std::size_t CircleSteps = 32u;
        const sf::Vector2f center = transformedPoint(collider.offset(), ownerTransform);
        const float worldRadius =
            radius * std::max(std::fabs(ownerTransform.scale.x), std::fabs(ownerTransform.scale.y));

        if (!finite(center) || !std::isfinite(worldRadius)) return;

        sf::ConvexShape shape(CircleSteps);

        for (std::size_t index = 0; index < CircleSteps; ++index)
        {
            const double angle = 2.0 * Pi * static_cast<double>(index) / CircleSteps;
            const sf::Vector2f point =
                center + sf::Vector2f{static_cast<float>(std::cos(angle) * worldRadius),
                                      static_cast<float>(std::sin(angle) * worldRadius)};
            shape.setPoint(index, context.worldToRender(point));
        }

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderCapsuleCollider(const CapsuleCollider2D& collider,
                                                       const TransformState& ownerTransform,
                                                       sf::RenderWindow& window,
                                                       const RenderContext2D& context) const
    {
        constexpr std::size_t HalfSteps = 12u;
        const sf::Vector2f center = transformedPoint(collider.offset(), ownerTransform);
        if (!finite(center)) return;
        const double radians = static_cast<double>(ownerTransform.rotation) * Pi / 180.0;
        const sf::Vector2f axisX = {static_cast<float>(std::cos(radians)),
                                    static_cast<float>(std::sin(radians))};
        const sf::Vector2f axisY = {-axisX.y, axisX.x};
        const float radius = collider.radius() * std::max(std::fabs(ownerTransform.scale.x),
                                                          std::fabs(ownerTransform.scale.y));
        const float halfSegment = std::max(0.f, collider.height() * 0.5f - collider.radius()) *
                                  std::fabs(ownerTransform.scale.y);
        if (!std::isfinite(radius) || !std::isfinite(halfSegment)) return;

        sf::ConvexShape shape((HalfSteps + 1u) * 2u);
        const sf::Vector2f firstCenter = center - axisY * halfSegment;
        const sf::Vector2f secondCenter = center + axisY * halfSegment;
        for (std::size_t index = 0; index <= HalfSteps; ++index)
        {
            const double angle =
                Pi + Pi * static_cast<double>(index) / static_cast<double>(HalfSteps);
            shape.setPoint(
                index, context.worldToRender(firstCenter +
                                             axisX * static_cast<float>(std::cos(angle) * radius) +
                                             axisY * static_cast<float>(std::sin(angle) * radius)));
        }
        for (std::size_t index = 0; index <= HalfSteps; ++index)
        {
            const double angle = Pi * static_cast<double>(index) / static_cast<double>(HalfSteps);
            shape.setPoint(HalfSteps + 1u + index,
                           context.worldToRender(
                               secondCenter + axisX * static_cast<float>(std::cos(angle) * radius) +
                               axisY * static_cast<float>(std::sin(angle) * radius)));
        }
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }

    void PhysicsDebugRenderer2D::renderConvexPolygonCollider(
        const ConvexPolygonCollider2D& collider, const TransformState& ownerTransform,
        sf::RenderWindow& window, const RenderContext2D& context) const
    {
        sf::ConvexShape shape(collider.vertices().size());
        for (std::size_t index = 0; index < collider.vertices().size(); ++index)
        {
            const sf::Vector2f point =
                transformedPoint(collider.offset() + collider.vertices()[index], ownerTransform);
            if (!finite(point)) return;
            shape.setPoint(index, context.worldToRender(point));
        }
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(m_outlineThickness);
        shape.setOutlineColor(
            colliderColor(collider, m_defaultColor, m_collidingColor, m_sensorColor));
        window.draw(shape);
    }
}

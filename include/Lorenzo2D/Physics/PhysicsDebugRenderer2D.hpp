#pragma once

#include <Lorenzo2D/ECS/Transform.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class Scene;
    class GameObject;
    class BoxCollider2D;
    class CircleCollider2D;
    class CapsuleCollider2D;
    class ConvexPolygonCollider2D;

    class PhysicsDebugRenderer2D
    {
      public:
        PhysicsDebugRenderer2D();

        void setEnabled(bool enabled);
        bool isEnabled() const;

        void setOutlineThickness(float thickness);
        float outlineThickness() const;

        void setDefaultColor(sf::Color color);
        sf::Color defaultColor() const;

        void setCollidingColor(sf::Color color);
        sf::Color collidingColor() const;

        void setSensorColor(sf::Color color);
        sf::Color sensorColor() const;

        void render(Scene& scene, sf::RenderWindow& window) const;
        void render(Scene& scene, sf::RenderWindow& window, float interpolationAlpha) const;

      private:
        void renderGameObject(GameObject& gameObject, sf::RenderWindow& window,
                              float interpolationAlpha) const;
        void renderBoxCollider(const BoxCollider2D& collider, const TransformState& ownerTransform,
                               sf::RenderWindow& window) const;
        void renderCircleCollider(const CircleCollider2D& collider,
                                  const TransformState& ownerTransform,
                                  sf::RenderWindow& window) const;
        void renderCapsuleCollider(const CapsuleCollider2D& collider,
                                   const TransformState& ownerTransform,
                                   sf::RenderWindow& window) const;
        void renderConvexPolygonCollider(const ConvexPolygonCollider2D& collider,
                                         const TransformState& ownerTransform,
                                         sf::RenderWindow& window) const;

      private:
        bool m_enabled;
        float m_outlineThickness;

        sf::Color m_defaultColor;
        sf::Color m_collidingColor;
        sf::Color m_sensorColor;
    };
}

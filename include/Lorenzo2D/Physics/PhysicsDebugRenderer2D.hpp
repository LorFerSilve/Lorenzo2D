#pragma once

#include <SFML/Graphics/Color.hpp>

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

        void render(Scene& scene, sf::RenderWindow& window) const;

    private:
        void renderGameObject(GameObject& gameObject, sf::RenderWindow& window) const;
        void renderBoxCollider(const BoxCollider2D& collider, sf::RenderWindow& window) const;
        void renderCircleCollider(const CircleCollider2D& collider, sf::RenderWindow& window) const;

    private:
        bool m_enabled;
        float m_outlineThickness;

        sf::Color m_defaultColor;
        sf::Color m_collidingColor;
    };
}
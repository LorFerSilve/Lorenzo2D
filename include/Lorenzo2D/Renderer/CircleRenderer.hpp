#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>

namespace l2d
{
    class CircleRenderer : public Component
    {
    public:
        CircleRenderer(float radius = 50.f, sf::Color color = sf::Color::White);

        // Nonfinite and negative radii become zero. Extreme finite radii are
        // capped before SFML generates local vertices.
        void setRadius(float radius);
        float radius() const;

        void setFillColor(sf::Color color);
        sf::Color fillColor() const;

        void onRender(sf::RenderWindow& window) override;
        void onRender(
            sf::RenderWindow& window,
            float interpolationAlpha
        ) override;

    private:
        sf::CircleShape m_shape;
    };
}

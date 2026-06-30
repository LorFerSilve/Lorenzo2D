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

        void setRadius(float radius);
        float radius() const;

        void setFillColor(sf::Color color);
        sf::Color fillColor() const;

        void onRender(sf::RenderWindow& window) override;

    private:
        sf::CircleShape m_shape;
    };
}
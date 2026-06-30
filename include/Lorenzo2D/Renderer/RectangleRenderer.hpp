#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class RectangleRenderer : public Component
    {
    public:
        RectangleRenderer(sf::Vector2f size = { 100.f, 100.f }, sf::Color color = sf::Color::White);

        void setSize(sf::Vector2f size);
        sf::Vector2f size() const;

        void setFillColor(sf::Color color);
        sf::Color fillColor() const;

        void onRender(sf::RenderWindow& window) override;

    private:
        sf::RectangleShape m_shape;
    };
}
#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class SpriteRenderer : public Component
    {
    public:
        explicit SpriteRenderer(const sf::Texture& texture);

        void setTexture(const sf::Texture& texture, bool resetRect = true);

        void setSize(sf::Vector2f size);
        const sf::Vector2f& sizeScale() const;

        void setColor(sf::Color color);
        sf::Color color() const;

        void onRender(sf::RenderWindow& window) override;
        void onRender(
            sf::RenderWindow& window,
            float interpolationAlpha
        ) override;

    private:
        const sf::Texture* m_texture;
        sf::Sprite m_sprite;

        sf::Vector2f m_sizeScale;
    };
}

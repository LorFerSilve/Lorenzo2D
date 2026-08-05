#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>
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
        explicit SpriteRenderer(TextureHandle texture);

        // Invalid handles are rejected without changing the current binding.
        bool setTexture(TextureHandle texture, bool resetRect = true);
        TextureHandle textureHandle() const;

        // Each nonfinite or negative desired axis becomes zero.
        void setSize(sf::Vector2f size);
        const sf::Vector2f& sizeScale() const;

        void setColor(sf::Color color);
        sf::Color color() const;

        void setTextureRect(sf::IntRect textureRect);
        sf::IntRect textureRect() const;

        void onRender(sf::RenderWindow& window) override;
        void onRender(sf::RenderWindow& window, float interpolationAlpha) override;

      private:
        // The lease must outlive the SFML drawable that borrows from it.
        TextureHandle m_texture;
        sf::Sprite m_sprite;

        sf::Vector2f m_sizeScale;
    };
}

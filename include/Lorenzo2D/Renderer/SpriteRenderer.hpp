#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>

namespace l2d
{
    class SpriteRenderer : public Component
    {
      public:
        explicit SpriteRenderer(TextureHandle texture);

        // Invalid handles are rejected without changing the current binding.
        bool setTexture(TextureHandle texture, bool resetRect = true);
        bool setLiveTexture(LiveTextureHandle texture, bool resetRect = true);
        LiveTextureHandle liveTextureHandle() const;
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
        void syncLiveTexture();

        // The lease must outlive the SFML drawable that borrows from it.
        TextureHandle m_texture;
        LiveTextureHandle m_liveTexture;
        std::uint64_t m_liveGeneration = 0;
        sf::Sprite m_sprite;

        sf::Vector2f m_sizeScale;
    };
}

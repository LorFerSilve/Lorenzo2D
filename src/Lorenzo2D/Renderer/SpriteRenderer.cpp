#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include "RendererNumeric.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

#include <stdexcept>
#include <utility>

namespace l2d
{
    namespace
    {
        const sf::Texture& requireTexture(const TextureHandle& texture)
        {
            if (!texture)
            {
                throw std::invalid_argument(
                    "SpriteRenderer requires a valid texture handle."
                );
            }

            return *texture;
        }
    }

    SpriteRenderer::SpriteRenderer(TextureHandle texture)
        : m_texture(std::move(texture)),
        m_sprite(requireTexture(m_texture)),
        m_sizeScale(1.f, 1.f)
    {
    }

    bool SpriteRenderer::setTexture(
        TextureHandle texture,
        bool resetRect
    )
    {
        if (!texture)
            return false;

        // Rebind the sprite before releasing the lease for its old texture.
        m_sprite.setTexture(*texture, resetRect);
        m_texture = std::move(texture);
        return true;
    }

    TextureHandle SpriteRenderer::textureHandle() const
    {
        return m_texture;
    }

    void SpriteRenderer::setSize(sf::Vector2f size)
    {
        size = {
            renderer_detail::sanitizeNonNegative(size.x),
            renderer_detail::sanitizeNonNegative(size.y)
        };

        if (!m_texture)
            return;

        const sf::Vector2u textureSize = m_texture->getSize();

        if (textureSize.x == 0 || textureSize.y == 0)
            return;

        m_sizeScale =
        {
            size.x / static_cast<float>(textureSize.x),
            size.y / static_cast<float>(textureSize.y)
        };
    }

    const sf::Vector2f& SpriteRenderer::sizeScale() const
    {
        return m_sizeScale;
    }

    void SpriteRenderer::setColor(sf::Color color)
    {
        m_sprite.setColor(color);
    }

    sf::Color SpriteRenderer::color() const
    {
        return m_sprite.getColor();
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void SpriteRenderer::onRender(
        sf::RenderWindow& window,
        float interpolationAlpha
    )
    {
        GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        const TransformState state =
            gameObject->transform.interpolated(interpolationAlpha);

        const TransformState spriteState = {
            state.position,
            state.rotation,
            {
                m_sizeScale.x * state.scale.x,
                m_sizeScale.y * state.scale.y
            }
        };

        if (!renderer_detail::hasSafeTransformedBounds(
            m_sprite.getLocalBounds(),
            spriteState
        ))
        {
            return;
        }

        m_sprite.setPosition(state.position);
        m_sprite.setRotation(sf::degrees(
            renderer_detail::normalizedRotationDegrees(state.rotation)
        ));

        m_sprite.setScale(
            {
                m_sizeScale.x * state.scale.x,
                m_sizeScale.y * state.scale.y
            }
        );

        window.draw(m_sprite);
    }
}

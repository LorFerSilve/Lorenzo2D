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
                throw std::invalid_argument("SpriteRenderer requires a valid texture handle.");
            }

            return *texture;
        }
    }

    SpriteRenderer::SpriteRenderer(TextureHandle texture)
        : m_texture(std::move(texture)), m_sprite(requireTexture(m_texture)), m_sizeScale(1.f, 1.f)
    {
    }

    bool SpriteRenderer::setTexture(TextureHandle texture, bool resetRect)
    {
        if (!texture) return false;

        // Rebind the sprite before releasing the lease for its old texture.
        m_sprite.setTexture(*texture, resetRect);
        m_texture = std::move(texture);
        m_liveTexture.reset();
        m_liveGeneration = 0;
        return true;
    }

    bool SpriteRenderer::setLiveTexture(LiveTextureHandle texture, bool resetRect)
    {
        const std::uint64_t generation = texture.generation();
        const TextureHandle snapshot = texture.snapshot();

        if (!snapshot) return false;

        m_sprite.setTexture(*snapshot, resetRect);
        m_texture = snapshot;
        m_liveGeneration = generation;
        m_liveTexture = std::move(texture);
        return true;
    }

    LiveTextureHandle SpriteRenderer::liveTextureHandle() const
    {
        return m_liveTexture;
    }

    TextureHandle SpriteRenderer::textureHandle() const
    {
        return m_texture;
    }

    void SpriteRenderer::setSize(sf::Vector2f size)
    {
        size = {renderer_detail::sanitizeNonNegative(size.x),
                renderer_detail::sanitizeNonNegative(size.y)};

        if (!m_texture) return;

        const sf::Vector2f localSize = m_sprite.getLocalBounds().size;

        if (localSize.x <= 0.f || localSize.y <= 0.f) return;

        m_sizeScale = {size.x / localSize.x, size.y / localSize.y};
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

    void SpriteRenderer::setTextureRect(sf::IntRect textureRect)
    {
        m_sprite.setTextureRect(textureRect);
    }

    sf::IntRect SpriteRenderer::textureRect() const
    {
        return m_sprite.getTextureRect();
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        syncLiveTexture();

        GameObject* gameObject = owner();

        if (gameObject == nullptr) return;

        const TransformState state = gameObject->transform.interpolated(interpolationAlpha);

        const TransformState spriteState = {
            state.position,
            state.rotation,
            {m_sizeScale.x * state.scale.x, m_sizeScale.y * state.scale.y}};

        if (!renderer_detail::hasSafeTransformedBounds(m_sprite.getLocalBounds(), spriteState))
        {
            return;
        }

        m_sprite.setPosition(state.position);
        m_sprite.setRotation(
            sf::degrees(renderer_detail::normalizedRotationDegrees(state.rotation)));

        m_sprite.setScale({m_sizeScale.x * state.scale.x, m_sizeScale.y * state.scale.y});

        window.draw(m_sprite);
    }

    void SpriteRenderer::syncLiveTexture()
    {
        if (!m_liveTexture) return;

        const std::uint64_t generation = m_liveTexture.generation();

        if (generation == m_liveGeneration) return;

        const TextureHandle snapshot = m_liveTexture.snapshot();
        m_liveGeneration = generation;

        if (!snapshot) return;

        m_sprite.setTexture(*snapshot, false);
        m_texture = snapshot;
    }
}

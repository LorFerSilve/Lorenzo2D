#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include "RendererNumeric.hpp"

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

#include <stdexcept>
#include <cmath>
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
        applyOriginPreset();
    }

    bool SpriteRenderer::setTexture(TextureHandle texture, bool resetRect)
    {
        if (!texture) return false;

        // Rebind the sprite before releasing the lease for its old texture.
        m_sprite.setTexture(*texture, resetRect);
        m_texture = std::move(texture);
        m_liveTexture.reset();
        m_liveGeneration = 0;
        if (resetRect) applyOriginPreset();
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
        if (resetRect) applyOriginPreset();
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
        applyOriginPreset();
    }

    sf::IntRect SpriteRenderer::textureRect() const
    {
        return m_sprite.getTextureRect();
    }

    bool SpriteRenderer::setOrigin(sf::Vector2f origin)
    {
        if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) return false;

        m_sprite.setOrigin(origin);
        m_usesOriginPreset = false;
        return true;
    }

    sf::Vector2f SpriteRenderer::origin() const
    {
        return m_sprite.getOrigin();
    }

    void SpriteRenderer::setOriginPreset(SpriteOriginPreset2D preset)
    {
        switch (preset)
        {
        case SpriteOriginPreset2D::TopLeft:
        case SpriteOriginPreset2D::Center:
        case SpriteOriginPreset2D::BottomCenter:
            break;
        default:
            return;
        }

        m_originPreset = preset;
        m_usesOriginPreset = true;
        applyOriginPreset();
    }

    SpriteOriginPreset2D SpriteRenderer::originPreset() const
    {
        return m_originPreset;
    }

    bool SpriteRenderer::usesOriginPreset() const
    {
        return m_usesOriginPreset;
    }

    void SpriteRenderer::setFlippedX(bool flipped)
    {
        m_flippedX = flipped;
    }

    void SpriteRenderer::setFlippedY(bool flipped)
    {
        m_flippedY = flipped;
    }

    bool SpriteRenderer::isFlippedX() const
    {
        return m_flippedX;
    }

    bool SpriteRenderer::isFlippedY() const
    {
        return m_flippedY;
    }

    void SpriteRenderer::setMaterial(Material2DHandle material)
    {
        m_material = std::move(material);
    }

    Material2DHandle SpriteRenderer::material() const noexcept
    {
        return m_material;
    }

    void SpriteRenderer::clearMaterial()
    {
        m_material.reset();
    }

    sf::Vector2f SpriteRenderer::worldFootPoint(float interpolationAlpha) const
    {
        const GameObject* gameObject = owner();

        if (gameObject == nullptr) return {};

        const TransformState state = gameObject->transform.interpolated(interpolationAlpha);
        const sf::FloatRect bounds = m_sprite.getLocalBounds();
        const sf::Vector2f localFoot =
            bounds.position + sf::Vector2f{bounds.size.x * 0.5f, bounds.size.y} - origin();
        const double radians = static_cast<double>(state.rotation) * 3.14159265358979323846 / 180.0;
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        const double x = static_cast<double>(localFoot.x) * m_sizeScale.x * state.scale.x;
        const double y = static_cast<double>(localFoot.y) * m_sizeScale.y * state.scale.y;

        return {static_cast<float>(state.position.x + cosine * x - sine * y),
                static_cast<float>(state.position.y + sine * x + cosine * y)};
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        onRender(window, RenderContext2D{interpolationAlpha});
    }

    void SpriteRenderer::onRender(sf::RenderWindow& window, const RenderContext2D& context)
    {
        syncLiveTexture();

        GameObject* gameObject = owner();

        if (gameObject == nullptr) return;

        TransformState state = gameObject->transform.interpolated(context.interpolationAlpha);
        state.position = context.worldToRender(state.position);

        const float flipX = m_flippedX ? -1.f : 1.f;
        const float flipY = m_flippedY ? -1.f : 1.f;
        sf::FloatRect localBounds = m_sprite.getLocalBounds();
        localBounds.position -= m_sprite.getOrigin();

        const TransformState spriteState = {
            state.position,
            state.rotation,
            {flipX * m_sizeScale.x * state.scale.x, flipY * m_sizeScale.y * state.scale.y}};

        if (!renderer_detail::hasSafeTransformedBounds(localBounds, spriteState))
        {
            return;
        }

        m_sprite.setPosition(state.position);
        m_sprite.setRotation(
            sf::degrees(renderer_detail::normalizedRotationDegrees(state.rotation)));

        m_sprite.setScale(spriteState.scale);

        if (m_material)
        {
            sf::RenderStates states;
            if (!m_material->apply(states)) return;
            window.draw(m_sprite, states);
            return;
        }

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
        applyOriginPreset();
    }

    void SpriteRenderer::applyOriginPreset()
    {
        if (!m_usesOriginPreset) return;

        const sf::FloatRect bounds = m_sprite.getLocalBounds();
        sf::Vector2f nextOrigin = bounds.position;

        switch (m_originPreset)
        {
        case SpriteOriginPreset2D::TopLeft:
            break;
        case SpriteOriginPreset2D::Center:
            nextOrigin += bounds.size * 0.5f;
            break;
        case SpriteOriginPreset2D::BottomCenter:
            nextOrigin += {bounds.size.x * 0.5f, bounds.size.y};
            break;
        }

        m_sprite.setOrigin(nextOrigin);
    }
}

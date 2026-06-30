#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

namespace l2d
{
    SpriteRenderer::SpriteRenderer(const sf::Texture& texture)
        : m_texture(&texture),
        m_sprite(texture),
        m_sizeScale(1.f, 1.f)
    {
    }

    void SpriteRenderer::setTexture(const sf::Texture& texture, bool resetRect)
    {
        m_texture = &texture;
        m_sprite.setTexture(texture, resetRect);
    }

    void SpriteRenderer::setSize(sf::Vector2f size)
    {
        if (m_texture == nullptr)
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
        GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        const sf::Vector2f transformScale = gameObject->transform.scale();

        m_sprite.setPosition(gameObject->transform.position());
        m_sprite.setRotation(sf::degrees(gameObject->transform.rotation()));

        m_sprite.setScale(
            {
                m_sizeScale.x * transformScale.x,
                m_sizeScale.y * transformScale.y
            }
        );

        window.draw(m_sprite);
    }
}
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

namespace l2d
{
    RectangleRenderer::RectangleRenderer(sf::Vector2f size, sf::Color color)
    {
        m_shape.setSize(size);
        m_shape.setFillColor(color);
    }

    void RectangleRenderer::setSize(sf::Vector2f size)
    {
        m_shape.setSize(size);
    }

    sf::Vector2f RectangleRenderer::size() const
    {
        return m_shape.getSize();
    }

    void RectangleRenderer::setFillColor(sf::Color color)
    {
        m_shape.setFillColor(color);
    }

    sf::Color RectangleRenderer::fillColor() const
    {
        return m_shape.getFillColor();
    }

    void RectangleRenderer::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void RectangleRenderer::onRender(
        sf::RenderWindow& window,
        float interpolationAlpha
    )
    {
        GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        const TransformState state =
            gameObject->transform.interpolated(interpolationAlpha);

        m_shape.setPosition(state.position);
        m_shape.setRotation(sf::degrees(state.rotation));
        m_shape.setScale(state.scale);

        window.draw(m_shape);
    }
}

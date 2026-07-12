#include <Lorenzo2D/Renderer/CircleRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

namespace l2d
{
    CircleRenderer::CircleRenderer(float radius, sf::Color color)
    {
        m_shape.setRadius(radius);
        m_shape.setFillColor(color);
    }

    void CircleRenderer::setRadius(float radius)
    {
        m_shape.setRadius(radius);
    }

    float CircleRenderer::radius() const
    {
        return m_shape.getRadius();
    }

    void CircleRenderer::setFillColor(sf::Color color)
    {
        m_shape.setFillColor(color);
    }

    sf::Color CircleRenderer::fillColor() const
    {
        return m_shape.getFillColor();
    }

    void CircleRenderer::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void CircleRenderer::onRender(
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

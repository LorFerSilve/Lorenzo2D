#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include "RendererNumeric.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Angle.hpp>

namespace l2d
{
    RectangleRenderer::RectangleRenderer(sf::Vector2f size, sf::Color color)
    {
        setSize(size);
        m_shape.setFillColor(color);
    }

    void RectangleRenderer::setSize(sf::Vector2f size)
    {
        m_shape.setSize({renderer_detail::sanitizeNonNegative(size.x),
                         renderer_detail::sanitizeNonNegative(size.y)});
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

    void RectangleRenderer::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        onRender(window, RenderContext2D{interpolationAlpha});
    }

    void RectangleRenderer::onRender(sf::RenderWindow& window, const RenderContext2D& context)
    {
        GameObject* gameObject = owner();

        if (gameObject == nullptr) return;

        TransformState state = gameObject->transform.interpolated(context.interpolationAlpha);
        state.position = context.worldToRender(state.position);

        if (!renderer_detail::hasSafeTransformedBounds(m_shape.getLocalBounds(), state))
        {
            return;
        }

        m_shape.setPosition(state.position);
        m_shape.setRotation(
            sf::degrees(renderer_detail::normalizedRotationDegrees(state.rotation)));
        m_shape.setScale(state.scale);

        window.draw(m_shape);
    }
}

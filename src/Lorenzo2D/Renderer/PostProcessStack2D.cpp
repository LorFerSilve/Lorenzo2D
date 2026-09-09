#include <Lorenzo2D/Renderer/PostProcessStack2D.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

namespace l2d
{
    std::size_t PostProcessStack2D::addPass(PostProcessPass2D pass)
    {
        m_passes.push_back(pass);
        return m_passes.size() - 1u;
    }

    bool PostProcessStack2D::setPass(std::size_t index, PostProcessPass2D pass)
    {
        if (index >= m_passes.size()) return false;
        m_passes[index] = pass;
        return true;
    }

    bool PostProcessStack2D::removePass(std::size_t index)
    {
        if (index >= m_passes.size()) return false;
        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    void PostProcessStack2D::clear()
    {
        m_passes.clear();
    }

    std::size_t PostProcessStack2D::passCount() const
    {
        return m_passes.size();
    }

    const PostProcessPass2D* PostProcessStack2D::pass(std::size_t index) const
    {
        return index < m_passes.size() ? &m_passes[index] : nullptr;
    }

    void PostProcessStack2D::apply(sf::RenderTarget& target) const
    {
        const sf::View previousView = target.getView();
        target.setView(target.getDefaultView());
        sf::RectangleShape overlay(
            {static_cast<float>(target.getSize().x), static_cast<float>(target.getSize().y)});

        for (const PostProcessPass2D& pass : m_passes)
        {
            if (!pass.enabled || pass.color.a == 0u) continue;

            overlay.setFillColor(pass.color);
            sf::RenderStates states;

            switch (pass.blend)
            {
            case PostProcessBlend2D::Alpha:
                states.blendMode = sf::BlendAlpha;
                break;
            case PostProcessBlend2D::Add:
                states.blendMode = sf::BlendAdd;
                break;
            case PostProcessBlend2D::Multiply:
                states.blendMode = sf::BlendMultiply;
                break;
            }

            target.draw(overlay, states);
        }

        target.setView(previousView);
    }

    void PostProcessStack2D::apply(sf::RenderWindow& window) const
    {
        apply(static_cast<sf::RenderTarget&>(window));
    }
}

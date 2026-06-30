#include <Lorenzo2D/Renderer/DebugOverlay.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

namespace l2d
{
    DebugOverlay::DebugOverlay()
        : m_text(m_ownedFont),
        m_hasFont(false)
    {
        m_text.setString("");
        m_text.setCharacterSize(18);
        m_text.setFillColor(sf::Color::White);
        m_text.setPosition({ 10.f, 10.f });
    }

    bool DebugOverlay::loadFontFromFile(const std::string& filepath)
    {
        m_hasFont = m_ownedFont.openFromFile(filepath);

        if (m_hasFont)
        {
            m_text.setFont(m_ownedFont);
        }

        return m_hasFont;
    }

    void DebugOverlay::setFont(const sf::Font& font)
    {
        m_text.setFont(font);
        m_hasFont = true;
    }

    bool DebugOverlay::hasFont() const
    {
        return m_hasFont;
    }

    void DebugOverlay::setText(const std::string& text)
    {
        m_text.setString(text);
    }

    void DebugOverlay::setPosition(sf::Vector2f position)
    {
        m_text.setPosition(position);
    }

    void DebugOverlay::setCharacterSize(unsigned int size)
    {
        m_text.setCharacterSize(size);
    }

    void DebugOverlay::setFillColor(sf::Color color)
    {
        m_text.setFillColor(color);
    }

    void DebugOverlay::render(sf::RenderWindow& window) const
    {
        if (!m_hasFont)
            return;

        window.draw(m_text);
    }
}
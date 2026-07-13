#include <Lorenzo2D/Renderer/DebugOverlay.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

#include <memory>
#include <utility>

namespace l2d
{
    namespace
    {
        FontHandle makeEmptyFontHandle()
        {
            return FontHandle(std::make_shared<sf::Font>());
        }
    }

    DebugOverlay::DebugOverlay()
        : m_font(makeEmptyFontHandle()),
        m_text(*m_font),
        m_hasFont(false)
    {
        m_text.setString("");
        m_text.setCharacterSize(18);
        m_text.setFillColor(sf::Color::White);
        m_text.setPosition({ 10.f, 10.f });
    }

    bool DebugOverlay::loadFontFromFile(const std::string& filepath)
    {
        std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();

        if (!font->openFromFile(filepath))
            return false;

        return setFont(FontHandle(std::move(font)));
    }

    bool DebugOverlay::setFont(FontHandle font)
    {
        if (!font)
            return false;

        // Rebind the text before releasing the lease for its old font.
        m_text.setFont(*font);
        m_font = std::move(font);
        m_hasFont = true;
        return true;
    }

    FontHandle DebugOverlay::fontHandle() const
    {
        if (!m_hasFont)
            return {};

        return m_font;
    }

    void DebugOverlay::clearFont()
    {
        FontHandle emptyFont = makeEmptyFontHandle();
        m_text.setFont(*emptyFont);
        m_font = std::move(emptyFont);
        m_hasFont = false;
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

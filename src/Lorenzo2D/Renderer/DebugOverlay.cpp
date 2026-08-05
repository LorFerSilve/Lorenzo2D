#include <Lorenzo2D/Renderer/DebugOverlay.hpp>

#include "RendererNumeric.hpp"

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

    DebugOverlay::DebugOverlay() : m_font(makeEmptyFontHandle()), m_text(*m_font), m_hasFont(false)
    {
        m_text.setString("");
        m_text.setCharacterSize(18);
        m_text.setFillColor(sf::Color::White);
        m_text.setPosition({10.f, 10.f});
    }

    bool DebugOverlay::loadFontFromFile(const std::string& filepath)
    {
        std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();

        if (!font->openFromFile(filepath)) return false;

        return setFont(FontHandle(std::move(font)));
    }

    bool DebugOverlay::setFont(FontHandle font)
    {
        if (!font) return false;

        // Rebind the text before releasing the lease for its old font.
        m_text.setFont(*font);
        m_font = std::move(font);
        m_liveFont.reset();
        m_liveGeneration = 0;
        m_hasFont = true;
        return true;
    }

    bool DebugOverlay::setLiveFont(LiveFontHandle font)
    {
        const std::uint64_t generation = font.generation();
        const FontHandle snapshot = font.snapshot();

        if (!snapshot) return false;

        m_text.setFont(*snapshot);
        m_font = snapshot;
        m_liveFont = std::move(font);
        m_liveGeneration = generation;
        m_hasFont = true;
        return true;
    }

    LiveFontHandle DebugOverlay::liveFontHandle() const
    {
        return m_liveFont;
    }

    FontHandle DebugOverlay::fontHandle() const
    {
        if (!m_hasFont) return {};

        return m_font;
    }

    void DebugOverlay::clearFont()
    {
        FontHandle emptyFont = makeEmptyFontHandle();
        m_text.setFont(*emptyFont);
        m_font = std::move(emptyFont);
        m_liveFont.reset();
        m_liveGeneration = 0;
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
        if (!renderer_detail::isSafeDrawablePosition(position)) return;

        m_text.setPosition(position);
    }

    sf::Vector2f DebugOverlay::position() const
    {
        return m_text.getPosition();
    }

    void DebugOverlay::setCharacterSize(unsigned int size)
    {
        if (size == 0u) size = 1u;

        m_text.setCharacterSize(size);
    }

    unsigned int DebugOverlay::characterSize() const
    {
        return m_text.getCharacterSize();
    }

    void DebugOverlay::setFillColor(sf::Color color)
    {
        m_text.setFillColor(color);
    }

    void DebugOverlay::render(sf::RenderWindow& window) const
    {
        syncLiveFont();

        if (!m_hasFont) return;

        window.draw(m_text);
    }

    void DebugOverlay::syncLiveFont() const
    {
        if (!m_liveFont) return;

        const std::uint64_t generation = m_liveFont.generation();

        if (generation == m_liveGeneration) return;

        const FontHandle snapshot = m_liveFont.snapshot();
        m_liveGeneration = generation;

        if (!snapshot) return;

        m_text.setFont(*snapshot);
        m_font = snapshot;
        m_hasFont = true;
    }
}

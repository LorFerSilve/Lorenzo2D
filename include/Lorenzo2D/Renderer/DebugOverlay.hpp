#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class DebugOverlay
    {
    public:
        DebugOverlay();

        DebugOverlay(const DebugOverlay&) = delete;
        DebugOverlay& operator=(const DebugOverlay&) = delete;
        DebugOverlay(DebugOverlay&&) = delete;
        DebugOverlay& operator=(DebugOverlay&&) = delete;

        bool loadFontFromFile(const std::string& filepath);
        // Invalid handles are rejected without changing the current binding.
        bool setFont(FontHandle font);
        FontHandle fontHandle() const;
        void clearFont();

        bool hasFont() const;

        void setText(const std::string& text);
        // Invalid or out-of-domain positions are rejected transactionally.
        void setPosition(sf::Vector2f position);
        sf::Vector2f position() const;
        // Character size is kept nonzero.
        void setCharacterSize(unsigned int size);
        unsigned int characterSize() const;
        void setFillColor(sf::Color color);

        void render(sf::RenderWindow& window) const;

    private:
        // The lease must outlive the SFML drawable that borrows from it.
        FontHandle m_font;
        sf::Text m_text;

        bool m_hasFont;
    };
}

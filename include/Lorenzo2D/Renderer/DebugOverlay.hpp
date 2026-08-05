#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
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
        bool setLiveFont(LiveFontHandle font);
        LiveFontHandle liveFontHandle() const;
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
        void syncLiveFont() const;

        // The lease must outlive the SFML drawable that borrows from it.
        mutable FontHandle m_font;
        LiveFontHandle m_liveFont;
        mutable std::uint64_t m_liveGeneration = 0;
        mutable sf::Text m_text;

        mutable bool m_hasFont;
    };
}

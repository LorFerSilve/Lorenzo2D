#pragma once

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

        bool loadFontFromFile(const std::string& filepath);
        void setFont(const sf::Font& font);

        bool hasFont() const;

        void setText(const std::string& text);
        void setPosition(sf::Vector2f position);
        void setCharacterSize(unsigned int size);
        void setFillColor(sf::Color color);

        void render(sf::RenderWindow& window) const;

    private:
        sf::Font m_ownedFont;
        sf::Text m_text;

        bool m_hasFont;
    };
}
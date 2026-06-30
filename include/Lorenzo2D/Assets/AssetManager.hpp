#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <unordered_map>

namespace l2d
{
    class AssetManager
    {
    public:
        bool loadFont(const std::string& name, const std::string& filepath);

        sf::Font* getFont(const std::string& name);
        const sf::Font* getFont(const std::string& name) const;

        bool hasFont(const std::string& name) const;

        bool loadTexture(const std::string& name, const std::string& filepath, bool smooth = true);

        sf::Texture* getTexture(const std::string& name);
        const sf::Texture* getTexture(const std::string& name) const;

        bool hasTexture(const std::string& name) const;

        void clearFonts();
        void clearTextures();
        void clearAll();

    private:
        std::unordered_map<std::string, sf::Font> m_fonts;
        std::unordered_map<std::string, sf::Texture> m_textures;
    };
}
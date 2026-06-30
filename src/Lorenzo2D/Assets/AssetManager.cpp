#include <Lorenzo2D/Assets/AssetManager.hpp>

namespace l2d
{
    bool AssetManager::loadFont(const std::string& name, const std::string& filepath)
    {
        sf::Font font;

        if (!font.openFromFile(filepath))
        {
            return false;
        }

        m_fonts[name] = std::move(font);
        return true;
    }

    sf::Font* AssetManager::getFont(const std::string& name)
    {
        auto it = m_fonts.find(name);

        if (it == m_fonts.end())
            return nullptr;

        return &it->second;
    }

    const sf::Font* AssetManager::getFont(const std::string& name) const
    {
        auto it = m_fonts.find(name);

        if (it == m_fonts.end())
            return nullptr;

        return &it->second;
    }

    bool AssetManager::hasFont(const std::string& name) const
    {
        return m_fonts.find(name) != m_fonts.end();
    }

    bool AssetManager::loadTexture(const std::string& name, const std::string& filepath, bool smooth)
    {
        sf::Texture texture;

        if (!texture.loadFromFile(filepath))
        {
            return false;
        }

        texture.setSmooth(smooth);

        m_textures[name] = std::move(texture);
        return true;
    }

    sf::Texture* AssetManager::getTexture(const std::string& name)
    {
        auto it = m_textures.find(name);

        if (it == m_textures.end())
            return nullptr;

        return &it->second;
    }

    const sf::Texture* AssetManager::getTexture(const std::string& name) const
    {
        auto it = m_textures.find(name);

        if (it == m_textures.end())
            return nullptr;

        return &it->second;
    }

    bool AssetManager::hasTexture(const std::string& name) const
    {
        return m_textures.find(name) != m_textures.end();
    }

    void AssetManager::clearFonts()
    {
        m_fonts.clear();
    }

    void AssetManager::clearTextures()
    {
        m_textures.clear();
    }

    void AssetManager::clearAll()
    {
        clearFonts();
        clearTextures();
    }
}
#include <Lorenzo2D/Assets/AssetManager.hpp>

#include <memory>
#include <utility>

namespace l2d
{
    bool AssetManager::loadFont(
        const std::string& name,
        const std::string& filepath
    )
    {
        std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();

        if (!font->openFromFile(filepath))
            return false;

        return storeFont(name, FontHandle(std::move(font)));
    }

    bool AssetManager::storeFont(
        const std::string& name,
        FontHandle font
    )
    {
        if (!font)
            return false;

        m_fonts.insert_or_assign(name, std::move(font));
        return true;
    }

    FontHandle AssetManager::getFont(const std::string& name) const
    {
        const auto iterator = m_fonts.find(name);

        if (iterator == m_fonts.end())
            return {};

        return iterator->second;
    }

    bool AssetManager::hasFont(const std::string& name) const
    {
        return m_fonts.find(name) != m_fonts.end();
    }

    bool AssetManager::unloadFont(const std::string& name)
    {
        return m_fonts.erase(name) > 0u;
    }

    std::size_t AssetManager::fontCount() const
    {
        return m_fonts.size();
    }

    bool AssetManager::loadTexture(
        const std::string& name,
        const std::string& filepath,
        bool smooth
    )
    {
        std::shared_ptr<sf::Texture> texture =
            std::make_shared<sf::Texture>();

        if (!texture->loadFromFile(filepath))
            return false;

        texture->setSmooth(smooth);
        return storeTexture(name, TextureHandle(std::move(texture)));
    }

    bool AssetManager::storeTexture(
        const std::string& name,
        TextureHandle texture
    )
    {
        if (!texture)
            return false;

        m_textures.insert_or_assign(name, std::move(texture));
        return true;
    }

    TextureHandle AssetManager::getTexture(const std::string& name) const
    {
        const auto iterator = m_textures.find(name);

        if (iterator == m_textures.end())
            return {};

        return iterator->second;
    }

    bool AssetManager::hasTexture(const std::string& name) const
    {
        return m_textures.find(name) != m_textures.end();
    }

    bool AssetManager::unloadTexture(const std::string& name)
    {
        return m_textures.erase(name) > 0u;
    }

    std::size_t AssetManager::textureCount() const
    {
        return m_textures.size();
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

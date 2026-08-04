#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace l2d
{
    class AssetManager
    {
      public:
        AssetManager() = default;
        ~AssetManager() = default;

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        AssetManager(AssetManager&&) noexcept = default;
        AssetManager& operator=(AssetManager&&) noexcept = default;

        // Loading is transactional: a failure leaves any existing asset with
        // the same name unchanged.
        bool loadFont(const std::string& name, const std::string& filepath);
        bool storeFont(const std::string& name, FontHandle font);

        FontHandle getFont(const std::string& name) const;

        bool hasFont(const std::string& name) const;
        bool unloadFont(const std::string& name);
        std::size_t fontCount() const;

        bool loadTexture(const std::string& name, const std::string& filepath, bool smooth = true);
        bool storeTexture(const std::string& name, TextureHandle texture);

        TextureHandle getTexture(const std::string& name) const;

        bool hasTexture(const std::string& name) const;
        bool unloadTexture(const std::string& name);
        std::size_t textureCount() const;

        // Clearing or destroying the manager releases only registry ownership.
        // Existing handles keep their asset generations alive.
        void clearFonts();
        void clearTextures();
        void clearAll();

      private:
        std::unordered_map<std::string, FontHandle> m_fonts;
        std::unordered_map<std::string, TextureHandle> m_textures;
    };
}

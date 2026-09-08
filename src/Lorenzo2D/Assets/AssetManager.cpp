#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <memory>
#include <utility>

namespace l2d
{
    bool AssetManager::loadFont(const std::string& name, const std::string& filepath)
    {
        std::shared_ptr<sf::Font> font = std::make_shared<sf::Font>();

        if (!font->openFromFile(filepath)) return false;

        return storeFont(name, FontHandle(std::move(font)));
    }

    bool AssetManager::loadFont(const std::string& name, const ResourceLocator& locator,
                                const std::string& resource)
    {
        const std::optional<ResourceLocator::Path> path = locator.locate(resource);
        return path && loadFont(name, path->string());
    }

    bool AssetManager::storeFont(const std::string& name, FontHandle font)
    {
        if (!font) return false;

        m_fonts.insert_or_assign(name, font);

        const auto live = m_liveFonts.find(name);

        if (live != m_liveFonts.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current = std::move(font);
            ++live->second->generation;
        }
        return true;
    }

    FontHandle AssetManager::getFont(const std::string& name) const
    {
        const auto iterator = m_fonts.find(name);

        if (iterator == m_fonts.end()) return {};

        return iterator->second;
    }

    LiveFontHandle AssetManager::liveFont(const std::string& name)
    {
        auto& state = m_liveFonts[name];

        if (!state)
        {
            state = std::make_shared<detail::LiveAssetState<sf::Font>>();
            state->current = getFont(name);
            state->generation = state->current ? 1u : 0u;
        }

        return LiveFontHandle(state);
    }

    bool AssetManager::hasFont(const std::string& name) const
    {
        return m_fonts.find(name) != m_fonts.end();
    }

    bool AssetManager::unloadFont(const std::string& name)
    {
        if (m_fonts.erase(name) == 0u) return false;

        const auto live = m_liveFonts.find(name);

        if (live != m_liveFonts.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        return true;
    }

    std::size_t AssetManager::fontCount() const
    {
        return m_fonts.size();
    }

    bool AssetManager::loadTexture(const std::string& name, const std::string& filepath,
                                   bool smooth)
    {
        std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();

        if (!texture->loadFromFile(filepath)) return false;

        texture->setSmooth(smooth);
        return storeTexture(name, TextureHandle(std::move(texture)));
    }

    bool AssetManager::loadTexture(const std::string& name, const ResourceLocator& locator,
                                   const std::string& resource, bool smooth)
    {
        const std::optional<ResourceLocator::Path> path = locator.locate(resource);
        return path && loadTexture(name, path->string(), smooth);
    }

    bool AssetManager::storeTexture(const std::string& name, TextureHandle texture)
    {
        if (!texture) return false;

        m_textures.insert_or_assign(name, texture);

        const auto live = m_liveTextures.find(name);

        if (live != m_liveTextures.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current = std::move(texture);
            ++live->second->generation;
        }
        return true;
    }

    TextureHandle AssetManager::getTexture(const std::string& name) const
    {
        const auto iterator = m_textures.find(name);

        if (iterator == m_textures.end()) return {};

        return iterator->second;
    }

    LiveTextureHandle AssetManager::liveTexture(const std::string& name)
    {
        auto& state = m_liveTextures[name];

        if (!state)
        {
            state = std::make_shared<detail::LiveAssetState<sf::Texture>>();
            state->current = getTexture(name);
            state->generation = state->current ? 1u : 0u;
        }

        return LiveTextureHandle(state);
    }

    bool AssetManager::hasTexture(const std::string& name) const
    {
        return m_textures.find(name) != m_textures.end();
    }

    bool AssetManager::unloadTexture(const std::string& name)
    {
        if (m_textures.erase(name) == 0u) return false;

        const auto live = m_liveTextures.find(name);

        if (live != m_liveTextures.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        return true;
    }

    std::size_t AssetManager::textureCount() const
    {
        return m_textures.size();
    }

    bool AssetManager::loadSoundBuffer(const std::string& name, const std::string& filepath)
    {
        std::shared_ptr<sf::SoundBuffer> buffer = std::make_shared<sf::SoundBuffer>();

        if (!buffer->loadFromFile(filepath)) return false;

        return storeSoundBuffer(name, SoundBufferHandle(std::move(buffer)));
    }

    bool AssetManager::loadSoundBuffer(const std::string& name, const ResourceLocator& locator,
                                       const std::string& resource)
    {
        const std::optional<ResourceLocator::Path> path = locator.locate(resource);
        return path && loadSoundBuffer(name, path->string());
    }

    bool AssetManager::storeSoundBuffer(const std::string& name, SoundBufferHandle buffer)
    {
        if (name.empty() || !buffer) return false;

        m_soundBuffers.insert_or_assign(name, buffer);
        const auto live = m_liveSoundBuffers.find(name);

        if (live != m_liveSoundBuffers.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current = std::move(buffer);
            ++live->second->generation;
        }

        return true;
    }

    SoundBufferHandle AssetManager::getSoundBuffer(const std::string& name) const
    {
        const auto iterator = m_soundBuffers.find(name);
        return iterator == m_soundBuffers.end() ? SoundBufferHandle{} : iterator->second;
    }

    LiveSoundBufferHandle AssetManager::liveSoundBuffer(const std::string& name)
    {
        auto& state = m_liveSoundBuffers[name];

        if (!state)
        {
            state = std::make_shared<detail::LiveAssetState<sf::SoundBuffer>>();
            state->current = getSoundBuffer(name);
            state->generation = state->current ? 1u : 0u;
        }

        return LiveSoundBufferHandle(state);
    }

    bool AssetManager::hasSoundBuffer(const std::string& name) const
    {
        return m_soundBuffers.find(name) != m_soundBuffers.end();
    }

    bool AssetManager::unloadSoundBuffer(const std::string& name)
    {
        if (m_soundBuffers.erase(name) == 0u) return false;

        const auto live = m_liveSoundBuffers.find(name);

        if (live != m_liveSoundBuffers.end())
        {
            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        return true;
    }

    std::size_t AssetManager::soundBufferCount() const
    {
        return m_soundBuffers.size();
    }

    bool AssetManager::storeAnimationClip(const std::string& name, AnimationClipHandle clip)
    {
        if (name.empty() || !clip) return false;

        m_animationClips.insert_or_assign(name, std::move(clip));
        return true;
    }

    AnimationClipHandle AssetManager::getAnimationClip(const std::string& name) const
    {
        const auto iterator = m_animationClips.find(name);
        return iterator == m_animationClips.end() ? AnimationClipHandle{} : iterator->second;
    }

    bool AssetManager::hasAnimationClip(const std::string& name) const
    {
        return m_animationClips.find(name) != m_animationClips.end();
    }

    bool AssetManager::unloadAnimationClip(const std::string& name)
    {
        return m_animationClips.erase(name) != 0u;
    }

    std::size_t AssetManager::animationClipCount() const
    {
        return m_animationClips.size();
    }

    std::size_t AssetManager::loadedAssetCount() const noexcept
    {
        return m_fonts.size() + m_textures.size() + m_soundBuffers.size() + m_animationClips.size();
    }

    std::size_t AssetManager::liveAssetSlotCount() const noexcept
    {
        return m_liveFonts.size() + m_liveTextures.size() + m_liveSoundBuffers.size();
    }

    void AssetManager::clearFonts()
    {
        for (const auto& entry : m_fonts)
        {
            const auto live = m_liveFonts.find(entry.first);

            if (live == m_liveFonts.end()) continue;

            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        m_fonts.clear();
    }

    void AssetManager::clearTextures()
    {
        for (const auto& entry : m_textures)
        {
            const auto live = m_liveTextures.find(entry.first);

            if (live == m_liveTextures.end()) continue;

            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        m_textures.clear();
    }

    void AssetManager::clearSoundBuffers()
    {
        for (const auto& entry : m_soundBuffers)
        {
            const auto live = m_liveSoundBuffers.find(entry.first);

            if (live == m_liveSoundBuffers.end()) continue;

            const std::lock_guard<std::mutex> lock(live->second->mutex);
            live->second->current.reset();
            ++live->second->generation;
        }

        m_soundBuffers.clear();
    }

    void AssetManager::clearAnimationClips()
    {
        m_animationClips.clear();
    }

    void AssetManager::clearAll()
    {
        clearFonts();
        clearTextures();
        clearSoundBuffers();
        clearAnimationClips();
    }
}

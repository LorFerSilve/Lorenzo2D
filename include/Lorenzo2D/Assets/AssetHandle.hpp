#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace sf
{
    class Font;
    class SoundBuffer;
    class Texture;
}

namespace l2d
{
    class AnimationClip;
    // A read-only shared lease for an asset. Registry removal or replacement
    // cannot invalidate a resource while at least one handle still owns it.
    template <typename Asset> class AssetHandle
    {
      public:
        AssetHandle() noexcept = default;

        explicit AssetHandle(std::shared_ptr<const Asset> asset) noexcept
            : m_asset(std::move(asset))
        {
        }

        const Asset* get() const noexcept
        {
            return m_asset.get();
        }

        const Asset& operator*() const noexcept
        {
            return *m_asset;
        }

        const Asset* operator->() const noexcept
        {
            return m_asset.get();
        }

        explicit operator bool() const noexcept
        {
            return m_asset != nullptr;
        }

        void reset() noexcept
        {
            m_asset.reset();
        }

        void swap(AssetHandle& other) noexcept
        {
            m_asset.swap(other.m_asset);
        }

        friend bool operator==(const AssetHandle& left, const AssetHandle& right) noexcept
        {
            return left.get() == right.get();
        }

        friend bool operator!=(const AssetHandle& left, const AssetHandle& right) noexcept
        {
            return !(left == right);
        }

        friend bool operator==(const AssetHandle& handle, std::nullptr_t) noexcept
        {
            return !handle;
        }

        friend bool operator==(std::nullptr_t, const AssetHandle& handle) noexcept
        {
            return !handle;
        }

        friend bool operator!=(const AssetHandle& handle, std::nullptr_t) noexcept
        {
            return static_cast<bool>(handle);
        }

        friend bool operator!=(std::nullptr_t, const AssetHandle& handle) noexcept
        {
            return static_cast<bool>(handle);
        }

      private:
        std::shared_ptr<const Asset> m_asset;
    };

    template <typename Asset>
    void swap(AssetHandle<Asset>& left, AssetHandle<Asset>& right) noexcept
    {
        left.swap(right);
    }

    namespace detail
    {
        template <typename Asset> struct LiveAssetState
        {
            mutable std::mutex mutex;
            AssetHandle<Asset> current;
            std::uint64_t generation = 0;
        };
    }

    // An opt-in binding to a named registry slot. snapshot() returns a safe
    // immutable lease for the slot's current generation; ordinary AssetHandle
    // values retain their original snapshot semantics.
    template <typename Asset> class LiveAssetHandle
    {
      public:
        LiveAssetHandle() noexcept = default;

        AssetHandle<Asset> snapshot() const
        {
            if (!m_state) return {};

            const std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->current;
        }

        std::uint64_t generation() const
        {
            if (!m_state) return 0;

            const std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->generation;
        }

        explicit operator bool() const noexcept
        {
            return m_state != nullptr;
        }

        void reset() noexcept
        {
            m_state.reset();
        }

      private:
        explicit LiveAssetHandle(std::shared_ptr<detail::LiveAssetState<Asset>> state) noexcept
            : m_state(std::move(state))
        {
        }

        std::shared_ptr<detail::LiveAssetState<Asset>> m_state;

        friend class AssetManager;
    };

    using FontHandle = AssetHandle<sf::Font>;
    using SoundBufferHandle = AssetHandle<sf::SoundBuffer>;
    using TextureHandle = AssetHandle<sf::Texture>;
    using AnimationClipHandle = AssetHandle<AnimationClip>;
    using LiveFontHandle = LiveAssetHandle<sf::Font>;
    using LiveSoundBufferHandle = LiveAssetHandle<sf::SoundBuffer>;
    using LiveTextureHandle = LiveAssetHandle<sf::Texture>;
}

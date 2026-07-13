#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace sf
{
    class Font;
    class Texture;
}

namespace l2d
{
    // A read-only shared lease for an asset. Registry removal or replacement
    // cannot invalidate a resource while at least one handle still owns it.
    template <typename Asset>
    class AssetHandle
    {
    public:
        AssetHandle() noexcept = default;

        explicit AssetHandle(
            std::shared_ptr<const Asset> asset
        ) noexcept
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

        friend bool operator==(
            const AssetHandle& left,
            const AssetHandle& right
        ) noexcept
        {
            return left.get() == right.get();
        }

        friend bool operator!=(
            const AssetHandle& left,
            const AssetHandle& right
        ) noexcept
        {
            return !(left == right);
        }

        friend bool operator==(
            const AssetHandle& handle,
            std::nullptr_t
        ) noexcept
        {
            return !handle;
        }

        friend bool operator==(
            std::nullptr_t,
            const AssetHandle& handle
        ) noexcept
        {
            return !handle;
        }

        friend bool operator!=(
            const AssetHandle& handle,
            std::nullptr_t
        ) noexcept
        {
            return static_cast<bool>(handle);
        }

        friend bool operator!=(
            std::nullptr_t,
            const AssetHandle& handle
        ) noexcept
        {
            return static_cast<bool>(handle);
        }

    private:
        std::shared_ptr<const Asset> m_asset;
    };

    template <typename Asset>
    void swap(
        AssetHandle<Asset>& left,
        AssetHandle<Asset>& right
    ) noexcept
    {
        left.swap(right);
    }

    using FontHandle = AssetHandle<sf::Font>;
    using TextureHandle = AssetHandle<sf::Texture>;
}

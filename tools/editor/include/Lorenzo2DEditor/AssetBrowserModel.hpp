#pragma once

#include <Lorenzo2D/Assets/AssetId.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace l2d_editor
{
    enum class AssetBrowserEntryKind : std::uint8_t
    {
        Generic,
        Texture
    };

    enum class AssetBrowserError : std::uint8_t
    {
        None,
        InvalidConfiguration,
        RootIsNotDirectory,
        ScanLimitExceeded,
        AssetLimitExceeded,
        FilesystemError
    };

    [[nodiscard]] std::string_view assetBrowserErrorMessage(AssetBrowserError error) noexcept;

    struct AssetBrowserLimits
    {
        std::size_t maxScannedEntries = 8192u;
        std::size_t maxAssets = 2048u;
        std::size_t maxDepth = 8u;
    };

    struct AssetBrowserEntry
    {
        l2d::AssetId id;
        std::filesystem::path absolutePath;
        std::size_t rootIndex = 0u;
        AssetBrowserEntryKind kind = AssetBrowserEntryKind::Generic;
    };

    // Editor-only resource-root browser. Successful refreshes are deterministic and
    // transactional: the complete bounded scan is sorted by AssetId before publication,
    // while a filesystem or configured-limit failure leaves the previous index intact.
    class AssetBrowserModel
    {
      public:
        explicit AssetBrowserModel(AssetBrowserLimits limits = {});

        [[nodiscard]] bool setRoots(const std::vector<std::filesystem::path>& roots);
        [[nodiscard]] bool setRoots(const l2d::ResourceLocator& locator);
        [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept;

        [[nodiscard]] bool refresh();
        [[nodiscard]] AssetBrowserError lastError() const noexcept;
        [[nodiscard]] const AssetBrowserLimits& limits() const noexcept;

        void setFilter(std::string filter);
        [[nodiscard]] std::string_view filter() const noexcept;

        [[nodiscard]] const std::vector<AssetBrowserEntry>& entries() const noexcept;
        [[nodiscard]] std::size_t visibleCount() const noexcept;
        [[nodiscard]] const AssetBrowserEntry* visibleEntry(std::size_t index) const noexcept;

        [[nodiscard]] bool selectVisibleIndex(std::size_t index);
        void clearSelection() noexcept;
        [[nodiscard]] const AssetBrowserEntry* selectedEntry() const noexcept;
        [[nodiscard]] std::optional<l2d::AssetId> selectedAssetId() const;

        // Resolves only portable relative AssetIds through the configured runtime
        // ResourceLocator contract. Absolute paths and traversal components are rejected.
        [[nodiscard]] std::optional<std::filesystem::path>
        resolveAssetId(const l2d::AssetId& id) const;
        [[nodiscard]] bool isSelectedResolvable() const;

        [[nodiscard]] static bool isPortableAssetId(std::string_view id) noexcept;
        [[nodiscard]] static AssetBrowserEntryKind classify(const l2d::AssetId& id) noexcept;

      private:
        [[nodiscard]] bool configurationIsValid() const noexcept;
        void rebuildVisible();
        void setError(AssetBrowserError error) noexcept;

        AssetBrowserLimits m_limits;
        std::vector<std::filesystem::path> m_roots;
        std::vector<AssetBrowserEntry> m_entries;
        std::vector<std::size_t> m_visibleIndices;
        std::string m_filter;
        std::optional<l2d::AssetId> m_selectedAssetId;
        AssetBrowserError m_lastError = AssetBrowserError::None;
    };
}

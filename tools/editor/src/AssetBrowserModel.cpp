#include <Lorenzo2DEditor/AssetBrowserModel.hpp>

#include <algorithm>
#include <system_error>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        char asciiLower(char value) noexcept
        {
            if (value >= 'A' && value <= 'Z')
                return static_cast<char>(value + static_cast<char>('a' - 'A'));
            return value;
        }

        bool equalsIgnoreAsciiCase(std::string_view left, std::string_view right) noexcept
        {
            if (left.size() != right.size()) return false;
            for (std::size_t index = 0u; index < left.size(); ++index)
            {
                if (asciiLower(left[index]) != asciiLower(right[index])) return false;
            }
            return true;
        }

        bool containsIgnoreAsciiCase(std::string_view value, std::string_view query) noexcept
        {
            if (query.empty()) return true;
            if (query.size() > value.size()) return false;

            for (std::size_t start = 0u; start + query.size() <= value.size(); ++start)
            {
                bool matches = true;
                for (std::size_t offset = 0u; offset < query.size(); ++offset)
                {
                    if (asciiLower(value[start + offset]) != asciiLower(query[offset]))
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches) return true;
            }
            return false;
        }
    }

    std::string_view assetBrowserErrorMessage(AssetBrowserError error) noexcept
    {
        switch (error)
        {
        case AssetBrowserError::None:
            return "none";
        case AssetBrowserError::InvalidConfiguration:
            return "invalid browser configuration";
        case AssetBrowserError::RootIsNotDirectory:
            return "configured resource root is not a directory";
        case AssetBrowserError::ScanLimitExceeded:
            return "asset scan limit exceeded";
        case AssetBrowserError::AssetLimitExceeded:
            return "published asset limit exceeded";
        case AssetBrowserError::FilesystemError:
            return "filesystem error while scanning resource roots";
        }
        return "unknown asset browser error";
    }

    AssetBrowserModel::AssetBrowserModel(AssetBrowserLimits limits) : m_limits(limits)
    {
        if (!configurationIsValid()) m_lastError = AssetBrowserError::InvalidConfiguration;
    }

    bool AssetBrowserModel::setRoots(const std::vector<std::filesystem::path>& roots)
    {
        if (!configurationIsValid())
        {
            setError(AssetBrowserError::InvalidConfiguration);
            return false;
        }

        l2d::ResourceLocator locator;
        for (const std::filesystem::path& root : roots)
        {
            if (!locator.addRoot(root))
            {
                setError(AssetBrowserError::InvalidConfiguration);
                return false;
            }
        }

        return setRoots(locator);
    }

    bool AssetBrowserModel::setRoots(const l2d::ResourceLocator& locator)
    {
        if (!configurationIsValid())
        {
            setError(AssetBrowserError::InvalidConfiguration);
            return false;
        }

        m_roots = locator.roots();
        m_entries.clear();
        m_visibleIndices.clear();
        m_selectedAssetId.reset();
        setError(AssetBrowserError::None);
        return true;
    }

    const std::vector<std::filesystem::path>& AssetBrowserModel::roots() const noexcept
    {
        return m_roots;
    }

    bool AssetBrowserModel::refresh()
    {
        if (!configurationIsValid())
        {
            setError(AssetBrowserError::InvalidConfiguration);
            return false;
        }

        l2d::ResourceLocator locator;
        for (const std::filesystem::path& root : m_roots)
        {
            if (!locator.addRoot(root))
            {
                setError(AssetBrowserError::InvalidConfiguration);
                return false;
            }
        }

        std::vector<AssetBrowserEntry> candidates;
        candidates.reserve(std::min(m_limits.maxAssets, static_cast<std::size_t>(256u)));
        std::size_t scannedEntries = 0u;

        for (std::size_t rootIndex = 0u; rootIndex < m_roots.size(); ++rootIndex)
        {
            const std::filesystem::path& root = m_roots[rootIndex];
            std::error_code error;
            const bool exists = std::filesystem::exists(root, error);
            if (error)
            {
                setError(AssetBrowserError::FilesystemError);
                return false;
            }
            if (!exists) continue;

            const bool directory = std::filesystem::is_directory(root, error);
            if (error)
            {
                setError(AssetBrowserError::FilesystemError);
                return false;
            }
            if (!directory)
            {
                setError(AssetBrowserError::RootIsNotDirectory);
                return false;
            }

            std::filesystem::recursive_directory_iterator iterator(
                root, std::filesystem::directory_options::skip_permission_denied, error);
            const std::filesystem::recursive_directory_iterator end;
            if (error)
            {
                setError(AssetBrowserError::FilesystemError);
                return false;
            }

            while (iterator != end)
            {
                if (scannedEntries >= m_limits.maxScannedEntries)
                {
                    setError(AssetBrowserError::ScanLimitExceeded);
                    return false;
                }
                ++scannedEntries;

                const std::filesystem::directory_entry entry = *iterator;
                error.clear();
                const std::filesystem::file_status status = entry.symlink_status(error);
                if (error)
                {
                    setError(AssetBrowserError::FilesystemError);
                    return false;
                }

                const std::size_t depth = static_cast<std::size_t>(iterator.depth());
                if (std::filesystem::is_directory(status) && depth >= m_limits.maxDepth)
                    iterator.disable_recursion_pending();

                if (!std::filesystem::is_symlink(status) &&
                    std::filesystem::is_regular_file(status))
                {
                    const std::filesystem::path relative = entry.path().lexically_relative(root);
                    const l2d::AssetId id = relative.generic_string();
                    if (isPortableAssetId(id))
                    {
                        const std::optional<std::filesystem::path> resolved = locator.locate(relative);
                        if (resolved && resolved->lexically_normal() == entry.path().lexically_normal())
                        {
                            candidates.push_back(
                                {id, entry.path().lexically_normal(), rootIndex, classify(id)});
                            if (candidates.size() > m_limits.maxAssets)
                            {
                                setError(AssetBrowserError::AssetLimitExceeded);
                                return false;
                            }
                        }
                    }
                }

                error.clear();
                iterator.increment(error);
                if (error)
                {
                    setError(AssetBrowserError::FilesystemError);
                    return false;
                }
            }
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const AssetBrowserEntry& left, const AssetBrowserEntry& right)
                  {
                      if (left.id != right.id) return left.id < right.id;
                      if (left.rootIndex != right.rootIndex) return left.rootIndex < right.rootIndex;
                      return left.absolutePath.generic_string() < right.absolutePath.generic_string();
                  });

        candidates.erase(
            std::unique(candidates.begin(), candidates.end(),
                        [](const AssetBrowserEntry& left, const AssetBrowserEntry& right)
                        { return left.id == right.id; }),
            candidates.end());

        const std::optional<l2d::AssetId> previousSelection = m_selectedAssetId;
        m_entries = std::move(candidates);
        m_selectedAssetId = previousSelection;
        rebuildVisible();
        setError(AssetBrowserError::None);
        return true;
    }

    AssetBrowserError AssetBrowserModel::lastError() const noexcept
    {
        return m_lastError;
    }

    const AssetBrowserLimits& AssetBrowserModel::limits() const noexcept
    {
        return m_limits;
    }

    void AssetBrowserModel::setFilter(std::string filter)
    {
        m_filter = std::move(filter);
        rebuildVisible();
    }

    std::string_view AssetBrowserModel::filter() const noexcept
    {
        return m_filter;
    }

    const std::vector<AssetBrowserEntry>& AssetBrowserModel::entries() const noexcept
    {
        return m_entries;
    }

    std::size_t AssetBrowserModel::visibleCount() const noexcept
    {
        return m_visibleIndices.size();
    }

    const AssetBrowserEntry* AssetBrowserModel::visibleEntry(std::size_t index) const noexcept
    {
        if (index >= m_visibleIndices.size()) return nullptr;
        return &m_entries[m_visibleIndices[index]];
    }

    bool AssetBrowserModel::selectVisibleIndex(std::size_t index)
    {
        const AssetBrowserEntry* entry = visibleEntry(index);
        if (entry == nullptr) return false;
        if (m_selectedAssetId && *m_selectedAssetId == entry->id) return false;
        m_selectedAssetId = entry->id;
        return true;
    }

    void AssetBrowserModel::clearSelection() noexcept
    {
        m_selectedAssetId.reset();
    }

    const AssetBrowserEntry* AssetBrowserModel::selectedEntry() const noexcept
    {
        if (!m_selectedAssetId) return nullptr;
        const auto iterator = std::lower_bound(
            m_entries.begin(), m_entries.end(), *m_selectedAssetId,
            [](const AssetBrowserEntry& entry, const l2d::AssetId& id) { return entry.id < id; });
        if (iterator == m_entries.end() || iterator->id != *m_selectedAssetId) return nullptr;
        return &*iterator;
    }

    std::optional<l2d::AssetId> AssetBrowserModel::selectedAssetId() const
    {
        return m_selectedAssetId;
    }

    std::optional<std::filesystem::path>
    AssetBrowserModel::resolveAssetId(const l2d::AssetId& id) const
    {
        if (!isPortableAssetId(id)) return std::nullopt;

        l2d::ResourceLocator locator;
        for (const std::filesystem::path& root : m_roots)
            if (!locator.addRoot(root)) return std::nullopt;

        const std::optional<std::filesystem::path> resolved = locator.locate(id);
        if (!resolved) return std::nullopt;

        std::error_code error;
        const bool regularFile = std::filesystem::is_regular_file(*resolved, error);
        if (error || !regularFile) return std::nullopt;
        return resolved->lexically_normal();
    }

    bool AssetBrowserModel::isSelectedResolvable() const
    {
        const AssetBrowserEntry* entry = selectedEntry();
        if (entry == nullptr) return false;
        const std::optional<std::filesystem::path> resolved = resolveAssetId(entry->id);
        return resolved && resolved->lexically_normal() == entry->absolutePath.lexically_normal();
    }

    bool AssetBrowserModel::isPortableAssetId(std::string_view id) noexcept
    {
        if (id.empty() || id.front() == '/' || id.front() == '\\') return false;
        if (id.find('\n') != std::string_view::npos || id.find('\r') != std::string_view::npos ||
            id.find('\\') != std::string_view::npos)
            return false;
        if (id.size() >= 2u && id[1] == ':') return false;

        std::size_t start = 0u;
        while (start < id.size())
        {
            const std::size_t separator = id.find('/', start);
            const std::size_t length = separator == std::string_view::npos
                                           ? id.size() - start
                                           : separator - start;
            const std::string_view component = id.substr(start, length);
            if (component.empty() || component == "." || component == "..") return false;
            if (separator == std::string_view::npos) break;
            start = separator + 1u;
        }
        return true;
    }

    AssetBrowserEntryKind AssetBrowserModel::classify(const l2d::AssetId& id) noexcept
    {
        const std::string_view value(id);
        const std::size_t slash = value.find_last_of('/');
        const std::size_t dot = value.find_last_of('.');
        if (dot == std::string_view::npos ||
            (slash != std::string_view::npos && dot < slash))
            return AssetBrowserEntryKind::Generic;

        const std::string_view extension = value.substr(dot);
        constexpr std::string_view textureExtensions[] = {".bmp", ".png", ".tga", ".jpg", ".jpeg",
                                                           ".gif", ".psd", ".hdr", ".pic", ".pnm"};
        for (std::string_view candidate : textureExtensions)
            if (equalsIgnoreAsciiCase(extension, candidate)) return AssetBrowserEntryKind::Texture;
        return AssetBrowserEntryKind::Generic;
    }

    bool AssetBrowserModel::configurationIsValid() const noexcept
    {
        return m_limits.maxScannedEntries > 0u && m_limits.maxAssets > 0u;
    }

    void AssetBrowserModel::rebuildVisible()
    {
        m_visibleIndices.clear();
        m_visibleIndices.reserve(m_entries.size());
        for (std::size_t index = 0u; index < m_entries.size(); ++index)
        {
            if (containsIgnoreAsciiCase(m_entries[index].id, m_filter))
                m_visibleIndices.push_back(index);
        }

        if (!m_selectedAssetId) return;
        const bool selectedVisible =
            std::any_of(m_visibleIndices.begin(), m_visibleIndices.end(),
                        [this](std::size_t index)
                        { return m_entries[index].id == *m_selectedAssetId; });
        if (!selectedVisible) m_selectedAssetId.reset();
    }

    void AssetBrowserModel::setError(AssetBrowserError error) noexcept
    {
        m_lastError = error;
    }
}

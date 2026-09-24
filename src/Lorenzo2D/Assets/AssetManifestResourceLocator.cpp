#include <Lorenzo2D/Assets/AssetManifestResourceLocator.hpp>

#include <system_error>
#include <utility>

namespace l2d
{
    AssetManifestResourceLocator::AssetManifestResourceLocator(AssetManifest manifest)
        : m_manifest(std::move(manifest))
    {
    }

    void AssetManifestResourceLocator::setManifest(AssetManifest manifest)
    {
        m_manifest = std::move(manifest);
    }

    const AssetManifest& AssetManifestResourceLocator::manifest() const noexcept
    {
        return m_manifest;
    }

    bool AssetManifestResourceLocator::addRoot(Path root, std::string* error)
    {
        const std::size_t previousSize = m_resources.roots().size();
        if (!m_resources.addRoot(root))
        {
            return fail(error, "manifest resource root could not be normalized");
        }

        if (m_resources.roots().size() > MaxRoots)
        {
            m_resources.removeRoot(root);
            return fail(error, "manifest resource locator reached its configured root limit");
        }

        if (m_resources.roots().size() < previousSize)
        {
            return fail(error, "manifest resource root state became inconsistent");
        }

        clearError(error);
        return true;
    }

    bool AssetManifestResourceLocator::removeRoot(const Path& root)
    {
        return m_resources.removeRoot(root);
    }

    void AssetManifestResourceLocator::clearRoots()
    {
        m_resources.clear();
    }

    const std::vector<AssetManifestResourceLocator::Path>& AssetManifestResourceLocator::roots()
        const noexcept
    {
        return m_resources.roots();
    }

    bool AssetManifestResourceLocator::resolve(const AssetId& id,
                                               const AssetSourceKind expectedKind,
                                               AssetManifestResource& output,
                                               std::string* error) const
    {
        if (!AssetMetadataRegistry::isValidAssetId(id))
        {
            return fail(error, "manifest resource id is invalid");
        }

        const AssetManifestEntry* entry = m_manifest.find(id);
        if (entry == nullptr)
        {
            return fail(error, "manifest resource id is not present in the manifest");
        }

        std::string validationError;
        if (!AssetManifest::validate(*entry, &validationError))
        {
            return fail(error, "manifest resource entry is invalid: " + validationError);
        }

        if (expectedKind != AssetSourceKind::Unknown && entry->source.kind != expectedKind)
        {
            return fail(error, "manifest resource kind does not match the requested runtime type");
        }

        const std::optional<Path> located = m_resources.locate(entry->cookedPath);
        if (!located)
        {
            return fail(error, "manifest cooked artifact could not be located in configured roots");
        }

        std::error_code filesystemError;
        if (!std::filesystem::is_regular_file(*located, filesystemError) || filesystemError)
        {
            return fail(error, "manifest cooked artifact is not a regular file");
        }

        AssetManifestResource candidate;
        candidate.id = entry->source.id;
        candidate.kind = entry->source.kind;
        candidate.path = *located;
        candidate.cookKey = entry->cookKey;
        output = std::move(candidate);
        clearError(error);
        return true;
    }

    bool AssetManifestResourceLocator::resolve(const AssetId& id, AssetManifestResource& output,
                                               std::string* error) const
    {
        return resolve(id, AssetSourceKind::Unknown, output, error);
    }

    std::optional<AssetManifestResourceLocator::Path> AssetManifestResourceLocator::locate(
        const AssetId& id, const AssetSourceKind expectedKind) const
    {
        AssetManifestResource resource;
        return resolve(id, expectedKind, resource) ? std::optional<Path>(resource.path)
                                                   : std::nullopt;
    }

    bool AssetManifestResourceLocator::contains(const AssetId& id,
                                                const AssetSourceKind expectedKind) const
    {
        return locate(id, expectedKind).has_value();
    }

    void AssetManifestResourceLocator::clearError(std::string* error)
    {
        if (error != nullptr)
        {
            error->clear();
        }
    }

    bool AssetManifestResourceLocator::fail(std::string* error, const std::string& message)
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    }
}

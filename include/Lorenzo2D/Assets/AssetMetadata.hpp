#pragma once

#include <Lorenzo2D/Assets/AssetId.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace l2d
{
    enum class AssetSourceKind
    {
        Unknown,
        Texture,
        Font,
        Sound,
        Animation,
        Level,
        Prefab,
        Tilemap,
        Shader,
        Material
    };

    struct AssetSourceDescriptor
    {
        AssetId id;
        AssetSourceKind kind = AssetSourceKind::Unknown;
        std::filesystem::path sourcePath;
        std::string importer;
        std::map<std::string, std::string> importSettings;
        std::vector<AssetId> dependencies;
    };

    // Project-owned source metadata. Asset IDs are stable serialized identities;
    // source paths are normalized project-relative paths and therefore remain
    // independent from runtime AssetManager names/handles and machine-specific roots.
    //
    // The registry is deliberately CPU-only and deterministic. Phase 14 cooking,
    // cache, manifest, and editor layers can consume this contract without making
    // AssetManager or rendering depend on authoring state.
    class AssetMetadataRegistry
    {
      public:
        static constexpr std::size_t MaxAssetIdLength = 128;
        static constexpr std::size_t MaxImporterLength = 64;
        static constexpr std::size_t MaxSettingsPerAsset = 64;
        static constexpr std::size_t MaxSettingKeyLength = 64;
        static constexpr std::size_t MaxSettingValueLength = 512;
        static constexpr std::size_t MaxDependenciesPerAsset = 256;
        static constexpr std::size_t MaxAssets = 65536;

        static bool isValidAssetId(const AssetId& id) noexcept
        {
            if (id.empty() || id.size() > MaxAssetIdLength)
            {
                return false;
            }

            for (const unsigned char character : id)
            {
                const bool alphaNumeric =
                    (character >= static_cast<unsigned char>('a') &&
                     character <= static_cast<unsigned char>('z')) ||
                    (character >= static_cast<unsigned char>('A') &&
                     character <= static_cast<unsigned char>('Z')) ||
                    (character >= static_cast<unsigned char>('0') &&
                     character <= static_cast<unsigned char>('9'));
                const bool punctuation = character == static_cast<unsigned char>('_') ||
                                         character == static_cast<unsigned char>('-') ||
                                         character == static_cast<unsigned char>('.') ||
                                         character == static_cast<unsigned char>('/') ||
                                         character == static_cast<unsigned char>(':');
                if (!alphaNumeric && !punctuation)
                {
                    return false;
                }
            }

            return true;
        }

        static bool isValidSourcePath(const std::filesystem::path& path) noexcept
        {
            if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
            {
                return false;
            }

            for (const auto& component : path)
            {
                if (component == "..")
                {
                    return false;
                }
            }
            return true;
        }

        static bool validate(const AssetSourceDescriptor& descriptor,
                             std::string* error = nullptr)
        {
            const auto fail = [error](const char* message) {
                if (error != nullptr)
                {
                    *error = message;
                }
                return false;
            };

            if (!isValidAssetId(descriptor.id))
            {
                return fail("asset id is empty, too long, or contains unsupported characters");
            }
            if (descriptor.kind == AssetSourceKind::Unknown)
            {
                return fail("asset source kind must be explicit");
            }
            if (!isValidSourcePath(descriptor.sourcePath))
            {
                return fail("source path must be a non-empty project-relative path without '..'");
            }
            if (descriptor.importer.empty() || descriptor.importer.size() > MaxImporterLength)
            {
                return fail("importer is empty or exceeds the configured limit");
            }
            if (descriptor.importSettings.size() > MaxSettingsPerAsset)
            {
                return fail("too many import settings");
            }
            for (const auto& setting : descriptor.importSettings)
            {
                if (setting.first.empty() || setting.first.size() > MaxSettingKeyLength ||
                    setting.second.size() > MaxSettingValueLength)
                {
                    return fail("import setting exceeds the configured key/value limits");
                }
            }
            if (descriptor.dependencies.size() > MaxDependenciesPerAsset)
            {
                return fail("too many asset dependencies");
            }

            std::vector<AssetId> dependencies = descriptor.dependencies;
            std::sort(dependencies.begin(), dependencies.end());
            for (std::size_t index = 0; index < dependencies.size(); ++index)
            {
                if (!isValidAssetId(dependencies[index]))
                {
                    return fail("dependency contains an invalid asset id");
                }
                if (dependencies[index] == descriptor.id)
                {
                    return fail("asset metadata cannot depend on itself");
                }
                if (index > 0 && dependencies[index] == dependencies[index - 1])
                {
                    return fail("asset metadata contains a duplicate dependency");
                }
            }

            if (error != nullptr)
            {
                error->clear();
            }
            return true;
        }

        bool upsert(AssetSourceDescriptor descriptor, std::string* error = nullptr)
        {
            if (!validate(descriptor, error))
            {
                return false;
            }
            if (m_assets.find(descriptor.id) == m_assets.end() && m_assets.size() >= MaxAssets)
            {
                if (error != nullptr)
                {
                    *error = "asset metadata registry reached its configured limit";
                }
                return false;
            }

            descriptor.sourcePath = descriptor.sourcePath.lexically_normal();
            std::sort(descriptor.dependencies.begin(), descriptor.dependencies.end());
            m_assets.insert_or_assign(descriptor.id, std::move(descriptor));
            if (error != nullptr)
            {
                error->clear();
            }
            return true;
        }

        const AssetSourceDescriptor* find(const AssetId& id) const noexcept
        {
            const auto found = m_assets.find(id);
            return found == m_assets.end() ? nullptr : &found->second;
        }

        bool contains(const AssetId& id) const noexcept
        {
            return m_assets.find(id) != m_assets.end();
        }

        bool erase(const AssetId& id)
        {
            return m_assets.erase(id) != 0u;
        }

        void clear() noexcept
        {
            m_assets.clear();
        }

        std::size_t size() const noexcept
        {
            return m_assets.size();
        }

        std::vector<AssetSourceDescriptor> descriptors() const
        {
            std::vector<AssetSourceDescriptor> result;
            result.reserve(m_assets.size());
            for (const auto& asset : m_assets)
            {
                result.push_back(asset.second);
            }
            return result;
        }

      private:
        std::map<AssetId, AssetSourceDescriptor> m_assets;
    };
}

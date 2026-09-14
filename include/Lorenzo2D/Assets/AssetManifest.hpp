#pragma once

#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace l2d
{
    struct AssetCookRequest
    {
        AssetSourceDescriptor source;
        std::uint64_t sourceContentHash = 0;
        std::string importerVersion;
        std::uint64_t cookKey = 0;
    };

    struct AssetManifestEntry
    {
        AssetSourceDescriptor source;
        std::uint64_t sourceContentHash = 0;
        std::string importerVersion;
        std::uint64_t cookKey = 0;
        std::filesystem::path cookedPath;
    };

    class AssetManifest
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1;
        static constexpr std::size_t MaxManifestBytes = 16u * 1024u * 1024u;
        static constexpr std::size_t MaxImporterVersionLength = 64;
        static constexpr std::size_t MaxPortablePathLength = 1024;

        static bool makeCookRequest(const AssetSourceDescriptor& source,
                                    std::uint64_t sourceContentHash,
                                    std::string importerVersion,
                                    AssetCookRequest& output,
                                    std::string* error = nullptr);

        static bool validate(const AssetManifestEntry& entry, std::string* error = nullptr);

        bool upsert(AssetManifestEntry entry, std::string* error = nullptr);
        bool erase(const AssetId& id);
        void clear() noexcept;

        const AssetManifestEntry* find(const AssetId& id) const noexcept;
        bool contains(const AssetId& id) const noexcept;
        std::size_t size() const noexcept;
        std::vector<AssetManifestEntry> entries() const;

        std::string serialize() const;
        bool deserialize(std::string_view document, std::string* error = nullptr);

      private:
        std::map<AssetId, AssetManifestEntry> m_entries;
    };
}

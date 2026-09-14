#pragma once

#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

    // Deterministic, CPU-only persistence for authoring metadata and cooked-output identity.
    // This remains independent from runtime AssetManager ownership and live resource handles.
    class AssetManifest
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1;
        static constexpr std::size_t MaxManifestBytes = 16u * 1024u * 1024u;
        static constexpr std::size_t MaxImporterVersionLength = 64;
        static constexpr std::size_t MaxPortablePathLength = 1024;

        static bool makeCookRequest(const AssetSourceDescriptor& source,
                                    const std::uint64_t sourceContentHash,
                                    std::string importerVersion,
                                    AssetCookRequest& output,
                                    std::string* error = nullptr)
        {
            std::string validationError;
            if (!AssetMetadataRegistry::validate(source, &validationError))
            {
                return fail(error, "invalid cook source descriptor: " + validationError);
            }
            if (!validPortablePath(source.sourcePath))
            {
                return fail(error, "cook source path exceeds the portable path limit");
            }
            if (importerVersion.empty() || importerVersion.size() > MaxImporterVersionLength)
            {
                return fail(error, "importer version is empty or exceeds the configured limit");
            }

            AssetCookRequest candidate;
            candidate.source = source;
            canonicalize(candidate.source);
            candidate.sourceContentHash = sourceContentHash;
            candidate.importerVersion = std::move(importerVersion);
            candidate.cookKey = computeCookKey(candidate.source,
                                               candidate.sourceContentHash,
                                               candidate.importerVersion);
            output = std::move(candidate);
            clearError(error);
            return true;
        }

        static bool validate(const AssetManifestEntry& entry, std::string* error = nullptr)
        {
            AssetCookRequest request;
            std::string requestError;
            if (!makeCookRequest(entry.source,
                                 entry.sourceContentHash,
                                 entry.importerVersion,
                                 request,
                                 &requestError))
            {
                return fail(error, requestError);
            }
            if (entry.cookKey != request.cookKey)
            {
                return fail(error, "manifest cook key does not match canonical cook inputs");
            }
            if (!validPortablePath(entry.cookedPath))
            {
                return fail(error,
                            "cooked path must be a bounded project-relative path without '..'");
            }
            clearError(error);
            return true;
        }

        bool upsert(AssetManifestEntry entry, std::string* error = nullptr)
        {
            if (!validate(entry, error))
            {
                return false;
            }
            if (m_entries.find(entry.source.id) == m_entries.end() &&
                m_entries.size() >= AssetMetadataRegistry::MaxAssets)
            {
                return fail(error, "asset manifest reached its configured entry limit");
            }

            canonicalize(entry.source);
            entry.cookedPath = entry.cookedPath.lexically_normal();
            m_entries.insert_or_assign(entry.source.id, std::move(entry));
            clearError(error);
            return true;
        }

        bool erase(const AssetId& id)
        {
            return m_entries.erase(id) != 0u;
        }

        void clear() noexcept
        {
            m_entries.clear();
        }

        const AssetManifestEntry* find(const AssetId& id) const noexcept
        {
            const auto found = m_entries.find(id);
            return found == m_entries.end() ? nullptr : &found->second;
        }

        bool contains(const AssetId& id) const noexcept
        {
            return m_entries.find(id) != m_entries.end();
        }

        std::size_t size() const noexcept
        {
            return m_entries.size();
        }

        std::vector<AssetManifestEntry> entries() const
        {
            std::vector<AssetManifestEntry> result;
            result.reserve(m_entries.size());
            for (const auto& entry : m_entries)
            {
                result.push_back(entry.second);
            }
            return result;
        }

        std::string serialize() const
        {
            std::string document;
            appendField(document, "L2D-ASSET-MANIFEST");
            appendField(document, std::to_string(FormatVersion));
            appendField(document, std::to_string(m_entries.size()));

            for (const auto& pair : m_entries)
            {
                const auto& entry = pair.second;
                appendField(document, entry.source.id);
                appendField(document,
                            std::to_string(static_cast<std::uint32_t>(entry.source.kind)));
                appendField(document, entry.source.sourcePath.generic_string());
                appendField(document, entry.source.importer);

                appendField(document, std::to_string(entry.source.importSettings.size()));
                for (const auto& setting : entry.source.importSettings)
                {
                    appendField(document, setting.first);
                    appendField(document, setting.second);
                }

                appendField(document, std::to_string(entry.source.dependencies.size()));
                for (const auto& dependency : entry.source.dependencies)
                {
                    appendField(document, dependency);
                }

                appendField(document, std::to_string(entry.sourceContentHash));
                appendField(document, entry.importerVersion);
                appendField(document, std::to_string(entry.cookKey));
                appendField(document, entry.cookedPath.generic_string());
            }
            return document;
        }

        bool deserialize(const std::string_view document, std::string* error = nullptr)
        {
            if (document.size() > MaxManifestBytes)
            {
                return fail(error, "asset manifest exceeds the configured byte limit");
            }

            std::size_t offset = 0;
            std::string_view field;
            if (!readField(document, offset, field, error) || field != "L2D-ASSET-MANIFEST")
            {
                return fail(error, "asset manifest has an invalid signature");
            }

            std::uint32_t version = 0;
            if (!readField(document, offset, field, error) || !parseUnsigned(field, version) ||
                version != FormatVersion)
            {
                return fail(error, "asset manifest format version is unsupported");
            }

            std::size_t entryCount = 0;
            if (!readField(document, offset, field, error) || !parseUnsigned(field, entryCount) ||
                entryCount > AssetMetadataRegistry::MaxAssets)
            {
                return fail(error, "asset manifest contains an invalid entry count");
            }

            AssetManifest candidate;
            for (std::size_t entryIndex = 0; entryIndex < entryCount; ++entryIndex)
            {
                AssetManifestEntry entry;
                if (!readString(document, offset, entry.source.id, error))
                {
                    return false;
                }

                std::uint32_t kind = 0;
                if (!readField(document, offset, field, error) || !parseUnsigned(field, kind) ||
                    kind <= static_cast<std::uint32_t>(AssetSourceKind::Unknown) ||
                    kind > static_cast<std::uint32_t>(AssetSourceKind::Material))
                {
                    return fail(error, "asset manifest contains an invalid asset kind");
                }
                entry.source.kind = static_cast<AssetSourceKind>(kind);

                std::string path;
                if (!readString(document, offset, path, error))
                {
                    return false;
                }
                entry.source.sourcePath = path;
                if (!readString(document, offset, entry.source.importer, error))
                {
                    return false;
                }

                std::size_t settingCount = 0;
                if (!readField(document, offset, field, error) ||
                    !parseUnsigned(field, settingCount) ||
                    settingCount > AssetMetadataRegistry::MaxSettingsPerAsset)
                {
                    return fail(error, "asset manifest contains an invalid import-setting count");
                }
                for (std::size_t settingIndex = 0; settingIndex < settingCount; ++settingIndex)
                {
                    std::string key;
                    std::string value;
                    if (!readString(document, offset, key, error) ||
                        !readString(document, offset, value, error))
                    {
                        return false;
                    }
                    if (!entry.source.importSettings.emplace(std::move(key), std::move(value)).second)
                    {
                        return fail(error, "asset manifest contains duplicate import-setting keys");
                    }
                }

                std::size_t dependencyCount = 0;
                if (!readField(document, offset, field, error) ||
                    !parseUnsigned(field, dependencyCount) ||
                    dependencyCount > AssetMetadataRegistry::MaxDependenciesPerAsset)
                {
                    return fail(error, "asset manifest contains an invalid dependency count");
                }
                entry.source.dependencies.reserve(dependencyCount);
                for (std::size_t dependencyIndex = 0; dependencyIndex < dependencyCount;
                     ++dependencyIndex)
                {
                    std::string dependency;
                    if (!readString(document, offset, dependency, error))
                    {
                        return false;
                    }
                    entry.source.dependencies.push_back(std::move(dependency));
                }

                if (!readField(document, offset, field, error) ||
                    !parseUnsigned(field, entry.sourceContentHash) ||
                    !readString(document, offset, entry.importerVersion, error) ||
                    !readField(document, offset, field, error) ||
                    !parseUnsigned(field, entry.cookKey))
                {
                    return fail(error, "asset manifest contains invalid cook identity fields");
                }

                std::string cookedPath;
                if (!readString(document, offset, cookedPath, error))
                {
                    return false;
                }
                entry.cookedPath = cookedPath;

                if (candidate.contains(entry.source.id))
                {
                    return fail(error, "asset manifest contains duplicate asset ids");
                }
                if (!candidate.upsert(std::move(entry), error))
                {
                    return false;
                }
            }

            if (offset != document.size())
            {
                return fail(error, "asset manifest contains trailing data");
            }

            m_entries = std::move(candidate.m_entries);
            clearError(error);
            return true;
        }

      private:
        static constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        static constexpr std::uint64_t FnvPrime = 1099511628211ull;

        static void clearError(std::string* error)
        {
            if (error != nullptr)
            {
                error->clear();
            }
        }

        static bool fail(std::string* error, const std::string& message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
            return false;
        }

        static bool validPortablePath(const std::filesystem::path& path)
        {
            return AssetMetadataRegistry::isValidSourcePath(path) &&
                   path.generic_string().size() <= MaxPortablePathLength;
        }

        static void canonicalize(AssetSourceDescriptor& source)
        {
            source.sourcePath = source.sourcePath.lexically_normal();
            std::sort(source.dependencies.begin(), source.dependencies.end());
        }

        static void hashByte(std::uint64_t& hash, const unsigned char value) noexcept
        {
            hash ^= static_cast<std::uint64_t>(value);
            hash *= FnvPrime;
        }

        static void hashUint64(std::uint64_t& hash, const std::uint64_t value) noexcept
        {
            for (unsigned int shift = 0; shift < 64u; shift += 8u)
            {
                hashByte(hash,
                         static_cast<unsigned char>((value >> shift) &
                                                    static_cast<std::uint64_t>(0xffu)));
            }
        }

        static void hashString(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hashUint64(hash, static_cast<std::uint64_t>(value.size()));
            for (const char character : value)
            {
                hashByte(hash, static_cast<unsigned char>(character));
            }
        }

        static std::uint64_t computeCookKey(const AssetSourceDescriptor& source,
                                            const std::uint64_t sourceContentHash,
                                            const std::string_view importerVersion)
        {
            std::uint64_t hash = FnvOffset;
            hashString(hash, source.id);
            hashUint64(hash, static_cast<std::uint64_t>(source.kind));
            hashString(hash, source.sourcePath.lexically_normal().generic_string());
            hashString(hash, source.importer);
            hashString(hash, importerVersion);
            hashUint64(hash, sourceContentHash);

            hashUint64(hash, static_cast<std::uint64_t>(source.importSettings.size()));
            for (const auto& setting : source.importSettings)
            {
                hashString(hash, setting.first);
                hashString(hash, setting.second);
            }

            std::vector<AssetId> dependencies = source.dependencies;
            std::sort(dependencies.begin(), dependencies.end());
            hashUint64(hash, static_cast<std::uint64_t>(dependencies.size()));
            for (const auto& dependency : dependencies)
            {
                hashString(hash, dependency);
            }
            return hash;
        }

        static void appendField(std::string& output, const std::string_view value)
        {
            output += std::to_string(value.size());
            output.push_back(':');
            output.append(value.data(), value.size());
        }

        static bool readField(const std::string_view document,
                              std::size_t& offset,
                              std::string_view& output,
                              std::string* error)
        {
            if (offset >= document.size())
            {
                return fail(error, "asset manifest ended before the next field");
            }

            std::size_t length = 0;
            bool hasDigit = false;
            while (offset < document.size() && document[offset] != ':')
            {
                const char character = document[offset];
                if (character < '0' || character > '9')
                {
                    return fail(error, "asset manifest field length is malformed");
                }
                hasDigit = true;
                const std::size_t digit = static_cast<std::size_t>(character - '0');
                if (length > (std::numeric_limits<std::size_t>::max() - digit) / 10u)
                {
                    return fail(error, "asset manifest field length overflowed");
                }
                length = length * 10u + digit;
                ++offset;
            }

            if (!hasDigit || offset >= document.size() || document[offset] != ':')
            {
                return fail(error, "asset manifest field delimiter is missing");
            }
            ++offset;
            if (length > document.size() - offset)
            {
                return fail(error, "asset manifest field exceeds remaining input");
            }

            output = document.substr(offset, length);
            offset += length;
            return true;
        }

        static bool readString(const std::string_view document,
                               std::size_t& offset,
                               std::string& output,
                               std::string* error)
        {
            std::string_view field;
            if (!readField(document, offset, field, error))
            {
                return false;
            }
            output.assign(field.data(), field.size());
            return true;
        }

        template <typename Unsigned>
        static bool parseUnsigned(const std::string_view value, Unsigned& output)
        {
            if (value.empty())
            {
                return false;
            }

            Unsigned candidate = 0;
            const char* const begin = value.data();
            const char* const end = begin + value.size();
            const auto result = std::from_chars(begin, end, candidate);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return false;
            }
            output = candidate;
            return true;
        }

        std::map<AssetId, AssetManifestEntry> m_entries;
    };
}

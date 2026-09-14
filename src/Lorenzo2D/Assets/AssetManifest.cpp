#include <Lorenzo2D/Assets/AssetManifest.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        using Json = nlohmann::json;

        constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        void hashByte(std::uint64_t& hash, const unsigned char value) noexcept
        {
            hash ^= static_cast<std::uint64_t>(value);
            hash *= FnvPrime;
        }

        void hashUint64(std::uint64_t& hash, const std::uint64_t value) noexcept
        {
            for (unsigned int shift = 0; shift < 64u; shift += 8u)
            {
                hashByte(hash, static_cast<unsigned char>((value >> shift) & 0xffu));
            }
        }

        void hashString(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hashUint64(hash, static_cast<std::uint64_t>(value.size()));
            for (const char character : value)
            {
                hashByte(hash, static_cast<unsigned char>(character));
            }
        }

        std::uint64_t computeCookKey(const AssetSourceDescriptor& source,
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

        bool validPortablePath(const std::filesystem::path& path)
        {
            return AssetMetadataRegistry::isValidSourcePath(path) &&
                   path.generic_string().size() <= AssetManifest::MaxPortablePathLength;
        }

        bool fail(std::string* error, const std::string& message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
            return false;
        }

        Json sourceToJson(const AssetSourceDescriptor& source)
        {
            Json settings = Json::object();
            for (const auto& setting : source.importSettings)
            {
                settings[setting.first] = setting.second;
            }

            return Json{{"id", source.id},
                        {"kind", static_cast<int>(source.kind)},
                        {"source", source.sourcePath.generic_string()},
                        {"importer", source.importer},
                        {"settings", std::move(settings)},
                        {"dependencies", source.dependencies}};
        }

        bool sourceFromJson(const Json& value, AssetSourceDescriptor& output, std::string* error)
        {
            if (!value.is_object() || !value.contains("id") || !value.at("id").is_string() ||
                !value.contains("kind") || !value.at("kind").is_number_integer() ||
                !value.contains("source") || !value.at("source").is_string() ||
                !value.contains("importer") || !value.at("importer").is_string() ||
                !value.contains("settings") || !value.at("settings").is_object() ||
                !value.contains("dependencies") || !value.at("dependencies").is_array())
            {
                return fail(error, "manifest source descriptor has an invalid shape");
            }

            const auto kindValue = value.at("kind").get<long long>();
            if (kindValue <= static_cast<long long>(AssetSourceKind::Unknown) ||
                kindValue > static_cast<long long>(AssetSourceKind::Material))
            {
                return fail(error, "manifest source descriptor has an invalid asset kind");
            }

            AssetSourceDescriptor candidate;
            candidate.id = value.at("id").get<std::string>();
            candidate.kind = static_cast<AssetSourceKind>(kindValue);
            candidate.sourcePath = value.at("source").get<std::string>();
            candidate.importer = value.at("importer").get<std::string>();

            const auto& settings = value.at("settings");
            if (settings.size() > AssetMetadataRegistry::MaxSettingsPerAsset)
            {
                return fail(error, "manifest source descriptor has too many import settings");
            }
            for (auto iterator = settings.begin(); iterator != settings.end(); ++iterator)
            {
                if (!iterator.value().is_string())
                {
                    return fail(error, "manifest import setting value must be a string");
                }
                candidate.importSettings.emplace(iterator.key(), iterator.value().get<std::string>());
            }

            const auto& dependencies = value.at("dependencies");
            if (dependencies.size() > AssetMetadataRegistry::MaxDependenciesPerAsset)
            {
                return fail(error, "manifest source descriptor has too many dependencies");
            }
            candidate.dependencies.reserve(dependencies.size());
            for (const auto& dependency : dependencies)
            {
                if (!dependency.is_string())
                {
                    return fail(error, "manifest dependency must be a string");
                }
                candidate.dependencies.push_back(dependency.get<std::string>());
            }

            std::string validationError;
            if (!AssetMetadataRegistry::validate(candidate, &validationError))
            {
                return fail(error, "invalid manifest source descriptor: " + validationError);
            }
            if (!validPortablePath(candidate.sourcePath))
            {
                return fail(error, "manifest source path exceeds the portable path limit");
            }

            candidate.sourcePath = candidate.sourcePath.lexically_normal();
            std::sort(candidate.dependencies.begin(), candidate.dependencies.end());
            output = std::move(candidate);
            return true;
        }
    }

    bool AssetManifest::makeCookRequest(const AssetSourceDescriptor& source,
                                        const std::uint64_t sourceContentHash,
                                        std::string importerVersion,
                                        AssetCookRequest& output,
                                        std::string* error)
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
        candidate.source.sourcePath = candidate.source.sourcePath.lexically_normal();
        std::sort(candidate.source.dependencies.begin(), candidate.source.dependencies.end());
        candidate.sourceContentHash = sourceContentHash;
        candidate.importerVersion = std::move(importerVersion);
        candidate.cookKey = computeCookKey(candidate.source,
                                           candidate.sourceContentHash,
                                           candidate.importerVersion);
        output = std::move(candidate);
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }

    bool AssetManifest::validate(const AssetManifestEntry& entry, std::string* error)
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
            return fail(error, "cooked path must be a bounded project-relative path without '..'");
        }
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }

    bool AssetManifest::upsert(AssetManifestEntry entry, std::string* error)
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

        entry.source.sourcePath = entry.source.sourcePath.lexically_normal();
        entry.cookedPath = entry.cookedPath.lexically_normal();
        std::sort(entry.source.dependencies.begin(), entry.source.dependencies.end());
        m_entries.insert_or_assign(entry.source.id, std::move(entry));
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }

    bool AssetManifest::erase(const AssetId& id)
    {
        return m_entries.erase(id) != 0u;
    }

    void AssetManifest::clear() noexcept
    {
        m_entries.clear();
    }

    const AssetManifestEntry* AssetManifest::find(const AssetId& id) const noexcept
    {
        const auto found = m_entries.find(id);
        return found == m_entries.end() ? nullptr : &found->second;
    }

    bool AssetManifest::contains(const AssetId& id) const noexcept
    {
        return m_entries.find(id) != m_entries.end();
    }

    std::size_t AssetManifest::size() const noexcept
    {
        return m_entries.size();
    }

    std::vector<AssetManifestEntry> AssetManifest::entries() const
    {
        std::vector<AssetManifestEntry> result;
        result.reserve(m_entries.size());
        for (const auto& entry : m_entries)
        {
            result.push_back(entry.second);
        }
        return result;
    }

    std::string AssetManifest::serialize() const
    {
        Json serializedEntries = Json::array();
        for (const auto& pair : m_entries)
        {
            const auto& entry = pair.second;
            serializedEntries.push_back(Json{{"source", sourceToJson(entry.source)},
                                             {"source_hash", entry.sourceContentHash},
                                             {"importer_version", entry.importerVersion},
                                             {"cook_key", entry.cookKey},
                                             {"cooked_path", entry.cookedPath.generic_string()}});
        }
        return Json{{"format", FormatVersion}, {"assets", std::move(serializedEntries)}}.dump();
    }

    bool AssetManifest::deserialize(const std::string_view document, std::string* error)
    {
        if (document.size() > MaxManifestBytes)
        {
            return fail(error, "asset manifest exceeds the configured byte limit");
        }

        const Json root = Json::parse(document.begin(), document.end(), nullptr, false);
        if (root.is_discarded() || !root.is_object() || !root.contains("format") ||
            !root.at("format").is_number_unsigned() || !root.contains("assets") ||
            !root.at("assets").is_array())
        {
            return fail(error, "asset manifest is malformed or has an invalid root shape");
        }
        if (root.at("format").get<std::uint32_t>() != FormatVersion)
        {
            return fail(error, "asset manifest format version is unsupported");
        }

        const auto& assets = root.at("assets");
        if (assets.size() > AssetMetadataRegistry::MaxAssets)
        {
            return fail(error, "asset manifest contains too many entries");
        }

        AssetManifest candidate;
        for (const auto& value : assets)
        {
            if (!value.is_object() || !value.contains("source") ||
                !value.contains("source_hash") || !value.at("source_hash").is_number_unsigned() ||
                !value.contains("importer_version") ||
                !value.at("importer_version").is_string() || !value.contains("cook_key") ||
                !value.at("cook_key").is_number_unsigned() || !value.contains("cooked_path") ||
                !value.at("cooked_path").is_string())
            {
                return fail(error, "asset manifest entry has an invalid shape");
            }

            AssetManifestEntry entry;
            if (!sourceFromJson(value.at("source"), entry.source, error))
            {
                return false;
            }
            entry.sourceContentHash = value.at("source_hash").get<std::uint64_t>();
            entry.importerVersion = value.at("importer_version").get<std::string>();
            entry.cookKey = value.at("cook_key").get<std::uint64_t>();
            entry.cookedPath = value.at("cooked_path").get<std::string>();

            if (candidate.contains(entry.source.id))
            {
                return fail(error, "asset manifest contains duplicate asset ids");
            }
            if (!candidate.upsert(std::move(entry), error))
            {
                return false;
            }
        }

        m_entries = std::move(candidate.m_entries);
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}

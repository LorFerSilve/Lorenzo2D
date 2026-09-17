#pragma once

#include <Lorenzo2D/Assets/AssetBuildGraph.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace l2d
{
    struct AssetCookCacheEntry
    {
        AssetId id;
        std::uint64_t buildKey = 0;
    };

    // Durable successful-cook state for Phase 14 authoring/build workflows.
    // Build keys are derived from authoritative AssetManifest data by
    // AssetCookExecutor; they are not a second asset identity system.
    class AssetCookCache
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1;
        // Every cache accepted by record() fits inside this persistence envelope:
        // MaxAssets * (max AssetId + decimal build key + framing) is below 16 MiB.
        static constexpr std::size_t MaxCacheBytes = 16u * 1024u * 1024u;

        bool record(const AssetId& id, const std::uint64_t buildKey, std::string* error = nullptr)
        {
            if (!AssetMetadataRegistry::isValidAssetId(id))
            {
                return fail(error, "asset cook cache id is invalid");
            }
            if (m_entries.find(id) == m_entries.end() &&
                m_entries.size() >= AssetMetadataRegistry::MaxAssets)
            {
                return fail(error, "asset cook cache reached its configured entry limit");
            }

            m_entries.insert_or_assign(id, buildKey);
            clearError(error);
            return true;
        }

        bool erase(const AssetId& id)
        {
            return m_entries.erase(id) != 0u;
        }

        void pruneToManifest(const AssetManifest& manifest)
        {
            for (auto entry = m_entries.begin(); entry != m_entries.end();)
            {
                if (!manifest.contains(entry->first))
                {
                    entry = m_entries.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }

        void clear() noexcept
        {
            m_entries.clear();
        }

        bool contains(const AssetId& id, const std::uint64_t buildKey) const noexcept
        {
            const auto found = m_entries.find(id);
            return found != m_entries.end() && found->second == buildKey;
        }

        std::size_t size() const noexcept
        {
            return m_entries.size();
        }

        std::vector<AssetCookCacheEntry> entries() const
        {
            std::vector<AssetCookCacheEntry> result;
            result.reserve(m_entries.size());
            for (const auto& pair : m_entries)
            {
                result.push_back({pair.first, pair.second});
            }
            return result;
        }

        std::string serialize() const
        {
            std::string document = "L2D-ASSET-COOK-CACHE\n" + std::to_string(FormatVersion) + "\n" +
                                   std::to_string(m_entries.size()) + "\n";
            for (const auto& pair : m_entries)
            {
                document += std::to_string(pair.first.size()) + ":" + pair.first + "\n" +
                            std::to_string(pair.second) + "\n";
            }
            return document;
        }

        bool deserialize(const std::string_view document, std::string* error = nullptr)
        {
            if (document.size() > MaxCacheBytes)
            {
                return fail(error, "asset cook cache exceeds the configured byte limit");
            }

            Reader reader(document);
            std::string_view field;
            if (!reader.line(field) || field != "L2D-ASSET-COOK-CACHE")
            {
                return fail(error, "asset cook cache header is invalid");
            }

            std::uint64_t version = 0;
            if (!reader.unsignedLine(version) || version != FormatVersion)
            {
                return fail(error, "asset cook cache version is unsupported");
            }

            std::uint64_t count = 0;
            if (!reader.unsignedLine(count) || count > AssetMetadataRegistry::MaxAssets)
            {
                return fail(error, "asset cook cache entry count is invalid");
            }

            AssetCookCache candidate;
            for (std::uint64_t index = 0; index < count; ++index)
            {
                std::string id;
                std::uint64_t buildKey = 0;
                if (!reader.sizedString(id) || !AssetMetadataRegistry::isValidAssetId(id) ||
                    !reader.unsignedLine(buildKey))
                {
                    return fail(error, "asset cook cache entry is malformed");
                }
                if (candidate.m_entries.find(id) != candidate.m_entries.end())
                {
                    return fail(error, "asset cook cache contains a duplicate asset id");
                }
                candidate.m_entries.emplace(std::move(id), buildKey);
            }

            if (!reader.finished())
            {
                return fail(error, "asset cook cache contains trailing data");
            }

            *this = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
        class Reader
        {
          public:
            explicit Reader(const std::string_view input) : m_input(input) {}

            bool line(std::string_view& output)
            {
                const auto end = m_input.find('\n', m_offset);
                if (end == std::string_view::npos)
                {
                    return false;
                }
                output = m_input.substr(m_offset, end - m_offset);
                m_offset = end + 1;
                return true;
            }

            bool unsignedLine(std::uint64_t& output)
            {
                std::string_view value;
                if (!line(value) || value.empty())
                {
                    return false;
                }
                const auto result =
                    std::from_chars(value.data(), value.data() + value.size(), output);
                return result.ec == std::errc{} && result.ptr == value.data() + value.size();
            }

            bool sizedString(std::string& output)
            {
                const auto colon = m_input.find(':', m_offset);
                if (colon == std::string_view::npos)
                {
                    return false;
                }
                std::size_t size = 0;
                const auto sizeText = m_input.substr(m_offset, colon - m_offset);
                const auto parsed =
                    std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size);
                if (parsed.ec != std::errc{} || parsed.ptr != sizeText.data() + sizeText.size())
                {
                    return false;
                }
                const auto begin = colon + 1;
                if (size > m_input.size() - begin || begin + size >= m_input.size() ||
                    m_input[begin + size] != '\n')
                {
                    return false;
                }
                output.assign(m_input.substr(begin, size));
                m_offset = begin + size + 1;
                return true;
            }

            bool finished() const noexcept
            {
                return m_offset == m_input.size();
            }

          private:
            std::string_view m_input;
            std::size_t m_offset = 0;
        };

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

        std::map<AssetId, std::uint64_t> m_entries;
    };

    struct AssetCookExecutionResult
    {
        std::vector<AssetId> cooked;
        std::vector<AssetId> remaining;
    };

    class AssetCookExecutor
    {
      public:
        using CookFunction = std::function<bool(const AssetCookRequest&,
                                                const std::filesystem::path&, std::string*)>;

        static bool execute(const AssetManifest& previous, const AssetManifest& current,
                            AssetCookCache& cache, const std::size_t maxJobs,
                            const CookFunction& cooker, AssetCookExecutionResult& output,
                            std::string* error = nullptr)
        {
            if (!cooker)
            {
                return fail(error, "asset cooker callback is not configured");
            }

            std::vector<AssetId> rebuildOrder;
            std::string graphError;
            if (!AssetBuildGraph::computeRebuildOrder(previous, current, rebuildOrder, &graphError))
            {
                return fail(error, graphError);
            }

            std::map<AssetId, std::uint64_t> buildKeys;
            if (!computeBuildKeys(current, buildKeys, &graphError))
            {
                return fail(error, graphError);
            }

            // Removed assets have no current cook target and must not accumulate forever.
            cache.pruneToManifest(current);

            AssetCookExecutionResult candidate;
            for (const auto& id : rebuildOrder)
            {
                const auto* entry = current.find(id);
                const auto buildKey = buildKeys.find(id);
                if (entry == nullptr || buildKey == buildKeys.end())
                {
                    return fail(error,
                                "rebuild plan references an asset missing from the manifest");
                }
                if (cache.contains(id, buildKey->second))
                {
                    continue;
                }
                if (candidate.cooked.size() >= maxJobs)
                {
                    candidate.remaining.push_back(id);
                    continue;
                }

                AssetCookRequest request;
                if (!AssetManifest::makeCookRequest(entry->source, entry->sourceContentHash,
                                                    entry->importerVersion, request, error))
                {
                    return false;
                }

                std::string cookError;
                if (!cooker(request, entry->cookedPath, &cookError))
                {
                    return fail(error, "asset '" + id + "' cook failed: " + cookError);
                }
                if (!cache.record(id, buildKey->second, error))
                {
                    return false;
                }
                candidate.cooked.push_back(id);
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
        static constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        static constexpr std::uint64_t FnvPrime = 1099511628211ull;

        static void hashByte(std::uint64_t& hash, const unsigned char value) noexcept
        {
            hash ^= static_cast<std::uint64_t>(value);
            hash *= FnvPrime;
        }

        static void hashUint64(std::uint64_t& hash, const std::uint64_t value) noexcept
        {
            for (unsigned int shift = 0; shift < 64u; shift += 8u)
            {
                hashByte(hash, static_cast<unsigned char>((value >> shift) &
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

        static bool computeBuildKeys(const AssetManifest& manifest,
                                     std::map<AssetId, std::uint64_t>& output,
                                     std::string* error = nullptr)
        {
            AssetBuildGraph graph;
            std::string graphError;
            if (!graph.build(manifest, &graphError))
            {
                return fail(error, graphError);
            }

            std::map<AssetId, std::uint64_t> candidate;
            for (const auto& id : graph.topologicalOrder())
            {
                const auto* entry = manifest.find(id);
                if (entry == nullptr)
                {
                    return fail(error, "asset build graph references a missing manifest entry");
                }

                std::uint64_t hash = FnvOffset;
                hashString(hash, "L2D-ASSET-BUILD-KEY-V1");
                hashString(hash, id);
                hashUint64(hash, entry->cookKey);
                hashString(hash, entry->cookedPath.lexically_normal().generic_string());

                std::vector<AssetId> dependencies = entry->source.dependencies;
                std::sort(dependencies.begin(), dependencies.end());
                hashUint64(hash, static_cast<std::uint64_t>(dependencies.size()));
                for (const auto& dependency : dependencies)
                {
                    const auto dependencyKey = candidate.find(dependency);
                    if (dependencyKey == candidate.end())
                    {
                        return fail(error, "asset build key references an unresolved dependency '" +
                                               dependency + "'");
                    }
                    hashString(hash, dependency);
                    hashUint64(hash, dependencyKey->second);
                }
                candidate.emplace(id, hash);
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

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
    };
}

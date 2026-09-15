#pragma once

#include <Lorenzo2D/Assets/AssetBuildGraph.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
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
        std::uint64_t cookKey = 0;
    };

    class AssetCookCache
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1;
        static constexpr std::size_t MaxCacheBytes = 4u * 1024u * 1024u;

        void record(const AssetId& id, const std::uint64_t cookKey)
        {
            m_entries.insert_or_assign(id, cookKey);
        }

        bool erase(const AssetId& id)
        {
            return m_entries.erase(id) != 0u;
        }

        void clear() noexcept
        {
            m_entries.clear();
        }

        bool contains(const AssetId& id, const std::uint64_t cookKey) const noexcept
        {
            const auto found = m_entries.find(id);
            return found != m_entries.end() && found->second == cookKey;
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
                std::uint64_t cookKey = 0;
                if (!reader.sizedString(id) || id.empty() || !reader.unsignedLine(cookKey))
                {
                    return fail(error, "asset cook cache entry is malformed");
                }
                if (candidate.m_entries.find(id) != candidate.m_entries.end())
                {
                    return fail(error, "asset cook cache contains a duplicate asset id");
                }
                candidate.m_entries.emplace(std::move(id), cookKey);
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
        using CookFunction =
            std::function<bool(const AssetCookRequest&, const std::filesystem::path&, std::string*)>;

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

            AssetCookExecutionResult candidate;
            for (const auto& id : rebuildOrder)
            {
                const auto* entry = current.find(id);
                if (entry == nullptr)
                {
                    return fail(error, "rebuild plan references an asset missing from the manifest");
                }
                if (cache.contains(id, entry->cookKey))
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
                cache.record(id, entry->cookKey);
                candidate.cooked.push_back(id);
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
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

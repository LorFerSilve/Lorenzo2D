#pragma once

#include <Lorenzo2D/Assets/AssetBuildGraph.hpp>
#include <Lorenzo2D/Assets/AssetManifestResourceLocator.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace l2d
{
    struct AssetContentGeneration
    {
        std::string id;
        std::filesystem::path root;
        AssetManifest manifest;

        // Replaces a runtime locator only after the complete generation root has been accepted.
        bool configure(AssetManifestResourceLocator& output,
                       std::string* error = nullptr) const
        {
            std::error_code filesystemError;
            if (!std::filesystem::is_directory(root, filesystemError) || filesystemError)
            {
                if (error != nullptr)
                {
                    *error = "published asset generation root is unavailable";
                }
                return false;
            }

            AssetManifestResourceLocator candidate(manifest);
            if (!candidate.addRoot(root, error))
            {
                return false;
            }

            output = std::move(candidate);
            if (error != nullptr)
            {
                error->clear();
            }
            return true;
        }
    };

    struct AssetGenerationStaging
    {
        std::string id;
        std::filesystem::path root;
        AssetManifest manifest;
    };

    // Transactional Phase 14 content-generation publication boundary.
    //
    // A rebuild is prepared in a private sibling staging directory. When a previous generation is
    // supplied, its immutable tree is copied forward and every output invalidated by the current
    // dependency graph is removed before cooking resumes. publish() validates that all manifest
    // artifacts exist inside the staging root and then exposes the whole directory with one rename.
    // Existing published generations are never modified in place.
    class AssetGenerationPublisher
    {
      public:
        using Path = std::filesystem::path;

        static constexpr std::string_view ManifestFilename = ".l2d-generation-manifest";

        explicit AssetGenerationPublisher(Path publicationRoot)
            : m_publicationRoot(std::move(publicationRoot))
        {
        }

        const Path& publicationRoot() const noexcept
        {
            return m_publicationRoot;
        }

        static std::string generationId(const AssetManifest& manifest)
        {
            std::uint64_t hash = FnvOffset;
            for (const char character : manifest.serialize())
            {
                hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
                hash *= FnvPrime;
            }

            static constexpr char HexDigits[] = "0123456789abcdef";
            std::string result = "generation-";
            result.reserve(27u);
            for (int shift = 60; shift >= 0; shift -= 4)
            {
                const auto digit =
                    static_cast<std::size_t>((hash >> static_cast<unsigned int>(shift)) & 0x0full);
                result.push_back(HexDigits[digit]);
            }
            return result;
        }

        bool prepare(const AssetManifest& current, AssetGenerationStaging& output,
                     std::string* error = nullptr) const
        {
            return prepare(current, nullptr, output, error);
        }

        bool prepare(const AssetManifest& current, const AssetContentGeneration* previous,
                     AssetGenerationStaging& output, std::string* error = nullptr) const
        {
            if (!validateManifest(current, error))
            {
                return false;
            }

            Path root;
            if (!resolvePublicationRoot(root, true, error))
            {
                return false;
            }

            AssetGenerationStaging candidate;
            candidate.id = generationId(current);
            candidate.root = root / (".l2d-stage-" + candidate.id);
            candidate.manifest = current;

            std::error_code filesystemError;
            if (std::filesystem::exists(root / candidate.id, filesystemError) || filesystemError)
            {
                return fail(error, filesystemError ? "asset generation destination check failed"
                                                   : "asset generation is already published");
            }
            if (std::filesystem::exists(candidate.root, filesystemError) || filesystemError)
            {
                return fail(error, filesystemError ? "asset generation staging check failed"
                                                   : "asset generation staging root already exists");
            }
            if (!std::filesystem::create_directory(candidate.root, filesystemError) ||
                filesystemError)
            {
                return fail(error, "asset generation staging root could not be created");
            }

            const auto cleanup = [&candidate]()
            {
                std::error_code ignored;
                std::filesystem::remove_all(candidate.root, ignored);
            };

            if (previous != nullptr)
            {
                if (!validatePrevious(*previous, root, error) ||
                    !copyTree(previous->root, candidate.root, error))
                {
                    cleanup();
                    return false;
                }

                std::vector<AssetId> rebuildOrder;
                if (!AssetBuildGraph::computeRebuildOrder(previous->manifest, current, rebuildOrder,
                                                          error))
                {
                    cleanup();
                    return false;
                }

                for (const auto& id : rebuildOrder)
                {
                    const auto* entry = current.find(id);
                    if (entry == nullptr)
                    {
                        cleanup();
                        return fail(error,
                                    "asset generation rebuild plan references a missing asset");
                    }

                    filesystemError.clear();
                    std::filesystem::remove(candidate.root / entry->cookedPath, filesystemError);
                    if (filesystemError)
                    {
                        cleanup();
                        return fail(error,
                                    "asset generation could not invalidate a staged artifact");
                    }
                }
            }

            if (!writeManifest(candidate.root, candidate.manifest, error))
            {
                cleanup();
                return false;
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

        bool publish(const AssetGenerationStaging& staging, AssetContentGeneration& output,
                     std::string* error = nullptr) const
        {
            if (!validateManifest(staging.manifest, error))
            {
                return false;
            }
            if (staging.id != generationId(staging.manifest))
            {
                return fail(error, "asset generation staging id does not match its manifest");
            }

            Path root;
            if (!resolvePublicationRoot(root, false, error))
            {
                return false;
            }

            std::error_code filesystemError;
            const Path actualStaging = std::filesystem::canonical(staging.root, filesystemError);
            if (filesystemError)
            {
                return fail(error, "asset generation staging root is unavailable");
            }

            filesystemError.clear();
            const Path expectedStaging = std::filesystem::canonical(
                root / (".l2d-stage-" + staging.id), filesystemError);
            if (filesystemError || actualStaging != expectedStaging)
            {
                return fail(error, "asset generation staging root is not owned by this publisher");
            }

            if (!validateArtifacts(actualStaging, staging.manifest, error) ||
                !writeManifest(actualStaging, staging.manifest, error))
            {
                return false;
            }

            const Path destination = root / staging.id;
            filesystemError.clear();
            if (std::filesystem::exists(destination, filesystemError) || filesystemError)
            {
                return fail(error, filesystemError ? "asset generation destination check failed"
                                                   : "asset generation is already published");
            }

            AssetContentGeneration candidate{staging.id, destination, staging.manifest};
            std::filesystem::rename(actualStaging, destination, filesystemError);
            if (filesystemError)
            {
                return fail(error, "asset generation could not be published atomically");
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
        static constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        static constexpr std::uint64_t FnvPrime = 1099511628211ull;

        static bool validateManifest(const AssetManifest& manifest, std::string* error)
        {
            std::string graphError;
            if (!AssetBuildGraph::validate(manifest, &graphError))
            {
                return fail(error, "asset generation manifest graph is invalid: " + graphError);
            }

            for (const auto& entry : manifest.entries())
            {
                std::string entryError;
                if (!AssetManifest::validate(entry, &entryError))
                {
                    return fail(error,
                                "asset generation manifest entry is invalid: " + entryError);
                }

                const auto path = entry.cookedPath.lexically_normal();
                const auto first = path.begin();
                if (first != path.end() &&
                    *first == Path(std::string(ManifestFilename)))
                {
                    return fail(error,
                                "asset generation cooked path uses reserved publication metadata");
                }
            }

            clearError(error);
            return true;
        }

        bool resolvePublicationRoot(Path& output, const bool create, std::string* error) const
        {
            if (m_publicationRoot.empty())
            {
                return fail(error, "asset generation publication root is empty");
            }

            std::error_code filesystemError;
            Path absoluteRoot = std::filesystem::absolute(m_publicationRoot, filesystemError);
            if (filesystemError)
            {
                return fail(error, "asset generation publication root could not be resolved");
            }

            if (create)
            {
                std::filesystem::create_directories(absoluteRoot, filesystemError);
                if (filesystemError)
                {
                    return fail(error, "asset generation publication root could not be created");
                }
            }

            Path canonicalRoot = std::filesystem::canonical(absoluteRoot, filesystemError);
            if (filesystemError ||
                !std::filesystem::is_directory(canonicalRoot, filesystemError) || filesystemError)
            {
                return fail(error,
                            "asset generation publication root does not exist as a directory");
            }

            output = std::move(canonicalRoot);
            clearError(error);
            return true;
        }

        static bool validatePrevious(const AssetContentGeneration& previous,
                                     const Path& publicationRoot, std::string* error)
        {
            if (previous.id != generationId(previous.manifest))
            {
                return fail(error, "previous asset generation identity is invalid");
            }

            std::error_code filesystemError;
            const Path canonicalRoot = std::filesystem::canonical(previous.root, filesystemError);
            if (filesystemError || canonicalRoot != publicationRoot / previous.id ||
                !std::filesystem::is_directory(canonicalRoot, filesystemError) || filesystemError)
            {
                return fail(error, "previous asset generation is not owned by this publisher");
            }

            return validateArtifacts(canonicalRoot, previous.manifest, error);
        }

        static bool validateArtifacts(const Path& root, const AssetManifest& manifest,
                                      std::string* error)
        {
            std::error_code filesystemError;
            const Path canonicalRoot = std::filesystem::canonical(root, filesystemError);
            if (filesystemError)
            {
                return fail(error, "asset generation root could not be resolved");
            }

            for (const auto& entry : manifest.entries())
            {
                filesystemError.clear();
                const Path artifact =
                    std::filesystem::canonical(canonicalRoot / entry.cookedPath, filesystemError);
                if (filesystemError || !isWithin(canonicalRoot, artifact) ||
                    !std::filesystem::is_regular_file(artifact, filesystemError) ||
                    filesystemError)
                {
                    return fail(error, "asset generation is missing required artifact '" +
                                           entry.source.id + "'");
                }
            }

            clearError(error);
            return true;
        }

        static bool copyTree(const Path& source, const Path& destination, std::string* error)
        {
            std::error_code filesystemError;
            std::filesystem::recursive_directory_iterator iterator(
                source, std::filesystem::directory_options::none, filesystemError);
            const std::filesystem::recursive_directory_iterator end;
            if (filesystemError)
            {
                return fail(error, "previous asset generation could not be enumerated");
            }

            while (iterator != end)
            {
                const auto status = iterator->symlink_status(filesystemError);
                if (filesystemError || std::filesystem::is_symlink(status))
                {
                    return fail(error,
                                "previous asset generation contains an unreadable or symlink entry");
                }

                const Path relative = iterator->path().lexically_relative(source);
                const Path target = destination / relative;
                if (relative == Path(std::string(ManifestFilename)))
                {
                    iterator.increment(filesystemError);
                    if (filesystemError)
                    {
                        return fail(error, "previous asset generation could not be enumerated");
                    }
                    continue;
                }

                if (std::filesystem::is_directory(status))
                {
                    std::filesystem::create_directories(target, filesystemError);
                }
                else if (std::filesystem::is_regular_file(status))
                {
                    std::filesystem::create_directories(target.parent_path(), filesystemError);
                    if (!filesystemError)
                    {
                        std::filesystem::copy_file(iterator->path(), target,
                                                   std::filesystem::copy_options::overwrite_existing,
                                                   filesystemError);
                    }
                }
                else
                {
                    return fail(error,
                                "previous asset generation contains an unsupported file type");
                }

                if (filesystemError)
                {
                    return fail(error, "previous asset generation could not be copied");
                }

                iterator.increment(filesystemError);
                if (filesystemError)
                {
                    return fail(error, "previous asset generation could not be enumerated");
                }
            }

            clearError(error);
            return true;
        }

        static bool writeManifest(const Path& root, const AssetManifest& manifest,
                                  std::string* error)
        {
            const std::string document = manifest.serialize();
            std::ofstream stream(root / Path(std::string(ManifestFilename)),
                                 std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                return fail(error, "asset generation manifest could not be opened for writing");
            }

            stream.write(document.data(), static_cast<std::streamsize>(document.size()));
            stream.close();
            if (!stream)
            {
                return fail(error, "asset generation manifest could not be written completely");
            }

            clearError(error);
            return true;
        }

        static bool isWithin(const Path& root, const Path& candidate) noexcept
        {
            auto rootIterator = root.begin();
            auto candidateIterator = candidate.begin();
            for (; rootIterator != root.end(); ++rootIterator, ++candidateIterator)
            {
                if (candidateIterator == candidate.end() || *candidateIterator != *rootIterator)
                {
                    return false;
                }
            }
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

        Path m_publicationRoot;
    };
}

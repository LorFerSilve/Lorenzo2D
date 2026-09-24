#pragma once

#include <Lorenzo2D/Assets/AssetCookCache.hpp>

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <charconv>\n#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace l2d
{
    struct TextureAtlasRegion
    {
        AssetId sourceId;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        bool operator==(const TextureAtlasRegion& other) const noexcept
        {
            return sourceId == other.sourceId && x == other.x && y == other.y &&
                   width == other.width && height == other.height;
        }
    };

    class TextureAtlasMetadata
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1;
        static constexpr std::size_t MaxRegions = 256;
        static constexpr std::size_t MaxSerializedBytes = 1024u * 1024u;

        sf::Vector2u size() const noexcept
        {
            return m_size;
        }

        const std::vector<TextureAtlasRegion>& regions() const noexcept
        {
            return m_regions;
        }

        const TextureAtlasRegion* find(const AssetId& id) const noexcept
        {
            const auto found =
                std::lower_bound(m_regions.begin(), m_regions.end(), id,
                                 [](const TextureAtlasRegion& region, const AssetId& value)
                                 { return region.sourceId < value; });
            return found != m_regions.end() && found->sourceId == id ? &*found : nullptr;
        }

        std::string serialize() const
        {
            std::string output = "L2D-TEXTURE-ATLAS\n";
            output += std::to_string(FormatVersion) + "\n";
            output += std::to_string(m_size.x) + " " + std::to_string(m_size.y) + "\n";
            output += std::to_string(m_regions.size()) + "\n";
            for (const auto& region : m_regions)
            {
                output += region.sourceId + "\t" + std::to_string(region.x) + "\t" +
                          std::to_string(region.y) + "\t" + std::to_string(region.width) + "\t" +
                          std::to_string(region.height) + "\n";
            }
            return output;
        }

        bool deserialize(const std::string_view document, std::string* error = nullptr)
        {
            if (document.size() > MaxSerializedBytes)
            {
                return fail(error, "texture atlas metadata exceeds the configured byte limit");
            }

            std::size_t offset = 0;
            std::string_view line;
            if (!readLine(document, offset, line) || line != "L2D-TEXTURE-ATLAS")
            {
                return fail(error, "texture atlas metadata has an invalid signature");
            }

            std::uint32_t version = 0;
            if (!readLine(document, offset, line) || !parseUnsigned(line, version) ||
                version != FormatVersion)
            {
                return fail(error, "texture atlas metadata version is unsupported");
            }

            if (!readLine(document, offset, line))
            {
                return fail(error, "texture atlas metadata is truncated");
            }
            const auto sizeSeparator = line.find(' ');
            sf::Vector2u size;
            if (sizeSeparator == std::string_view::npos ||
                !parseUnsigned(line.substr(0, sizeSeparator), size.x) ||
                !parseUnsigned(line.substr(sizeSeparator + 1), size.y) || size.x == 0u ||
                size.y == 0u)
            {
                return fail(error, "texture atlas metadata contains an invalid atlas size");
            }

            std::size_t regionCount = 0;
            if (!readLine(document, offset, line) || !parseUnsigned(line, regionCount) ||
                regionCount == 0u || regionCount > MaxRegions)
            {
                return fail(error, "texture atlas metadata contains an invalid region count");
            }

            std::vector<TextureAtlasRegion> regions;
            regions.reserve(regionCount);
            std::set<AssetId> seen;
            for (std::size_t index = 0; index < regionCount; ++index)
            {
                if (!readLine(document, offset, line))
                {
                    return fail(error, "texture atlas metadata is truncated");
                }

                std::vector<std::string_view> fields;
                std::size_t fieldOffset = 0;
                while (fieldOffset <= line.size())
                {
                    const auto separator = line.find('\t', fieldOffset);
                    if (separator == std::string_view::npos)
                    {
                        fields.push_back(line.substr(fieldOffset));
                        break;
                    }
                    fields.push_back(line.substr(fieldOffset, separator - fieldOffset));
                    fieldOffset = separator + 1u;
                }
                if (fields.size() != 5u)
                {
                    return fail(error, "texture atlas region record is malformed");
                }

                TextureAtlasRegion region;
                region.sourceId.assign(fields[0].begin(), fields[0].end());
                if (!AssetMetadataRegistry::isValidAssetId(region.sourceId) ||
                    !parseUnsigned(fields[1], region.x) || !parseUnsigned(fields[2], region.y) ||
                    !parseUnsigned(fields[3], region.width) ||
                    !parseUnsigned(fields[4], region.height) || region.width == 0u ||
                    region.height == 0u || region.x > size.x || region.y > size.y ||
                    region.width > size.x - region.x || region.height > size.y - region.y)
                {
                    return fail(error, "texture atlas region record is invalid");
                }
                if (!seen.insert(region.sourceId).second)
                {
                    return fail(error, "texture atlas metadata contains duplicate source ids");
                }
                regions.push_back(std::move(region));
            }

            if (offset != document.size())
            {
                return fail(error, "texture atlas metadata contains trailing data");
            }

            std::sort(regions.begin(), regions.end(),
                      [](const TextureAtlasRegion& left, const TextureAtlasRegion& right)
                      { return left.sourceId < right.sourceId; });
            m_size = size;
            m_regions = std::move(regions);
            clearError(error);
            return true;
        }

      private:
        friend class AssetTextureAtlasCooker;

        static bool readLine(const std::string_view document, std::size_t& offset,
                             std::string_view& output) noexcept
        {
            if (offset >= document.size())
            {
                return false;
            }
            const auto end = document.find('\n', offset);
            if (end == std::string_view::npos)
            {
                return false;
            }
            output = document.substr(offset, end - offset);
            offset = end + 1u;
            return true;
        }

        template <typename T>
        static bool parseUnsigned(const std::string_view value, T& output) noexcept
        {
            if (value.empty())
            {
                return false;
            }
            T parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            {
                return false;
            }
            output = parsed;
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

        sf::Vector2u m_size{};
        std::vector<TextureAtlasRegion> m_regions;
    };

    // Deterministic Phase 14.6 texture-atlas cooker.
    //
    // The atlas is a generated Texture asset whose source dependencies are canonical manifest
    // texture entries. Source IDs are sorted before packing, no rotation is performed, and the
    // configured shelf-packing policy is part of the cook identity. Publication writes both the
    // PNG and its ".regions" metadata sidecar as one recoverable generation.
    class AssetTextureAtlasCooker
    {
      public:
        static constexpr std::string_view TextureAtlasImporter = "l2d.texture.atlas";
        static constexpr std::string_view ImporterVersion = "1";
        static constexpr std::size_t MaxSources = TextureAtlasMetadata::MaxRegions;
        static constexpr std::uint32_t MaxAtlasDimension = 8192u;
        static constexpr std::uint32_t MaxPadding = 32u;

        AssetTextureAtlasCooker(std::filesystem::path projectRoot, AssetManifest sourceManifest,
                                const std::uint32_t maxDimension = 4096u,
                                const std::uint32_t padding = 1u)
            : m_projectRoot(std::move(projectRoot)), m_sourceManifest(std::move(sourceManifest)),
              m_maxDimension(maxDimension), m_padding(padding)
        {
        }

        static std::filesystem::path metadataPath(const std::filesystem::path& cookedPath)
        {
            auto result = cookedPath;
            result += ".regions";
            return result;
        }

        bool makeManifestEntry(const AssetId& atlasId, std::vector<AssetId> sourceIds,
                               const std::filesystem::path& cookedPath, AssetManifestEntry& output,
                               std::string* error = nullptr) const
        {
            if (!validConfiguration(error))
            {
                return false;
            }
            if (!AssetMetadataRegistry::isValidAssetId(atlasId))
            {
                return fail(error, "texture atlas id is invalid");
            }
            if (!validateCookedPath(cookedPath, error))
            {
                return false;
            }
            if (!canonicalizeSources(sourceIds, error))
            {
                return false;
            }

            std::uint64_t sourceHash = 0;
            if (!computeSourceHash(sourceIds, sourceHash, error))
            {
                return false;
            }

            AssetSourceDescriptor source;
            source.id = atlasId;
            source.kind = AssetSourceKind::Texture;
            source.sourcePath = "generated/texture-atlas";
            source.importer = std::string(TextureAtlasImporter);
            source.importSettings.emplace("max_dimension", std::to_string(m_maxDimension));
            source.importSettings.emplace("padding", std::to_string(m_padding));
            source.dependencies = std::move(sourceIds);

            AssetCookRequest request;
            if (!AssetManifest::makeCookRequest(source, sourceHash, std::string(ImporterVersion),
                                                request, error))
            {
                return false;
            }

            AssetManifestEntry candidate;
            candidate.source = request.source;
            candidate.sourceContentHash = request.sourceContentHash;
            candidate.importerVersion = request.importerVersion;
            candidate.cookKey = request.cookKey;
            candidate.cookedPath = cookedPath;
            if (!AssetManifest::validate(candidate, error))
            {
                return false;
            }

            output = std::move(candidate);
            clearError(error);
            return true;
        }

        AssetCookExecutor::CookFunction cookFunction() const
        {
            const auto root = m_projectRoot;
            const auto manifest = m_sourceManifest;
            const auto maxDimension = m_maxDimension;
            const auto padding = m_padding;
            return [root, manifest, maxDimension, padding](const AssetCookRequest& request,
                                                           const std::filesystem::path& cookedPath,
                                                           std::string* error)
            {
                return AssetTextureAtlasCooker(root, manifest, maxDimension, padding)
                    .cook(request, cookedPath, error);
            };
        }

        bool cook(const AssetCookRequest& request, const std::filesystem::path& cookedPath,
                  std::string* error = nullptr) const
        {
            if (!validConfiguration(error) || !validateCookedPath(cookedPath, error) ||
                !validateRequest(request, error))
            {
                return false;
            }

            std::uint64_t sourceHash = 0;
            if (!computeSourceHash(request.source.dependencies, sourceHash, error))
            {
                return false;
            }
            if (sourceHash != request.sourceContentHash)
            {
                return fail(error,
                            "texture atlas dependencies changed after the cook identity was created");
            }

            std::vector<LoadedImage> images;
            if (!loadImages(request.source.dependencies, images, error))
            {
                return false;
            }

            TextureAtlasMetadata metadata;
            if (!pack(images, metadata, error))
            {
                return false;
            }

            sf::Image atlas(metadata.size(), sf::Color::Transparent);
            for (std::size_t imageIndex = 0; imageIndex < images.size(); ++imageIndex)
            {
                const auto* region = metadata.find(images[imageIndex].id);
                if (region == nullptr)
                {
                    return fail(error, "texture atlas packing lost a source region");
                }
                const auto sourceSize = images[imageIndex].image.getSize();
                for (unsigned int y = 0; y < sourceSize.y; ++y)
                {
                    for (unsigned int x = 0; x < sourceSize.x; ++x)
                    {
                        atlas.setPixel({region->x + x, region->y + y},
                                       images[imageIndex].image.getPixel({x, y}));
                    }
                }
            }

            std::filesystem::path destination;
            if (!prepareDestination(cookedPath, destination, error))
            {
                return false;
            }
            const auto regionsDestination = destination.string() + ".regions";
            if (!validateFinalDestination(regionsDestination, error))
            {
                return false;
            }

            const auto staged = stagedImagePath(destination);
            const auto backup = backupImagePath(destination);
            const auto regionsStaged = std::filesystem::path(regionsDestination.string() + ".l2d-tmp");
            const auto regionsBackup =
                std::filesystem::path(regionsDestination.string() + ".l2d-backup");

            if (!recoverGeneration(destination, staged, backup, regionsDestination, regionsStaged,
                                   regionsBackup, error))
            {
                return false;
            }

            if (!atlas.saveToFile(staged))
            {
                removeBestEffort(staged);
                return fail(error, "texture atlas image could not be staged");
            }
            if (!writeText(regionsStaged, metadata.serialize(), error))
            {
                removeBestEffort(staged);
                removeBestEffort(regionsStaged);
                return false;
            }

            sf::Image validationImage;
            if (!validationImage.loadFromFile(staged) || validationImage.getSize() != metadata.size())
            {
                removeBestEffort(staged);
                removeBestEffort(regionsStaged);
                return fail(error, "staged texture atlas image failed validation");
            }
            TextureAtlasMetadata validationMetadata;
            std::string metadataText;
            if (!readText(regionsStaged, metadataText, error) ||
                !validationMetadata.deserialize(metadataText, error) ||
                validationMetadata.serialize() != metadata.serialize())
            {
                removeBestEffort(staged);
                removeBestEffort(regionsStaged);
                return false;
            }

            if (!publishGeneration(destination, staged, backup, regionsDestination, regionsStaged,
                                   regionsBackup, error))
            {
                removeBestEffort(staged);
                removeBestEffort(regionsStaged);
                return false;
            }

            clearError(error);
            return true;
        }

        bool readPublishedMetadata(const std::filesystem::path& cookedPath,
                                   TextureAtlasMetadata& output,
                                   std::string* error = nullptr) const
        {
            if (!validateCookedPath(cookedPath, error))
            {
                return false;
            }
            std::filesystem::path resolved;
            if (!resolvePublished(metadataPath(cookedPath), resolved, error))
            {
                return false;
            }
            std::string document;
            if (!readText(resolved, document, error))
            {
                return false;
            }
            TextureAtlasMetadata candidate;
            if (!candidate.deserialize(document, error))
            {
                return false;
            }
            output = std::move(candidate);
            clearError(error);
            return true;
        }

      private:
        static constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        static constexpr std::uint64_t FnvPrime = 1099511628211ull;

        struct LoadedImage
        {
            AssetId id;
            sf::Image image;
        };

        bool validConfiguration(std::string* error = nullptr) const
        {
            if (m_maxDimension == 0u || m_maxDimension > MaxAtlasDimension)
            {
                return fail(error, "texture atlas maximum dimension is outside the supported range");
            }
            if (m_padding > MaxPadding)
            {
                return fail(error, "texture atlas padding exceeds the configured limit");
            }
            clearError(error);
            return true;
        }

        static bool validateCookedPath(const std::filesystem::path& path,
                                       std::string* error = nullptr)
        {
            if (!AssetMetadataRegistry::isValidSourcePath(path) ||
                path.generic_string().size() > AssetManifest::MaxPortablePathLength)
            {
                return fail(error,
                            "texture atlas cooked path must be a bounded project-relative path");
            }
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](const char value)
                           { return static_cast<char>(std::tolower(static_cast<unsigned char>(value))); });
            if (extension != ".png")
            {
                return fail(error, "texture atlas cooked artifact must use the .png extension");
            }
            return true;
        }

        bool validateRequest(const AssetCookRequest& request, std::string* error = nullptr) const
        {
            if (request.source.kind != AssetSourceKind::Texture ||
                request.source.importer != TextureAtlasImporter ||
                request.importerVersion != ImporterVersion)
            {
                return fail(error, "texture atlas cook request uses an unsupported importer contract");
            }
            if (request.source.importSettings.size() != 2u ||
                request.source.importSettings.find("max_dimension") ==
                    request.source.importSettings.end() ||
                request.source.importSettings.find("padding") == request.source.importSettings.end() ||
                request.source.importSettings.at("max_dimension") != std::to_string(m_maxDimension) ||
                request.source.importSettings.at("padding") != std::to_string(m_padding))
            {
                return fail(error, "texture atlas cook settings do not match the configured cooker");
            }

            AssetCookRequest canonical;
            if (!AssetManifest::makeCookRequest(request.source, request.sourceContentHash,
                                                request.importerVersion, canonical, error) ||
                canonical.cookKey != request.cookKey)
            {
                return fail(error, "texture atlas request is not a canonical manifest request");
            }

            auto dependencies = request.source.dependencies;
            if (!canonicalizeSources(dependencies, error) ||
                dependencies != request.source.dependencies)
            {
                return fail(error, "texture atlas dependencies are not canonical");
            }
            clearError(error);
            return true;
        }

        static bool canonicalizeSources(std::vector<AssetId>& sourceIds,
                                        std::string* error = nullptr)
        {
            if (sourceIds.empty() || sourceIds.size() > MaxSources)
            {
                return fail(error, "texture atlas requires a bounded non-empty source set");
            }
            for (const auto& id : sourceIds)
            {
                if (!AssetMetadataRegistry::isValidAssetId(id))
                {
                    return fail(error, "texture atlas contains an invalid source asset id");
                }
            }
            std::sort(sourceIds.begin(), sourceIds.end());
            if (std::adjacent_find(sourceIds.begin(), sourceIds.end()) != sourceIds.end())
            {
                return fail(error, "texture atlas contains duplicate source asset ids");
            }
            clearError(error);
            return true;
        }

        bool computeSourceHash(const std::vector<AssetId>& sourceIds, std::uint64_t& output,
                               std::string* error = nullptr) const
        {
            auto canonical = sourceIds;
            if (!canonicalizeSources(canonical, error))
            {
                return false;
            }

            std::uint64_t hash = FnvOffset;
            hashString(hash, "L2D-TEXTURE-ATLAS-SOURCE-V1");
            hashUint64(hash, m_maxDimension);
            hashUint64(hash, m_padding);
            hashUint64(hash, static_cast<std::uint64_t>(canonical.size()));
            for (const auto& id : canonical)
            {
                const auto* entry = m_sourceManifest.find(id);
                if (entry == nullptr)
                {
                    return fail(error, "texture atlas source dependency is missing from the manifest");
                }
                if (entry->source.kind != AssetSourceKind::Texture)
                {
                    return fail(error, "texture atlas source dependency is not a texture asset");
                }
                hashString(hash, id);
                hashUint64(hash, entry->cookKey);
                hashString(hash, entry->cookedPath.lexically_normal().generic_string());
            }
            output = hash;
            clearError(error);
            return true;
        }

        bool loadImages(const std::vector<AssetId>& sourceIds, std::vector<LoadedImage>& output,
                        std::string* error = nullptr) const
        {
            std::vector<LoadedImage> candidate;
            candidate.reserve(sourceIds.size());
            for (const auto& id : sourceIds)
            {
                const auto* entry = m_sourceManifest.find(id);
                if (entry == nullptr)
                {
                    return fail(error, "texture atlas source dependency disappeared");
                }

                std::filesystem::path sourcePath;
                if (!resolvePublished(entry->cookedPath, sourcePath, error))
                {
                    return false;
                }

                LoadedImage loaded;
                loaded.id = id;
                if (!loaded.image.loadFromFile(sourcePath))
                {
                    return fail(error, "texture atlas could not decode a cooked texture dependency");
                }
                const auto size = loaded.image.getSize();
                if (size.x == 0u || size.y == 0u || size.x > m_maxDimension ||
                    size.y > m_maxDimension)
                {
                    return fail(error, "texture atlas source dimensions exceed configured bounds");
                }
                candidate.push_back(std::move(loaded));
            }
            output = std::move(candidate);
            clearError(error);
            return true;
        }

        bool pack(const std::vector<LoadedImage>& images, TextureAtlasMetadata& output,
                  std::string* error = nullptr) const
        {
            std::uint32_t cursorX = 0;
            std::uint32_t cursorY = 0;
            std::uint32_t rowHeight = 0;
            std::uint32_t atlasWidth = 0;
            std::uint32_t atlasHeight = 0;
            std::vector<TextureAtlasRegion> regions;
            regions.reserve(images.size());

            for (const auto& loaded : images)
            {
                const auto size = loaded.image.getSize();
                const auto width = static_cast<std::uint32_t>(size.x);
                const auto height = static_cast<std::uint32_t>(size.y);

                if (cursorX != 0u && width > m_maxDimension - cursorX)
                {
                    cursorX = 0u;
                    if (rowHeight > m_maxDimension - cursorY)
                    {
                        return fail(error, "texture atlas sources exceed the configured atlas bounds");
                    }
                    cursorY += rowHeight;
                    rowHeight = 0u;
                }
                if (height > m_maxDimension - cursorY)
                {
                    return fail(error, "texture atlas sources exceed the configured atlas bounds");
                }

                regions.push_back({loaded.id, cursorX, cursorY, width, height});
                atlasWidth = std::max(atlasWidth, cursorX + width);
                atlasHeight = std::max(atlasHeight, cursorY + height);

                if (width > m_maxDimension - cursorX)
                {
                    return fail(error, "texture atlas source width exceeds configured bounds");
                }
                const auto advance = width + m_padding;
                cursorX = advance > m_maxDimension - cursorX ? m_maxDimension : cursorX + advance;
                rowHeight = std::max(rowHeight, height + m_padding);
            }

            if (atlasWidth == 0u || atlasHeight == 0u)
            {
                return fail(error, "texture atlas packing produced an empty atlas");
            }

            TextureAtlasMetadata candidate;
            candidate.m_size = {atlasWidth, atlasHeight};
            candidate.m_regions = std::move(regions);
            std::sort(candidate.m_regions.begin(), candidate.m_regions.end(),
                      [](const TextureAtlasRegion& left, const TextureAtlasRegion& right)
                      { return left.sourceId < right.sourceId; });
            output = std::move(candidate);
            clearError(error);
            return true;
        }

        bool canonicalProjectRoot(std::filesystem::path& output, std::string* error = nullptr) const
        {
            if (m_projectRoot.empty())
            {
                return fail(error, "texture atlas project root is empty");
            }
            std::error_code ec;
            const auto absolute = std::filesystem::absolute(m_projectRoot, ec);
            if (ec)
            {
                return fail(error, "texture atlas project root could not be resolved");
            }
            const auto canonical = std::filesystem::canonical(absolute, ec);
            if (ec || !std::filesystem::is_directory(canonical, ec) || ec)
            {
                return fail(error, "texture atlas project root is not a directory");
            }
            output = canonical;
            clearError(error);
            return true;
        }

        static bool isWithin(const std::filesystem::path& root,
                             const std::filesystem::path& candidate) noexcept
        {
            auto rootIt = root.begin();
            auto candidateIt = candidate.begin();
            for (; rootIt != root.end(); ++rootIt, ++candidateIt)
            {
                if (candidateIt == candidate.end() || *candidateIt != *rootIt)
                {
                    return false;
                }
            }
            return true;
        }

        bool resolvePublished(const std::filesystem::path& relative, std::filesystem::path& output,
                              std::string* error = nullptr) const
        {
            if (!AssetMetadataRegistry::isValidSourcePath(relative))
            {
                return fail(error, "texture atlas path is not a safe project-relative path");
            }
            std::filesystem::path root;
            if (!canonicalProjectRoot(root, error))
            {
                return false;
            }
            std::error_code ec;
            const auto candidate = std::filesystem::canonical(root / relative, ec);
            if (ec || !isWithin(root, candidate) ||
                !std::filesystem::is_regular_file(candidate, ec) || ec)
            {
                return fail(error, "texture atlas dependency is not a regular file inside the project");
            }
            output = candidate;
            clearError(error);
            return true;
        }

        bool prepareDestination(const std::filesystem::path& relative, std::filesystem::path& output,
                                std::string* error = nullptr) const
        {
            std::filesystem::path root;
            if (!canonicalProjectRoot(root, error))
            {
                return false;
            }

            std::filesystem::path current = root;
            std::error_code ec;
            for (const auto& component : relative.parent_path())
            {
                current /= component;
                const bool exists = std::filesystem::exists(current, ec);
                if (ec)
                {
                    return fail(error, "texture atlas output directory could not be inspected");
                }
                if (exists)
                {
                    const auto status = std::filesystem::symlink_status(current, ec);
                    if (ec || std::filesystem::is_symlink(status) ||
                        !std::filesystem::is_directory(status))
                    {
                        return fail(error,
                                    "texture atlas output crosses a symlink or non-directory");
                    }
                }
                else if (!std::filesystem::create_directory(current, ec) || ec)
                {
                    return fail(error, "texture atlas output directory could not be created");
                }
            }

            const auto destination = current / relative.filename();
            if (!validateFinalDestination(destination, error))
            {
                return false;
            }
            output = destination;
            clearError(error);
            return true;
        }

        static bool validateFinalDestination(const std::filesystem::path& destination,
                                             std::string* error = nullptr)
        {
            std::error_code ec;
            const bool exists = std::filesystem::exists(destination, ec);
            if (ec)
            {
                return fail(error, "texture atlas output destination could not be inspected");
            }
            if (!exists)
            {
                clearError(error);
                return true;
            }
            const auto status = std::filesystem::symlink_status(destination, ec);
            if (ec || std::filesystem::is_symlink(status) ||
                !std::filesystem::is_regular_file(status))
            {
                return fail(error, "texture atlas output destination must be a regular file");
            }
            clearError(error);
            return true;
        }

        static std::filesystem::path stagedImagePath(const std::filesystem::path& destination)
        {
            return destination.parent_path() /
                   (destination.stem().string() + ".l2d-tmp" + destination.extension().string());
        }

        static std::filesystem::path backupImagePath(const std::filesystem::path& destination)
        {
            return destination.parent_path() /
                   (destination.stem().string() + ".l2d-backup" + destination.extension().string());
        }

        static std::filesystem::path transactionPath(const std::filesystem::path& image)
        {
            auto result = image;
            result += ".l2d-transaction";
            return result;
        }

        static bool recoverGeneration(const std::filesystem::path& image,
                                      const std::filesystem::path& imageStaged,
                                      const std::filesystem::path& imageBackup,
                                      const std::filesystem::path& regions,
                                      const std::filesystem::path& regionsStaged,
                                      const std::filesystem::path& regionsBackup,
                                      std::string* error = nullptr)
        {
            removeBestEffort(imageStaged);
            removeBestEffort(regionsStaged);

            const auto transaction = transactionPath(image);
            std::error_code ec;
            const bool transactionExists = std::filesystem::exists(transaction, ec);
            if (ec)
            {
                return fail(error, "texture atlas transaction marker could not be inspected");
            }

            if (transactionExists)
            {
                std::string state;
                if (!readText(transaction, state, error) ||
                    (state != "0 0\n" && state != "0 1\n" && state != "1 0\n" &&
                     state != "1 1\n"))
                {
                    return fail(error, "texture atlas transaction marker is invalid");
                }

                const bool hadImage = state[0] == '1';
                const bool hadRegions = state[2] == '1';
                rollbackGeneration(image, imageBackup, hadImage, regions, regionsBackup, hadRegions);
                std::filesystem::remove(transaction, ec);
                if (ec)
                {
                    return fail(error, "texture atlas transaction marker could not be removed");
                }
            }

            if (!recoverStaleBackup(image, imageBackup, error) ||
                !recoverStaleBackup(regions, regionsBackup, error))
            {
                return false;
            }

            clearError(error);
            return true;
        }

        static bool recoverStaleBackup(const std::filesystem::path& destination,
                                       const std::filesystem::path& backup,
                                       std::string* error = nullptr)
        {
            std::error_code ec;
            const bool backupExists = std::filesystem::exists(backup, ec);
            if (ec)
            {
                return fail(error, "texture atlas publication backup could not be inspected");
            }
            if (!backupExists)
            {
                return true;
            }

            const bool destinationExists = std::filesystem::exists(destination, ec);
            if (ec)
            {
                return fail(error, "texture atlas destination could not be inspected");
            }
            if (destinationExists)
            {
                std::filesystem::remove(backup, ec);
                if (ec)
                {
                    return fail(error, "stale texture atlas publication backup could not be removed");
                }
            }
            else
            {
                std::filesystem::rename(backup, destination, ec);
                if (ec)
                {
                    return fail(error, "texture atlas publication backup could not be restored");
                }
            }
            return true;
        }

        static bool publishGeneration(const std::filesystem::path& image,
                                      const std::filesystem::path& imageStaged,
                                      const std::filesystem::path& imageBackup,
                                      const std::filesystem::path& regions,
                                      const std::filesystem::path& regionsStaged,
                                      const std::filesystem::path& regionsBackup,
                                      std::string* error = nullptr)
        {
            bool hadImage = false;
            bool hadRegions = false;
            if (!pathExists(image, hadImage, error) || !pathExists(regions, hadRegions, error))
            {
                return false;
            }

            if (hadImage && !renamePath(image, imageBackup, error))
            {
                return false;
            }
            if (hadRegions && !renamePath(regions, regionsBackup, error))
            {
                if (hadImage)
                {
                    renameBestEffort(imageBackup, image);
                }
                return false;
            }

            // The marker is written only after every previous artifact has reached its backup.
            // Therefore marker recovery can always restore the complete previous generation.
            const auto transaction = transactionPath(image);
            const std::string state = std::string(hadImage ? "1" : "0") + " " +
                                      (hadRegions ? "1\n" : "0\n");
            if (!writeText(transaction, state, error))
            {
                rollbackGeneration(image, imageBackup, hadImage, regions, regionsBackup, hadRegions);
                return false;
            }

            if (!renamePath(imageStaged, image, error))
            {
                rollbackGeneration(image, imageBackup, hadImage, regions, regionsBackup, hadRegions);
                removeBestEffort(transaction);
                return false;
            }
            if (!renamePath(regionsStaged, regions, error))
            {
                rollbackGeneration(image, imageBackup, hadImage, regions, regionsBackup, hadRegions);
                removeBestEffort(transaction);
                return false;
            }

            std::error_code ec;
            std::filesystem::remove(transaction, ec);
            if (ec)
            {
                rollbackGeneration(image, imageBackup, hadImage, regions, regionsBackup, hadRegions);
                removeBestEffort(transaction);
                return fail(error, "texture atlas transaction marker could not be committed");
            }

            removeBestEffort(imageBackup);
            removeBestEffort(regionsBackup);
            clearError(error);
            return true;
        }

        static void rollbackGeneration(const std::filesystem::path& image,
                                       const std::filesystem::path& imageBackup,
                                       const bool hadImage,
                                       const std::filesystem::path& regions,
                                       const std::filesystem::path& regionsBackup,
                                       const bool hadRegions) noexcept
        {
            removeBestEffort(image);
            removeBestEffort(regions);
            if (hadImage)
            {
                renameBestEffort(imageBackup, image);
            }
            else
            {
                removeBestEffort(imageBackup);
            }
            if (hadRegions)
            {
                renameBestEffort(regionsBackup, regions);
            }
            else
            {
                removeBestEffort(regionsBackup);
            }
        }

        static bool pathExists(const std::filesystem::path& path, bool& output,
                               std::string* error = nullptr)
        {
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            if (ec)
            {
                return fail(error, "texture atlas publication destination could not be inspected");
            }
            output = exists;
            clearError(error);
            return true;
        }

        static bool renamePath(const std::filesystem::path& from, const std::filesystem::path& to,
                               std::string* error = nullptr)
        {
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            if (ec)
            {
                return fail(error, "texture atlas generation could not be published transactionally");
            }
            return true;
        }

        static void renameBestEffort(const std::filesystem::path& from,
                                     const std::filesystem::path& to) noexcept
        {
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
        }

        static bool writeText(const std::filesystem::path& path, const std::string& text,
                              std::string* error = nullptr)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                return fail(error, "texture atlas metadata could not be staged");
            }
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();
            if (!stream)
            {
                return fail(error, "texture atlas metadata could not be written completely");
            }
            clearError(error);
            return true;
        }

        static bool readText(const std::filesystem::path& path, std::string& output,
                             std::string* error = nullptr)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return fail(error, "texture atlas metadata could not be opened");
            }
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            if (end < 0 || static_cast<std::uint64_t>(end) > TextureAtlasMetadata::MaxSerializedBytes)
            {
                return fail(error, "texture atlas metadata exceeds the configured byte limit");
            }
            stream.seekg(0, std::ios::beg);
            std::string candidate(static_cast<std::size_t>(end), '\0');
            if (!candidate.empty())
            {
                stream.read(candidate.data(), static_cast<std::streamsize>(candidate.size()));
                if (!stream)
                {
                    return fail(error, "texture atlas metadata could not be read completely");
                }
            }
            output = std::move(candidate);
            clearError(error);
            return true;
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
                hashByte(hash, static_cast<unsigned char>((value >> shift) & 0xffu));
            }
        }

        static void hashString(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hashUint64(hash, static_cast<std::uint64_t>(value.size()));
            for (const auto character : value)
            {
                hashByte(hash, static_cast<unsigned char>(character));
            }
        }

        static void removeBestEffort(const std::filesystem::path& path) noexcept
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
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

        std::filesystem::path m_projectRoot;
        AssetManifest m_sourceManifest;
        std::uint32_t m_maxDimension = 4096u;
        std::uint32_t m_padding = 1u;
    };
}

#pragma once

#include <Lorenzo2D/Assets/AssetCookCache.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace l2d
{
    // Concrete Phase 14.5 source-to-runtime artifact cooker.
    //
    // The built-in importers intentionally preserve runtime-native file bytes rather than
    // inventing a second cooked container format. They validate supported source signatures,
    // verify the source bytes still match AssetCookRequest::sourceContentHash, stage the output,
    // validate the staged artifact, and only then publish it transactionally.
    class AssetFileCooker
    {
      public:
        static constexpr std::size_t MaxSourceBytes = 64u * 1024u * 1024u;
        static constexpr std::string_view TextureImporter = "l2d.texture.copy";
        static constexpr std::string_view FontImporter = "l2d.font.copy";
        static constexpr std::string_view SoundImporter = "l2d.sound.copy";
        static constexpr std::string_view ShaderImporter = "l2d.shader.copy";
        static constexpr std::string_view BuiltInImporterVersion = "1";

        explicit AssetFileCooker(std::filesystem::path projectRoot)
            : m_projectRoot(std::move(projectRoot))
        {
        }

        const std::filesystem::path& projectRoot() const noexcept
        {
            return m_projectRoot;
        }

        static std::string_view importerVersion(const std::string_view importer) noexcept
        {
            return isBuiltInImporter(importer) ? BuiltInImporterVersion : std::string_view{};
        }

        bool makeManifestEntry(const AssetSourceDescriptor& source,
                               const std::filesystem::path& cookedPath, AssetManifestEntry& output,
                               std::string* error = nullptr) const
        {
            std::vector<unsigned char> bytes;
            if (!readAndValidateSource(source, bytes, error))
            {
                return false;
            }
            if (!validateCookedPath(source, cookedPath, error))
            {
                return false;
            }

            AssetCookRequest request;
            if (!AssetManifest::makeCookRequest(
                    source, hashBytes(bytes), std::string(BuiltInImporterVersion), request, error))
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

        bool sourceContentHash(const AssetSourceDescriptor& source, std::uint64_t& output,
                               std::string* error = nullptr) const
        {
            std::vector<unsigned char> bytes;
            if (!readAndValidateSource(source, bytes, error))
            {
                return false;
            }
            output = hashBytes(bytes);
            clearError(error);
            return true;
        }

        bool cook(const AssetCookRequest& request, const std::filesystem::path& cookedPath,
                  std::string* error = nullptr) const
        {
            if (!validateImporter(request.source, error))
            {
                return false;
            }
            if (request.importerVersion != BuiltInImporterVersion)
            {
                return fail(error, "asset file cooker importer version is unsupported");
            }
            if (!validateCookedPath(request.source, cookedPath, error))
            {
                return false;
            }

            AssetCookRequest canonicalRequest;
            std::string canonicalError;
            if (!AssetManifest::makeCookRequest(request.source, request.sourceContentHash,
                                                request.importerVersion, canonicalRequest,
                                                &canonicalError) ||
                canonicalRequest.cookKey != request.cookKey)
            {
                return fail(error, "asset file cooker request is not a canonical manifest request");
            }

            std::vector<unsigned char> sourceBytes;
            if (!readAndValidateSource(request.source, sourceBytes, error))
            {
                return false;
            }
            if (hashBytes(sourceBytes) != request.sourceContentHash)
            {
                return fail(error,
                            "asset source changed after the manifest cook identity was created");
            }

            std::filesystem::path destination;
            if (!prepareDestination(cookedPath, destination, error))
            {
                return false;
            }

            std::filesystem::path staged = destination;
            staged += ".l2d-tmp";
            std::filesystem::path backup = destination;
            backup += ".l2d-backup";

            if (!recoverInterruptedPublication(destination, staged, backup, error))
            {
                return false;
            }
            if (!writeFile(staged, sourceBytes, error))
            {
                removeBestEffort(staged);
                return false;
            }

            std::vector<unsigned char> stagedBytes;
            if (!readFile(staged, stagedBytes, error) ||
                !validatePayload(request.source, stagedBytes, error) ||
                hashBytes(stagedBytes) != request.sourceContentHash)
            {
                removeBestEffort(staged);
                if (error != nullptr && error->empty())
                {
                    *error = "staged cooked artifact does not match the validated source bytes";
                }
                return false;
            }

            if (!publish(destination, staged, backup, error))
            {
                removeBestEffort(staged);
                return false;
            }

            clearError(error);
            return true;
        }

        AssetCookExecutor::CookFunction cookFunction() const
        {
            const auto root = m_projectRoot;
            return [root](const AssetCookRequest& request, const std::filesystem::path& cookedPath,
                          std::string* error)
            { return AssetFileCooker(root).cook(request, cookedPath, error); };
        }

      private:
        static constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
        static constexpr std::uint64_t FnvPrime = 1099511628211ull;

        static bool isBuiltInImporter(const std::string_view importer) noexcept
        {
            return importer == TextureImporter || importer == FontImporter ||
                   importer == SoundImporter || importer == ShaderImporter;
        }

        static std::string lowercaseExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(
                extension.begin(), extension.end(), extension.begin(), [](const char value)
                { return static_cast<char>(std::tolower(static_cast<unsigned char>(value))); });
            return extension;
        }

        static bool validateImporter(const AssetSourceDescriptor& source,
                                     std::string* error = nullptr)
        {
            if (!AssetMetadataRegistry::validate(source, error))
            {
                return false;
            }

            if (source.importer == TextureImporter)
            {
                if (source.kind != AssetSourceKind::Texture)
                {
                    return fail(error, "texture file importer requires Texture source kind");
                }
            }
            else if (source.importer == FontImporter)
            {
                if (source.kind != AssetSourceKind::Font)
                {
                    return fail(error, "font file importer requires Font source kind");
                }
            }
            else if (source.importer == SoundImporter)
            {
                if (source.kind != AssetSourceKind::Sound)
                {
                    return fail(error, "sound file importer requires Sound source kind");
                }
            }
            else if (source.importer == ShaderImporter)
            {
                if (source.kind != AssetSourceKind::Shader)
                {
                    return fail(error, "shader file importer requires Shader source kind");
                }
            }
            else
            {
                return fail(error, "asset file cooker importer is unsupported");
            }

            if (!isSupportedExtension(source, lowercaseExtension(source.sourcePath)))
            {
                return fail(error, "asset file cooker source extension is unsupported");
            }
            clearError(error);
            return true;
        }

        static bool isSupportedExtension(const AssetSourceDescriptor& source,
                                         const std::string_view extension) noexcept
        {
            if (source.importer == TextureImporter)
            {
                return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                       extension == ".bmp";
            }
            if (source.importer == FontImporter)
            {
                return extension == ".ttf" || extension == ".otf";
            }
            if (source.importer == SoundImporter)
            {
                return extension == ".wav" || extension == ".ogg" || extension == ".flac";
            }
            if (source.importer == ShaderImporter)
            {
                return extension == ".vert" || extension == ".frag" || extension == ".glsl";
            }
            return false;
        }

        bool readAndValidateSource(const AssetSourceDescriptor& source,
                                   std::vector<unsigned char>& output,
                                   std::string* error = nullptr) const
        {
            if (!validateImporter(source, error))
            {
                return false;
            }

            std::filesystem::path sourcePath;
            if (!resolveSource(source.sourcePath, sourcePath, error))
            {
                return false;
            }
            if (!readFile(sourcePath, output, error))
            {
                return false;
            }
            if (!validatePayload(source, output, error))
            {
                return false;
            }
            clearError(error);
            return true;
        }

        static bool validateCookedPath(const AssetSourceDescriptor& source,
                                       const std::filesystem::path& cookedPath,
                                       std::string* error = nullptr)
        {
            if (!AssetMetadataRegistry::isValidSourcePath(cookedPath) ||
                cookedPath.generic_string().size() > AssetManifest::MaxPortablePathLength)
            {
                return fail(
                    error,
                    "cooked artifact path must be a bounded project-relative path without '..'");
            }
            if (lowercaseExtension(source.sourcePath) != lowercaseExtension(cookedPath))
            {
                return fail(error,
                            "passthrough cooked artifact must preserve the source file extension");
            }
            clearError(error);
            return true;
        }

        static bool hasPrefix(const std::vector<unsigned char>& bytes,
                              const std::initializer_list<unsigned int> prefix) noexcept
        {
            if (bytes.size() < prefix.size())
            {
                return false;
            }
            std::size_t index = 0;
            for (const auto value : prefix)
            {
                if (bytes[index] != static_cast<unsigned char>(value))
                {
                    return false;
                }
                ++index;
            }
            return true;
        }

        static bool validatePayload(const AssetSourceDescriptor& source,
                                    const std::vector<unsigned char>& bytes,
                                    std::string* error = nullptr)
        {
            const auto extension = lowercaseExtension(source.sourcePath);
            bool valid = false;
            if (extension == ".png")
            {
                valid = hasPrefix(bytes, {0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au});
            }
            else if (extension == ".jpg" || extension == ".jpeg")
            {
                valid = hasPrefix(bytes, {0xffu, 0xd8u, 0xffu});
            }
            else if (extension == ".bmp")
            {
                valid = hasPrefix(bytes, {'B', 'M'});
            }
            else if (extension == ".ttf")
            {
                valid = hasPrefix(bytes, {0x00u, 0x01u, 0x00u, 0x00u}) ||
                        hasPrefix(bytes, {'t', 'r', 'u', 'e'});
            }
            else if (extension == ".otf")
            {
                valid = hasPrefix(bytes, {'O', 'T', 'T', 'O'});
            }
            else if (extension == ".wav")
            {
                valid = bytes.size() >= 12u && hasPrefix(bytes, {'R', 'I', 'F', 'F'}) &&
                        bytes[8] == static_cast<unsigned char>('W') &&
                        bytes[9] == static_cast<unsigned char>('A') &&
                        bytes[10] == static_cast<unsigned char>('V') &&
                        bytes[11] == static_cast<unsigned char>('E');
            }
            else if (extension == ".ogg")
            {
                valid = hasPrefix(bytes, {'O', 'g', 'g', 'S'});
            }
            else if (extension == ".flac")
            {
                valid = hasPrefix(bytes, {'f', 'L', 'a', 'C'});
            }
            else if (extension == ".vert" || extension == ".frag" || extension == ".glsl")
            {
                valid = validateShaderText(bytes);
            }

            if (!valid)
            {
                return fail(error, "asset source bytes do not match the declared supported format");
            }
            clearError(error);
            return true;
        }

        static bool validateShaderText(const std::vector<unsigned char>& bytes) noexcept
        {
            bool hasVisibleCharacter = false;
            for (const auto byte : bytes)
            {
                if (byte == 0u)
                {
                    return false;
                }
                if (byte < 0x20u && byte != static_cast<unsigned char>('\t') &&
                    byte != static_cast<unsigned char>('\n') &&
                    byte != static_cast<unsigned char>('\r'))
                {
                    return false;
                }
                if (byte >= 0x80u || !std::isspace(byte))
                {
                    hasVisibleCharacter = true;
                }
            }
            return hasVisibleCharacter;
        }

        static std::uint64_t hashBytes(const std::vector<unsigned char>& bytes) noexcept
        {
            std::uint64_t hash = FnvOffset;
            for (const auto byte : bytes)
            {
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= FnvPrime;
            }
            return hash;
        }

        bool canonicalProjectRoot(std::filesystem::path& output, std::string* error = nullptr) const
        {
            if (m_projectRoot.empty())
            {
                return fail(error, "asset file cooker project root is empty");
            }

            std::error_code ec;
            auto absoluteRoot = std::filesystem::absolute(m_projectRoot, ec);
            if (ec)
            {
                return fail(error, "asset file cooker could not resolve the project root");
            }
            auto canonicalRoot = std::filesystem::canonical(absoluteRoot, ec);
            if (ec || !std::filesystem::is_directory(canonicalRoot, ec) || ec)
            {
                return fail(error, "asset file cooker project root does not exist as a directory");
            }
            output = std::move(canonicalRoot);
            clearError(error);
            return true;
        }

        static bool isWithin(const std::filesystem::path& root,
                             const std::filesystem::path& candidate) noexcept
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

        bool resolveSource(const std::filesystem::path& relative, std::filesystem::path& output,
                           std::string* error = nullptr) const
        {
            if (!AssetMetadataRegistry::isValidSourcePath(relative))
            {
                return fail(error, "asset source path is not a safe project-relative path");
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
                return fail(
                    error,
                    "asset source does not resolve to a regular file inside the project root");
            }
            output = candidate;
            clearError(error);
            return true;
        }

        bool prepareDestination(const std::filesystem::path& relative,
                                std::filesystem::path& output, std::string* error = nullptr) const
        {
            std::filesystem::path root;
            if (!canonicalProjectRoot(root, error))
            {
                return false;
            }

            std::filesystem::path current = root;
            const auto parent = relative.parent_path();
            std::error_code ec;
            for (const auto& component : parent)
            {
                current /= component;
                const auto status = std::filesystem::symlink_status(current, ec);
                if (ec)
                {
                    return fail(error,
                                "asset cooker could not inspect the cooked artifact directory");
                }
                if (std::filesystem::exists(status))
                {
                    if (std::filesystem::is_symlink(status) ||
                        !std::filesystem::is_directory(status))
                    {
                        return fail(
                            error,
                            "cooked artifact path crosses a symlink or non-directory component");
                    }
                }
                else if (!std::filesystem::create_directory(current, ec) || ec)
                {
                    return fail(error,
                                "asset cooker could not create the cooked artifact directory");
                }
            }

            const auto destination = current / relative.filename();
            const auto status = std::filesystem::symlink_status(destination, ec);
            if (ec)
            {
                return fail(error,
                            "asset cooker could not inspect the cooked artifact destination");
            }
            if (std::filesystem::exists(status))
            {
                if (std::filesystem::is_symlink(status))
                {
                    return fail(error, "cooked artifact destination cannot be a symlink");
                }
                if (!std::filesystem::is_regular_file(status))
                {
                    return fail(error,
                                "existing cooked artifact destination must be a regular file");
                }
            }
            output = destination;
            clearError(error);
            return true;
        }

        static bool readFile(const std::filesystem::path& path, std::vector<unsigned char>& output,
                             std::string* error = nullptr)
        {
            std::error_code ec;
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size > MaxSourceBytes ||
                size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
            {
                return fail(error, "asset file is unreadable or exceeds the configured byte limit");
            }

            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return fail(error, "asset file could not be opened for reading");
            }

            std::vector<unsigned char> candidate(static_cast<std::size_t>(size));
            if (!candidate.empty())
            {
                stream.read(reinterpret_cast<char*>(candidate.data()),
                            static_cast<std::streamsize>(candidate.size()));
                if (!stream)
                {
                    return fail(error, "asset file could not be read completely");
                }
            }
            output = std::move(candidate);
            clearError(error);
            return true;
        }

        static bool writeFile(const std::filesystem::path& path,
                              const std::vector<unsigned char>& bytes, std::string* error = nullptr)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                return fail(error, "staged cooked artifact could not be opened for writing");
            }
            if (!bytes.empty())
            {
                stream.write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size()));
            }
            stream.close();
            if (!stream)
            {
                return fail(error, "staged cooked artifact could not be written completely");
            }
            clearError(error);
            return true;
        }

        static bool recoverInterruptedPublication(const std::filesystem::path& destination,
                                                  const std::filesystem::path& staged,
                                                  const std::filesystem::path& backup,
                                                  std::string* error = nullptr)
        {
            std::error_code ec;
            removeBestEffort(staged);
            const bool backupExists = std::filesystem::exists(backup, ec);
            if (ec)
            {
                return fail(error,
                            "asset cooker could not inspect an interrupted publication backup");
            }
            if (!backupExists)
            {
                clearError(error);
                return true;
            }

            const bool destinationExists = std::filesystem::exists(destination, ec);
            if (ec)
            {
                return fail(error,
                            "asset cooker could not inspect the cooked artifact destination");
            }
            if (destinationExists)
            {
                std::filesystem::remove(backup, ec);
                if (ec)
                {
                    return fail(error, "asset cooker could not remove a stale publication backup");
                }
            }
            else
            {
                std::filesystem::rename(backup, destination, ec);
                if (ec)
                {
                    return fail(error, "asset cooker could not recover an interrupted publication");
                }
            }
            clearError(error);
            return true;
        }

        static bool publish(const std::filesystem::path& destination,
                            const std::filesystem::path& staged,
                            const std::filesystem::path& backup, std::string* error = nullptr)
        {
            std::error_code ec;
            const bool hadDestination = std::filesystem::exists(destination, ec);
            if (ec)
            {
                return fail(error, "asset cooker could not inspect the existing cooked artifact");
            }

            if (hadDestination)
            {
                std::filesystem::rename(destination, backup, ec);
                if (ec)
                {
                    return fail(error, "asset cooker could not stage the previous cooked artifact");
                }
            }

            std::filesystem::rename(staged, destination, ec);
            if (ec)
            {
                if (hadDestination)
                {
                    std::error_code restoreError;
                    std::filesystem::rename(backup, destination, restoreError);
                }
                return fail(error, "asset cooker could not publish the validated cooked artifact");
            }

            if (hadDestination)
            {
                removeBestEffort(backup);
            }
            clearError(error);
            return true;
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
    };
}

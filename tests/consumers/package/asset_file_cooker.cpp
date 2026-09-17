#include <Lorenzo2D/Assets/AssetFileCooker.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    bool writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
        }
        stream.close();
        return static_cast<bool>(stream);
    }

    bool writeText(const std::filesystem::path& path, const std::string& text)
    {
        return writeBytes(path, std::vector<unsigned char>(text.begin(), text.end()));
    }

    std::vector<unsigned char> readBytes(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return {};
        }
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    l2d::AssetSourceDescriptor textureDescriptor()
    {
        l2d::AssetSourceDescriptor source;
        source.id = "textures/phase14/player";
        source.kind = l2d::AssetSourceKind::Texture;
        source.sourcePath = "sources/player.png";
        source.importer = std::string(l2d::AssetFileCooker::TextureImporter);
        source.importSettings = {{"filter", "linear"}};
        return source;
    }

    l2d::AssetSourceDescriptor shaderDescriptor()
    {
        l2d::AssetSourceDescriptor source;
        source.id = "shaders/phase14/basic";
        source.kind = l2d::AssetSourceKind::Shader;
        source.sourcePath = "sources/basic.frag";
        source.importer = std::string(l2d::AssetFileCooker::ShaderImporter);
        return source;
    }
}

int main()
{
    std::error_code filesystemError;
    const auto tempRoot = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        return 1;
    }

    const auto projectRoot = tempRoot / "lorenzo2d-phase14-file-cooker-consumer";
    std::filesystem::remove_all(projectRoot, filesystemError);
    filesystemError.clear();
    if (!std::filesystem::create_directories(projectRoot / "sources", filesystemError) ||
        filesystemError)
    {
        return 2;
    }

    const std::vector<unsigned char> pngBytes = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au, 0x00u, 0x00u, 0x00u, 0x0du,
        'I',   'H',   'D',   'R',   0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u};
    const std::string shaderText = "#version 330 core\nvoid main() {}\n";
    if (!writeBytes(projectRoot / "sources/player.png", pngBytes) ||
        !writeText(projectRoot / "sources/basic.frag", shaderText))
    {
        return 3;
    }

    l2d::AssetFileCooker cooker(projectRoot);
    if (l2d::AssetFileCooker::importerVersion(l2d::AssetFileCooker::TextureImporter) != "1" ||
        !l2d::AssetFileCooker::importerVersion("unknown").empty())
    {
        return 4;
    }

    const auto texture = textureDescriptor();
    const auto shader = shaderDescriptor();
    l2d::AssetManifestEntry textureEntry;
    l2d::AssetManifestEntry shaderEntry;
    std::string error;
    if (!cooker.makeManifestEntry(texture, "cooked/player.png", textureEntry, &error) ||
        !error.empty() ||
        !cooker.makeManifestEntry(shader, "cooked/basic.frag", shaderEntry, &error) ||
        !error.empty() || textureEntry.importerVersion != "1" || shaderEntry.importerVersion != "1")
    {
        return 5;
    }

    std::uint64_t textureHash = 0;
    if (!cooker.sourceContentHash(texture, textureHash, &error) ||
        textureHash != textureEntry.sourceContentHash || !error.empty())
    {
        return 6;
    }

    l2d::AssetManifest current;
    if (!current.upsert(textureEntry, &error) || !current.upsert(shaderEntry, &error))
    {
        return 7;
    }

    l2d::AssetManifest previous;
    l2d::AssetCookCache cache;
    l2d::AssetCookExecutionResult first;
    if (!l2d::AssetCookExecutor::execute(previous, current, cache, 2u, cooker.cookFunction(), first,
                                         &error) ||
        !error.empty() || first.cooked.size() != 2u || !first.remaining.empty() ||
        cache.size() != 2u)
    {
        return 8;
    }

    if (readBytes(projectRoot / "cooked/player.png") != pngBytes ||
        readBytes(projectRoot / "cooked/basic.frag") !=
            std::vector<unsigned char>(shaderText.begin(), shaderText.end()))
    {
        return 9;
    }

    l2d::AssetCookExecutionResult cached;
    if (!l2d::AssetCookExecutor::execute(previous, current, cache, 2u, cooker.cookFunction(),
                                         cached, &error) ||
        !cached.cooked.empty() || !cached.remaining.empty() || cache.size() != 2u)
    {
        return 10;
    }

    auto changedPng = pngBytes;
    changedPng.push_back(0x2au);
    if (!writeBytes(projectRoot / "sources/player.png", changedPng))
    {
        return 11;
    }

    l2d::AssetManifest textureOnly;
    if (!textureOnly.upsert(textureEntry, &error))
    {
        return 12;
    }
    l2d::AssetCookCache freshCache;
    l2d::AssetCookExecutionResult failed = {{"sentinel"}, {"sentinel"}};
    if (l2d::AssetCookExecutor::execute(previous, textureOnly, freshCache, 1u,
                                        cooker.cookFunction(), failed, &error) ||
        error.find("source changed") == std::string::npos || freshCache.size() != 0u ||
        failed.cooked != std::vector<l2d::AssetId>({"sentinel"}) ||
        readBytes(projectRoot / "cooked/player.png") != pngBytes)
    {
        return 13;
    }

    if (!writeText(projectRoot / "sources/bad.png", "not a png"))
    {
        return 14;
    }
    auto malformed = textureDescriptor();
    malformed.id = "textures/phase14/bad";
    malformed.sourcePath = "sources/bad.png";
    l2d::AssetManifestEntry preserved;
    preserved.source.id = "sentinel";
    if (cooker.makeManifestEntry(malformed, "cooked/bad.png", preserved, &error) || error.empty() ||
        preserved.source.id != "sentinel")
    {
        return 15;
    }

    l2d::AssetManifestEntry wrongExtension;
    if (cooker.makeManifestEntry(shader, "cooked/basic.bin", wrongExtension, &error) ||
        error.find("preserve the source file extension") == std::string::npos)
    {
        return 16;
    }

    auto wrongImporter = shader;
    wrongImporter.id = "shaders/phase14/unsupported";
    wrongImporter.importer = "custom.unknown";
    if (cooker.makeManifestEntry(wrongImporter, "cooked/unsupported.frag", wrongExtension,
                                 &error) ||
        error.find("unsupported") == std::string::npos)
    {
        return 17;
    }

    filesystemError.clear();
    if (!std::filesystem::create_directories(projectRoot / "blocked/player.png", filesystemError) ||
        filesystemError)
    {
        return 18;
    }

    l2d::AssetManifestEntry blockedEntry;
    if (!cooker.makeManifestEntry(texture, "blocked/player.png", blockedEntry, &error))
    {
        return 19;
    }
    l2d::AssetManifest blockedManifest;
    if (!blockedManifest.upsert(blockedEntry, &error))
    {
        return 20;
    }

    l2d::AssetCookCache blockedCache;
    l2d::AssetCookExecutionResult blockedResult = {{"sentinel"}, {"sentinel"}};
    if (l2d::AssetCookExecutor::execute(previous, blockedManifest, blockedCache, 1u,
                                        cooker.cookFunction(), blockedResult, &error) ||
        error.find("regular file") == std::string::npos || blockedCache.size() != 0u ||
        blockedResult.cooked != std::vector<l2d::AssetId>({"sentinel"}) ||
        !std::filesystem::is_directory(projectRoot / "blocked/player.png", filesystemError) ||
        filesystemError)
    {
        return 21;
    }

    std::filesystem::remove_all(projectRoot, filesystemError);
    return filesystemError ? 22 : 0;
}

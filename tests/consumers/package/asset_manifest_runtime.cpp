#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Assets/AssetManifestResourceLocator.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main()
{
    std::error_code filesystemError;
    const auto tempRoot = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        return 1;
    }

    const auto root = tempRoot / "lorenzo2d-manifest-runtime-consumer";
    std::filesystem::remove_all(root, filesystemError);
    filesystemError.clear();
    if (!std::filesystem::create_directories(root / "cooked", filesystemError) || filesystemError)
    {
        return 2;
    }

    {
        std::ofstream stream(root / "cooked/effect.frag", std::ios::binary | std::ios::trunc);
        stream << "void main() {}\n";
        if (!stream)
        {
            return 3;
        }
    }

    l2d::AssetSourceDescriptor source;
    source.id = "shaders/consumer/effect";
    source.kind = l2d::AssetSourceKind::Shader;
    source.sourcePath = "sources/effect.frag";
    source.importer = "consumer-runtime";

    l2d::AssetCookRequest request;
    std::string error;
    if (!l2d::AssetManifest::makeCookRequest(source, 42u, "1", request, &error))
    {
        return 4;
    }

    l2d::AssetManifestEntry entry;
    entry.source = request.source;
    entry.sourceContentHash = request.sourceContentHash;
    entry.importerVersion = request.importerVersion;
    entry.cookKey = request.cookKey;
    entry.cookedPath = "cooked/effect.frag";

    l2d::AssetManifest manifest;
    if (!manifest.upsert(entry, &error))
    {
        return 5;
    }

    l2d::AssetManifestResourceLocator locator(manifest);
    if (!locator.addRoot(root, &error) || !error.empty())
    {
        return 6;
    }

    l2d::AssetManifestResource resolved;
    if (!locator.resolve(source.id, l2d::AssetSourceKind::Shader, resolved, &error) ||
        !error.empty() || resolved.id != source.id || resolved.path != root / entry.cookedPath ||
        resolved.cookKey != entry.cookKey)
    {
        return 7;
    }

    if (locator.contains(source.id, l2d::AssetSourceKind::Texture) ||
        !locator.contains(source.id, l2d::AssetSourceKind::Shader))
    {
        return 8;
    }

    l2d::AssetManager assets;
    if (assets.loadTexture(locator, source.id) || assets.loadFont(locator, source.id) ||
        assets.loadSoundBuffer(locator, source.id))
    {
        return 9;
    }

    std::filesystem::remove_all(root, filesystemError);
    return filesystemError ? 10 : 0;
}

#include <Lorenzo2D/Assets/AssetGeneration.hpp>

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

    const auto root = tempRoot / "lorenzo2d-generation-package-consumer";
    std::filesystem::remove_all(root, filesystemError);
    filesystemError.clear();

    l2d::AssetSourceDescriptor source;
    source.id = "shaders/consumer";
    source.kind = l2d::AssetSourceKind::Shader;
    source.sourcePath = "sources/consumer.frag";
    source.importer = "generation-consumer";

    l2d::AssetCookRequest request;
    std::string error;
    if (!l2d::AssetManifest::makeCookRequest(source, 42u, "1", request, &error))
    {
        return 2;
    }

    l2d::AssetManifestEntry entry;
    entry.source = request.source;
    entry.sourceContentHash = request.sourceContentHash;
    entry.importerVersion = request.importerVersion;
    entry.cookKey = request.cookKey;
    entry.cookedPath = "cooked/consumer.frag";

    l2d::AssetManifest manifest;
    if (!manifest.upsert(entry, &error))
    {
        return 3;
    }

    l2d::AssetGenerationPublisher publisher(root / "published");
    l2d::AssetGenerationStaging staging;
    if (!publisher.prepare(manifest, staging, &error))
    {
        return 4;
    }

    std::filesystem::create_directories((staging.root / entry.cookedPath).parent_path(),
                                        filesystemError);
    if (filesystemError)
    {
        return 5;
    }

    {
        std::ofstream stream(staging.root / entry.cookedPath, std::ios::binary | std::ios::trunc);
        stream << "void main() {}\n";
        if (!stream)
        {
            return 6;
        }
    }

    l2d::AssetContentGeneration generation;
    if (!publisher.publish(staging, generation, &error) || !error.empty())
    {
        return 7;
    }

    l2d::AssetManifestResourceLocator locator;
    if (!generation.configure(locator, &error) || !error.empty())
    {
        return 8;
    }

    l2d::AssetManifestResource resolved;
    if (!locator.resolve(source.id, l2d::AssetSourceKind::Shader, resolved, &error) ||
        resolved.path != generation.root / entry.cookedPath)
    {
        return 9;
    }

    std::filesystem::remove_all(root, filesystemError);
    return filesystemError ? 10 : 0;
}

#include <Lorenzo2D/Assets/AssetGeneration.hpp>

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: Lorenzo2DPhase14ContentGenerationExample "
                     "<publication-root> <artifact-file> <asset-id>\n";
        return 1;
    }

    const std::filesystem::path artifact = argv[2];
    if (!std::filesystem::is_regular_file(artifact))
    {
        std::cerr << "artifact file does not exist\n";
        return 2;
    }

    l2d::AssetSourceDescriptor source;
    source.id = argv[3];
    source.kind = l2d::AssetSourceKind::Shader;
    source.sourcePath = "sources/example.frag";
    source.importer = "phase14-generation-example";

    l2d::AssetCookRequest request;
    std::string error;
    if (!l2d::AssetManifest::makeCookRequest(source, 1u, "1", request, &error))
    {
        std::cerr << error << '\n';
        return 3;
    }

    l2d::AssetManifestEntry entry;
    entry.source = request.source;
    entry.sourceContentHash = request.sourceContentHash;
    entry.importerVersion = request.importerVersion;
    entry.cookKey = request.cookKey;
    entry.cookedPath = std::filesystem::path("cooked") / artifact.filename();

    l2d::AssetManifest manifest;
    if (!manifest.upsert(entry, &error))
    {
        std::cerr << error << '\n';
        return 4;
    }

    l2d::AssetGenerationPublisher publisher(argv[1]);
    l2d::AssetGenerationStaging staging;
    if (!publisher.prepare(manifest, staging, &error))
    {
        std::cerr << error << '\n';
        return 5;
    }

    std::filesystem::create_directories((staging.root / entry.cookedPath).parent_path());
    std::filesystem::copy_file(artifact, staging.root / entry.cookedPath);

    l2d::AssetContentGeneration generation;
    if (!publisher.publish(staging, generation, &error))
    {
        std::cerr << error << '\n';
        return 6;
    }

    l2d::AssetManifestResourceLocator resources;
    if (!generation.configure(resources, &error))
    {
        std::cerr << error << '\n';
        return 7;
    }

    l2d::AssetManifestResource resolved;
    if (!resources.resolve(source.id, l2d::AssetSourceKind::Shader, resolved, &error))
    {
        std::cerr << error << '\n';
        return 8;
    }

    std::cout << "published " << generation.id << " at " << generation.root << '\n';
    std::cout << source.id << " -> " << resolved.path << '\n';
    return 0;
}

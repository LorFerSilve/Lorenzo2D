#include <Lorenzo2D/Assets/AssetFileCooker.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: Lorenzo2DPhase14AssetCookExample <project-root>\n";
        return 1;
    }

    const std::filesystem::path projectRoot = argv[1];
    l2d::AssetFileCooker fileCooker(projectRoot);

    l2d::AssetSourceDescriptor source;
    source.id = "textures/example/player";
    source.kind = l2d::AssetSourceKind::Texture;
    source.sourcePath = "assets/player.png";
    source.importer = std::string(l2d::AssetFileCooker::TextureImporter);

    l2d::AssetManifestEntry entry;
    std::string error;
    if (!fileCooker.makeManifestEntry(source, "cooked/player.png", entry, &error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    l2d::AssetManifest previous;
    l2d::AssetManifest current;
    if (!current.upsert(entry, &error))
    {
        std::cerr << error << '\n';
        return 3;
    }

    l2d::AssetCookCache cache;
    l2d::AssetCookExecutionResult result;
    if (!l2d::AssetCookExecutor::execute(previous, current, cache, 8u, fileCooker.cookFunction(),
                                         result, &error))
    {
        std::cerr << error << '\n';
        return 4;
    }

    std::cout << "cooked " << result.cooked.size() << " asset(s); " << result.remaining.size()
              << " remaining\n";
    return result.remaining.empty() ? 0 : 5;
}

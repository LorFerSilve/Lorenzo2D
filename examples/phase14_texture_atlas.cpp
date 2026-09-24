#include <Lorenzo2D/Assets/AssetTextureAtlas.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: Lorenzo2DPhase14TextureAtlasExample <project-root>\n";
        return 1;
    }

    const std::filesystem::path projectRoot = argv[1];

    l2d::AssetManifest sources;
    std::string error;

    l2d::AssetSourceDescriptor player;
    player.id = "textures/example/player";
    player.kind = l2d::AssetSourceKind::Texture;
    player.sourcePath = "assets/player.png";
    player.importer = "l2d.texture.copy";

    l2d::AssetSourceDescriptor enemy = player;
    enemy.id = "textures/example/enemy";
    enemy.sourcePath = "assets/enemy.png";

    // In a real content build these entries come from the preceding file-cook step.
    // The atlas cooker consumes their canonical cooked identities and paths.
    l2d::AssetCookRequest playerRequest;
    l2d::AssetCookRequest enemyRequest;
    if (!l2d::AssetManifest::makeCookRequest(player, 1u, "1", playerRequest, &error) ||
        !l2d::AssetManifest::makeCookRequest(enemy, 2u, "1", enemyRequest, &error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    l2d::AssetManifestEntry playerEntry{playerRequest.source,
                                        playerRequest.sourceContentHash,
                                        playerRequest.importerVersion,
                                        playerRequest.cookKey,
                                        "cooked/player.png"};
    l2d::AssetManifestEntry enemyEntry{enemyRequest.source,
                                       enemyRequest.sourceContentHash,
                                       enemyRequest.importerVersion,
                                       enemyRequest.cookKey,
                                       "cooked/enemy.png"};
    if (!sources.upsert(playerEntry, &error) || !sources.upsert(enemyEntry, &error))
    {
        std::cerr << error << '\n';
        return 3;
    }

    l2d::AssetTextureAtlasCooker atlasCooker(projectRoot, sources, 4096u, 1u);

    l2d::AssetManifestEntry atlasEntry;
    if (!atlasCooker.makeManifestEntry("atlases/example/characters",
                                        {"textures/example/player", "textures/example/enemy"},
                                        "cooked/characters.png", atlasEntry, &error))
    {
        std::cerr << error << '\n';
        return 4;
    }

    l2d::AssetManifest current = sources;
    if (!current.upsert(atlasEntry, &error))
    {
        std::cerr << error << '\n';
        return 5;
    }

    l2d::AssetCookCache cache;
    l2d::AssetCookExecutionResult result;
    if (!l2d::AssetCookExecutor::execute(sources, current, cache, 1u,
                                         atlasCooker.cookFunction(), result, &error))
    {
        std::cerr << error << '\n';
        return 6;
    }

    l2d::TextureAtlasMetadata metadata;
    if (!atlasCooker.readPublishedMetadata("cooked/characters.png", metadata, &error))
    {
        std::cerr << error << '\n';
        return 7;
    }

    std::cout << "atlas " << metadata.size().x << 'x' << metadata.size().y << " with "
              << metadata.regions().size() << " region(s)\n";
    return 0;
}

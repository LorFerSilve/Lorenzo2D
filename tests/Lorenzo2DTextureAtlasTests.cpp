#include <Lorenzo2D/Assets/AssetCookCache.hpp>
#include <Lorenzo2D/Assets/AssetFileCooker.hpp>
#include <Lorenzo2D/Assets/AssetTextureAtlas.hpp>

#include <SFML/Graphics/Image.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;
    using l2d::test::TemporaryFile;

    class TemporaryDirectory final
    {
      public:
        explicit TemporaryDirectory(const std::string& prefix) : m_seed(prefix)
        {
            m_path = m_seed.path();
            std::filesystem::create_directories(m_path);
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
        }

        const std::filesystem::path& path() const
        {
            return m_path;
        }

      private:
        TemporaryFile m_seed;
        std::filesystem::path m_path;
    };

    l2d::AssetSourceDescriptor texture(const std::string& id, const std::string& sourcePath)
    {
        l2d::AssetSourceDescriptor descriptor;
        descriptor.id = id;
        descriptor.kind = l2d::AssetSourceKind::Texture;
        descriptor.sourcePath = sourcePath;
        descriptor.importer = std::string(l2d::AssetFileCooker::TextureImporter);
        return descriptor;
    }

    void writeImage(const std::filesystem::path& path, const sf::Vector2u size,
                    const sf::Color first, const sf::Color second)
    {
        sf::Image image(size, first);
        image.setPixel({size.x - 1u, size.y - 1u}, second);
        L2D_REQUIRE(image.saveToFile(path));
    }

    l2d::AssetManifest cookSources(const std::filesystem::path& root)
    {
        std::filesystem::create_directories(root / "sources");
        writeImage(root / "sources/a.png", {2u, 1u}, sf::Color::Red, sf::Color::Blue);
        writeImage(root / "sources/b.png", {1u, 2u}, sf::Color::Green, sf::Color::Yellow);

        l2d::AssetFileCooker fileCooker(root);
        l2d::AssetManifest manifest;
        l2d::AssetManifestEntry a;
        l2d::AssetManifestEntry b;
        std::string error;
        L2D_REQUIRE(fileCooker.makeManifestEntry(texture("textures/a", "sources/a.png"),
                                                  "cooked/a.png", a, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(fileCooker.makeManifestEntry(texture("textures/b", "sources/b.png"),
                                                  "cooked/b.png", b, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(manifest.upsert(a, &error));
        L2D_REQUIRE(manifest.upsert(b, &error));

        l2d::AssetCookCache cache;
        l2d::AssetCookExecutionResult result;
        l2d::AssetManifest empty;
        L2D_REQUIRE(l2d::AssetCookExecutor::execute(empty, manifest, cache, 2u,
                                                     fileCooker.cookFunction(), result, &error));
        L2D_REQUIRE(result.cooked.size() == 2u);
        L2D_REQUIRE(result.remaining.empty());
        return manifest;
    }

    void testAtlasPackingAndMetadataAreDeterministic()
    {
        TemporaryDirectory directory("lorenzo2d_texture_atlas");
        auto sources = cookSources(directory.path());

        l2d::AssetTextureAtlasCooker cooker(directory.path(), sources, 4u, 1u);
        l2d::AssetManifestEntry first;
        l2d::AssetManifestEntry second;
        std::string error;

        L2D_REQUIRE(cooker.makeManifestEntry("atlases/main", {"textures/b", "textures/a"},
                                             "cooked/main.png", first, &error));
        L2D_REQUIRE(cooker.makeManifestEntry("atlases/main", {"textures/a", "textures/b"},
                                             "cooked/main.png", second, &error));
        L2D_REQUIRE(first.cookKey == second.cookKey);
        L2D_REQUIRE(first.source.dependencies == second.source.dependencies);
        L2D_REQUIRE(first.source.dependencies ==
                    std::vector<l2d::AssetId>({"textures/a", "textures/b"}));

        auto current = sources;
        L2D_REQUIRE(current.upsert(first, &error));

        l2d::AssetCookCache cache;
        l2d::AssetCookExecutionResult result;
        L2D_REQUIRE(l2d::AssetCookExecutor::execute(sources, current, cache, 1u,
                                                     cooker.cookFunction(), result, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(result.cooked == std::vector<l2d::AssetId>({"atlases/main"}));
        L2D_REQUIRE(result.remaining.empty());

        sf::Image atlas;
        L2D_REQUIRE(atlas.loadFromFile(directory.path() / "cooked/main.png"));
        L2D_REQUIRE(atlas.getSize() == sf::Vector2u(4u, 2u));
        L2D_REQUIRE(atlas.getPixel({0u, 0u}) == sf::Color::Red);
        L2D_REQUIRE(atlas.getPixel({1u, 0u}) == sf::Color::Blue);
        L2D_REQUIRE(atlas.getPixel({3u, 0u}) == sf::Color::Green);
        L2D_REQUIRE(atlas.getPixel({3u, 1u}) == sf::Color::Yellow);
        L2D_REQUIRE(atlas.getPixel({2u, 0u}) == sf::Color::Transparent);

        l2d::TextureAtlasMetadata metadata;
        L2D_REQUIRE(cooker.readPublishedMetadata("cooked/main.png", metadata, &error));
        L2D_REQUIRE(metadata.size() == sf::Vector2u(4u, 2u));
        L2D_REQUIRE(metadata.regions().size() == 2u);

        const auto* a = metadata.find("textures/a");
        const auto* b = metadata.find("textures/b");
        L2D_REQUIRE(a != nullptr);
        L2D_REQUIRE(b != nullptr);
        L2D_REQUIRE(*a == l2d::TextureAtlasRegion{"textures/a", 0u, 0u, 2u, 1u});
        L2D_REQUIRE(*b == l2d::TextureAtlasRegion{"textures/b", 3u, 0u, 1u, 2u});

        l2d::TextureAtlasMetadata roundTrip;
        L2D_REQUIRE(roundTrip.deserialize(metadata.serialize(), &error));
        L2D_REQUIRE(roundTrip.serialize() == metadata.serialize());

        l2d::AssetCookExecutionResult cached;
        L2D_REQUIRE(l2d::AssetCookExecutor::execute(sources, current, cache, 1u,
                                                     cooker.cookFunction(), cached, &error));
        L2D_REQUIRE(cached.cooked.empty());
        L2D_REQUIRE(cached.remaining.empty());
    }

    void testAtlasIdentityTracksDependencyChanges()
    {
        TemporaryDirectory directory("lorenzo2d_texture_atlas_identity");
        auto sources = cookSources(directory.path());

        l2d::AssetTextureAtlasCooker cooker(directory.path(), sources, 8u, 0u);
        l2d::AssetManifestEntry original;
        std::string error;
        L2D_REQUIRE(cooker.makeManifestEntry("atlases/main", {"textures/a", "textures/b"},
                                             "cooked/main.png", original, &error));

        auto changedSources = sources;
        auto changed = *changedSources.find("textures/a");
        ++changed.sourceContentHash;
        l2d::AssetCookRequest changedRequest;
        L2D_REQUIRE(l2d::AssetManifest::makeCookRequest(changed.source, changed.sourceContentHash,
                                                         changed.importerVersion, changedRequest,
                                                         &error));
        changed.cookKey = changedRequest.cookKey;
        L2D_REQUIRE(changedSources.upsert(changed, &error));

        l2d::AssetTextureAtlasCooker changedCooker(directory.path(), changedSources, 8u, 0u);
        l2d::AssetManifestEntry rebuilt;
        L2D_REQUIRE(changedCooker.makeManifestEntry("atlases/main",
                                                    {"textures/a", "textures/b"},
                                                    "cooked/main.png", rebuilt, &error));
        L2D_REQUIRE(rebuilt.sourceContentHash != original.sourceContentHash);
        L2D_REQUIRE(rebuilt.cookKey != original.cookKey);
    }

    void testAtlasRejectsInvalidInputsTransactionally()
    {
        TemporaryDirectory directory("lorenzo2d_texture_atlas_invalid");
        auto sources = cookSources(directory.path());
        l2d::AssetTextureAtlasCooker cooker(directory.path(), sources, 2u, 1u);

        l2d::AssetManifestEntry sentinel;
        sentinel.source.id = "sentinel";
        std::string error;

        L2D_REQUIRE(!cooker.makeManifestEntry("atlases/main", {}, "cooked/main.png", sentinel,
                                              &error));
        L2D_REQUIRE(sentinel.source.id == "sentinel");
        L2D_REQUIRE(!cooker.makeManifestEntry("atlases/main", {"textures/a", "textures/a"},
                                              "cooked/main.png", sentinel, &error));
        L2D_REQUIRE(!cooker.makeManifestEntry("atlases/main", {"textures/missing"},
                                              "cooked/main.png", sentinel, &error));
        L2D_REQUIRE(!cooker.makeManifestEntry("atlases/main", {"textures/a"},
                                              "cooked/main.jpg", sentinel, &error));

        l2d::AssetManifestEntry valid;
        L2D_REQUIRE(cooker.makeManifestEntry("atlases/main", {"textures/a", "textures/b"},
                                             "cooked/main.png", valid, &error));
        auto current = sources;
        L2D_REQUIRE(current.upsert(valid, &error));

        l2d::AssetCookCache cache;
        l2d::AssetCookExecutionResult result = {{"sentinel"}, {"sentinel"}};
        L2D_REQUIRE(!l2d::AssetCookExecutor::execute(sources, current, cache, 1u,
                                                      cooker.cookFunction(), result, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(result.cooked == std::vector<l2d::AssetId>({"sentinel"}));
        L2D_REQUIRE(!std::filesystem::exists(directory.path() / "cooked/main.png"));
        L2D_REQUIRE(!std::filesystem::exists(directory.path() / "cooked/main.png.regions"));
    }
}

int main()
{
    int failures = 0;
    runTest("texture atlas packing and metadata are deterministic",
            testAtlasPackingAndMetadataAreDeterministic, failures);
    runTest("texture atlas identity tracks dependency changes",
            testAtlasIdentityTracksDependencyChanges, failures);
    runTest("texture atlas rejects invalid inputs transactionally",
            testAtlasRejectsInvalidInputsTransactionally, failures);

    if (failures != 0)
    {
        std::cerr << failures << " texture atlas test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D texture atlas tests passed.\n";
    return 0;
}

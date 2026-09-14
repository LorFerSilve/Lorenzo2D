#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Assets/AssetManifest.hpp>
#include <Lorenzo2D/Assets/AssetMetadata.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <filesystem>
#include <fstream>
#include <string>

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

    l2d::AssetSourceDescriptor textureDescriptor()
    {
        l2d::AssetSourceDescriptor descriptor;
        descriptor.id = "textures/player/body";
        descriptor.kind = l2d::AssetSourceKind::Texture;
        descriptor.sourcePath = "textures/player/./body.png";
        descriptor.importer = "sfml-texture";
        descriptor.importSettings = {{"filter", "nearest"}, {"smooth", "false"}};
        descriptor.dependencies = {"materials/player", "atlases/characters"};
        return descriptor;
    }

    l2d::AssetManifestEntry manifestEntry(l2d::AssetSourceDescriptor descriptor,
                                          const std::uint64_t sourceHash,
                                          const std::string& importerVersion,
                                          const std::filesystem::path& cookedPath)
    {
        l2d::AssetCookRequest request;
        L2D_REQUIRE(l2d::AssetManifest::makeCookRequest(
            descriptor, sourceHash, importerVersion, request));

        l2d::AssetManifestEntry entry;
        entry.source = request.source;
        entry.sourceContentHash = request.sourceContentHash;
        entry.importerVersion = request.importerVersion;
        entry.cookKey = request.cookKey;
        entry.cookedPath = cookedPath;
        return entry;
    }

    void testResourceLocatorUsesOrderedRuntimeRoots()
    {
        TemporaryDirectory first("lorenzo2d_resources_first");
        TemporaryDirectory second("lorenzo2d_resources_second");
        std::filesystem::create_directories(first.path() / "levels");
        std::filesystem::create_directories(second.path() / "levels");

        {
            std::ofstream file(first.path() / "levels" / "shared.txt");
            file << "first";
        }
        {
            std::ofstream file(second.path() / "levels" / "shared.txt");
            file << "second";
        }
        {
            std::ofstream file(second.path() / "levels" / "only-second.txt");
            file << "second";
        }

        l2d::ResourceLocator locator;
        L2D_REQUIRE(locator.addRoot(first.path()));
        L2D_REQUIRE(locator.addRoot(first.path()));
        L2D_REQUIRE(locator.addRoot(second.path()));
        L2D_REQUIRE(locator.roots().size() == 2u);

        const auto shared = locator.locate("levels/shared.txt");
        L2D_REQUIRE(shared.has_value());
        L2D_REQUIRE(shared->parent_path().parent_path() == first.path());
        L2D_REQUIRE(locator.contains("levels/only-second.txt"));
        L2D_REQUIRE(!locator.contains("levels/missing.txt"));

        const auto absolute = locator.locate(second.path() / "levels" / "only-second.txt");
        L2D_REQUIRE(absolute.has_value());
        L2D_REQUIRE(*absolute == (second.path() / "levels" / "only-second.txt").lexically_normal());

        L2D_REQUIRE(locator.removeRoot(first.path()));
        L2D_REQUIRE(!locator.removeRoot(first.path()));
        const auto nowSecond = locator.locate("levels/shared.txt");
        L2D_REQUIRE(nowSecond.has_value());
        L2D_REQUIRE(nowSecond->parent_path().parent_path() == second.path());

        locator.clear();
        L2D_REQUIRE(locator.roots().empty());
        L2D_REQUIRE(!locator.locate("levels/shared.txt"));
    }

    void testLocatorIntegratesWithTransactionalAssetLoads()
    {
        TemporaryDirectory root("lorenzo2d_resource_assets");
        l2d::ResourceLocator locator;
        l2d::AssetManager assets;

        L2D_REQUIRE(locator.addRoot(root.path()));
        L2D_REQUIRE(!assets.loadTexture("missing", locator, "textures/missing.png"));
        L2D_REQUIRE(!assets.loadFont("missing", locator, "fonts/missing.ttf"));
        L2D_REQUIRE(!assets.hasTexture("missing"));
        L2D_REQUIRE(!assets.hasFont("missing"));
    }

    void testExecutableDirectoryHasSafeFallback()
    {
        const std::filesystem::path current = std::filesystem::current_path().lexically_normal();
        L2D_REQUIRE(l2d::ResourceLocator::executableDirectory({}) == current);

        const std::filesystem::path executable = current / "bin" / "game";
        L2D_REQUIRE(l2d::ResourceLocator::executableDirectory(executable) == current / "bin");
    }

    void testAssetMetadataValidationAndStableOrdering()
    {
        L2D_REQUIRE(l2d::AssetMetadataRegistry::isValidAssetId("textures/player.body-v2"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidAssetId("player body"));
        L2D_REQUIRE(l2d::AssetMetadataRegistry::isValidSourcePath("textures/player.png"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidSourcePath("../player.png"));

        l2d::AssetMetadataRegistry registry;
        auto zeta = textureDescriptor();
        zeta.id = "textures/zeta";
        auto alpha = textureDescriptor();
        alpha.id = "textures/alpha";

        L2D_REQUIRE(registry.upsert(zeta));
        L2D_REQUIRE(registry.upsert(alpha));
        L2D_REQUIRE(registry.size() == 2u);

        const auto descriptors = registry.descriptors();
        L2D_REQUIRE(descriptors.size() == 2u);
        L2D_REQUIRE(descriptors[0].id == "textures/alpha");
        L2D_REQUIRE(descriptors[1].id == "textures/zeta");
        L2D_REQUIRE(descriptors[0].sourcePath == std::filesystem::path("textures/player/body.png"));
        L2D_REQUIRE(descriptors[0].dependencies[0] == "atlases/characters");
        L2D_REQUIRE(descriptors[0].dependencies[1] == "materials/player");
    }

    void testAssetMetadataRejectsInvalidReplacementTransactionally()
    {
        l2d::AssetMetadataRegistry registry;
        auto descriptor = textureDescriptor();
        L2D_REQUIRE(registry.upsert(descriptor));

        std::string error;
        auto invalid = descriptor;
        invalid.sourcePath = "../outside-project.png";
        invalid.importer = "broken";
        L2D_REQUIRE(!registry.upsert(invalid, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(registry.size() == 1u);
        L2D_REQUIRE(registry.find(descriptor.id) != nullptr);
        L2D_REQUIRE(registry.find(descriptor.id)->importer == "sfml-texture");

        invalid = descriptor;
        invalid.dependencies = {descriptor.id};
        L2D_REQUIRE(!registry.upsert(invalid, &error));

        invalid = descriptor;
        invalid.dependencies = {"materials/player", "materials/player"};
        L2D_REQUIRE(!registry.upsert(invalid, &error));
    }

    void testCookIdentityIsCanonicalAndSensitiveToInputs()
    {
        const auto descriptor = textureDescriptor();
        auto reordered = descriptor;
        reordered.dependencies = {"atlases/characters", "materials/player"};

        l2d::AssetCookRequest first;
        l2d::AssetCookRequest second;
        L2D_REQUIRE(l2d::AssetManifest::makeCookRequest(descriptor, 0x1234u, "1.0", first));
        L2D_REQUIRE(l2d::AssetManifest::makeCookRequest(reordered, 0x1234u, "1.0", second));
        L2D_REQUIRE(first.cookKey == second.cookKey);
        L2D_REQUIRE(first.source.sourcePath == std::filesystem::path("textures/player/body.png"));
        L2D_REQUIRE(first.source.dependencies[0] == "atlases/characters");

        l2d::AssetCookRequest changedHash;
        l2d::AssetCookRequest changedVersion;
        L2D_REQUIRE(
            l2d::AssetManifest::makeCookRequest(descriptor, 0x1235u, "1.0", changedHash));
        L2D_REQUIRE(
            l2d::AssetManifest::makeCookRequest(descriptor, 0x1234u, "1.1", changedVersion));
        L2D_REQUIRE(first.cookKey != changedHash.cookKey);
        L2D_REQUIRE(first.cookKey != changedVersion.cookKey);
    }

    void testAssetManifestRoundTripsDeterministically()
    {
        auto alpha = textureDescriptor();
        alpha.id = "textures/alpha";
        auto zeta = textureDescriptor();
        zeta.id = "textures/zeta";

        l2d::AssetManifest manifest;
        L2D_REQUIRE(manifest.upsert(
            manifestEntry(zeta, 20u, "texture-importer-v2", "cooked/zeta.texture")));
        L2D_REQUIRE(manifest.upsert(
            manifestEntry(alpha, 10u, "texture-importer-v2", "cooked/alpha.texture")));

        const std::string serialized = manifest.serialize();
        l2d::AssetManifest restored;
        std::string error;
        L2D_REQUIRE(restored.deserialize(serialized, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(restored.serialize() == serialized);

        const auto entries = restored.entries();
        L2D_REQUIRE(entries.size() == 2u);
        L2D_REQUIRE(entries[0].source.id == "textures/alpha");
        L2D_REQUIRE(entries[1].source.id == "textures/zeta");
        L2D_REQUIRE(entries[0].cookedPath == std::filesystem::path("cooked/alpha.texture"));
    }

    void testAssetManifestRejectsTamperingTransactionally()
    {
        auto descriptor = textureDescriptor();
        descriptor.id = "textures/retained";
        const auto retained = manifestEntry(descriptor, 77u, "1.0", "cooked/retained.texture");

        l2d::AssetManifest manifest;
        L2D_REQUIRE(manifest.upsert(retained));
        const std::string original = manifest.serialize();

        auto invalidEntry = retained;
        ++invalidEntry.cookKey;
        L2D_REQUIRE(!manifest.upsert(invalidEntry));
        L2D_REQUIRE(manifest.serialize() == original);

        std::string malformed = original;
        malformed.push_back('x');
        std::string error;
        L2D_REQUIRE(!manifest.deserialize(malformed, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(manifest.serialize() == original);

        L2D_REQUIRE(!manifest.deserialize("3:bad", &error));
        L2D_REQUIRE(manifest.serialize() == original);
    }
}

int main()
{
    int failures = 0;
    runTest("resource locator uses ordered runtime roots",
            testResourceLocatorUsesOrderedRuntimeRoots, failures);
    runTest("resource locator integrates with asset loads",
            testLocatorIntegratesWithTransactionalAssetLoads, failures);
    runTest("resource locator resolves executable directory",
            testExecutableDirectoryHasSafeFallback, failures);
    runTest("asset metadata validates stable deterministic descriptors",
            testAssetMetadataValidationAndStableOrdering, failures);
    runTest("asset metadata rejects invalid replacements transactionally",
            testAssetMetadataRejectsInvalidReplacementTransactionally, failures);
    runTest("asset cook identity is canonical and input-sensitive",
            testCookIdentityIsCanonicalAndSensitiveToInputs, failures);
    runTest("asset manifest round-trips deterministically",
            testAssetManifestRoundTripsDeterministically, failures);
    runTest("asset manifest rejects tampering transactionally",
            testAssetManifestRejectsTamperingTransactionally, failures);

    if (failures != 0)
    {
        std::cerr << failures << " resource test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D resource tests passed.\n";
    return 0;
}

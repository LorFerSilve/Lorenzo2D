#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

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

    void testStableIdentityAndValidation()
    {
        L2D_REQUIRE(l2d::AssetMetadataRegistry::isValidAssetId("textures/player.body-v2"));
        L2D_REQUIRE(l2d::AssetMetadataRegistry::isValidAssetId("namespace:player/body"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidAssetId(""));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidAssetId("player body"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidAssetId("player\\body"));

        L2D_REQUIRE(l2d::AssetMetadataRegistry::isValidSourcePath("textures/player.png"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidSourcePath("../player.png"));
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::isValidSourcePath({}));

        std::string error;
        auto descriptor = textureDescriptor();
        L2D_REQUIRE(l2d::AssetMetadataRegistry::validate(descriptor, &error));
        L2D_REQUIRE(error.empty());

        descriptor.kind = l2d::AssetSourceKind::Unknown;
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::validate(descriptor, &error));
        L2D_REQUIRE(!error.empty());
    }

    void testRegistryIsTransactionalAndCanonicalizesMetadata()
    {
        l2d::AssetMetadataRegistry registry;
        std::string error;

        auto descriptor = textureDescriptor();
        L2D_REQUIRE(registry.upsert(descriptor, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(registry.size() == 1u);

        const auto* stored = registry.find(descriptor.id);
        L2D_REQUIRE(stored != nullptr);
        L2D_REQUIRE(stored->sourcePath == std::filesystem::path("textures/player/body.png"));
        L2D_REQUIRE(stored->dependencies.size() == 2u);
        L2D_REQUIRE(stored->dependencies[0] == "atlases/characters");
        L2D_REQUIRE(stored->dependencies[1] == "materials/player");

        auto invalidReplacement = descriptor;
        invalidReplacement.sourcePath = "../outside-project.png";
        invalidReplacement.importer = "broken";
        L2D_REQUIRE(!registry.upsert(invalidReplacement, &error));
        L2D_REQUIRE(registry.size() == 1u);
        stored = registry.find(descriptor.id);
        L2D_REQUIRE(stored != nullptr);
        L2D_REQUIRE(stored->importer == "sfml-texture");

        auto validReplacement = descriptor;
        validReplacement.importer = "custom-texture";
        validReplacement.dependencies = {"materials/player"};
        L2D_REQUIRE(registry.upsert(validReplacement, &error));
        L2D_REQUIRE(registry.size() == 1u);
        L2D_REQUIRE(registry.find(descriptor.id)->importer == "custom-texture");
    }

    void testDependencyValidationRejectsAmbiguousDescriptors()
    {
        std::string error;
        auto descriptor = textureDescriptor();

        descriptor.dependencies = {descriptor.id};
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::validate(descriptor, &error));

        descriptor = textureDescriptor();
        descriptor.dependencies = {"materials/player", "materials/player"};
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::validate(descriptor, &error));

        descriptor = textureDescriptor();
        descriptor.dependencies = {"invalid dependency"};
        L2D_REQUIRE(!l2d::AssetMetadataRegistry::validate(descriptor, &error));
    }

    void testSnapshotsHaveDeterministicIdentityOrdering()
    {
        l2d::AssetMetadataRegistry registry;

        auto zeta = textureDescriptor();
        zeta.id = "textures/zeta";
        auto alpha = textureDescriptor();
        alpha.id = "textures/alpha";
        auto middle = textureDescriptor();
        middle.id = "textures/middle";

        L2D_REQUIRE(registry.upsert(zeta));
        L2D_REQUIRE(registry.upsert(alpha));
        L2D_REQUIRE(registry.upsert(middle));

        const std::vector<l2d::AssetSourceDescriptor> snapshot = registry.descriptors();
        L2D_REQUIRE(snapshot.size() == 3u);
        L2D_REQUIRE(snapshot[0].id == "textures/alpha");
        L2D_REQUIRE(snapshot[1].id == "textures/middle");
        L2D_REQUIRE(snapshot[2].id == "textures/zeta");

        L2D_REQUIRE(registry.erase("textures/middle"));
        L2D_REQUIRE(!registry.erase("textures/middle"));
        L2D_REQUIRE(!registry.contains("textures/middle"));
        registry.clear();
        L2D_REQUIRE(registry.size() == 0u);
    }
}

int main()
{
    int failures = 0;
    failures += runTest("asset metadata stable identity and validation", testStableIdentityAndValidation);
    failures += runTest("asset metadata transactional registry", testRegistryIsTransactionalAndCanonicalizesMetadata);
    failures += runTest("asset metadata dependency validation", testDependencyValidationRejectsAmbiguousDescriptors);
    failures += runTest("asset metadata deterministic snapshots", testSnapshotsHaveDeterministicIdentityOrdering);
    return failures == 0 ? 0 : 1;
}

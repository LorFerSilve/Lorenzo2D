#include <Lorenzo2D/Assets/AssetManifest.hpp>
#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <filesystem>
#include <string>

int main()
{
    l2d::AssetSourceDescriptor descriptor;
    descriptor.id = "textures/consumer/example";
    descriptor.kind = l2d::AssetSourceKind::Texture;
    descriptor.sourcePath = "textures/example.png";
    descriptor.importer = "consumer-test";
    descriptor.importSettings = {{"smooth", "false"}};

    l2d::AssetMetadataRegistry registry;
    if (!registry.upsert(descriptor))
    {
        return 1;
    }

    const auto* stored = registry.find(descriptor.id);
    if (stored == nullptr || stored->sourcePath != std::filesystem::path("textures/example.png"))
    {
        return 2;
    }

    l2d::AssetCookRequest request;
    if (!l2d::AssetManifest::makeCookRequest(descriptor, 42u, "consumer-v1", request))
    {
        return 3;
    }

    l2d::AssetManifestEntry entry;
    entry.source = request.source;
    entry.sourceContentHash = request.sourceContentHash;
    entry.importerVersion = request.importerVersion;
    entry.cookKey = request.cookKey;
    entry.cookedPath = "cooked/example.texture";

    l2d::AssetManifest manifest;
    if (!manifest.upsert(entry))
    {
        return 4;
    }

    const std::string serialized = manifest.serialize();
    l2d::AssetManifest restored;
    if (!restored.deserialize(serialized) || restored.serialize() != serialized)
    {
        return 5;
    }

    const auto snapshot = registry.descriptors();
    return snapshot.size() == 1u && snapshot.front().id == descriptor.id &&
                   restored.contains(descriptor.id)
               ? 0
               : 6;
}

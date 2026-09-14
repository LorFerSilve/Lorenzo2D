#include <Lorenzo2D/Assets/AssetMetadata.hpp>

#include <filesystem>

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

    const auto snapshot = registry.descriptors();
    return snapshot.size() == 1u && snapshot.front().id == descriptor.id ? 0 : 3;
}

#include <Lorenzo2D/Assets/AssetTextureAtlas.hpp>

#include <filesystem>
#include <string>
#include <vector>

int main()
{
    l2d::TextureAtlasMetadata metadata;
    std::string error;
    if (metadata.deserialize(
            "L2D-TEXTURE-ATLAS\n1\n4 2\n2\ntextures/a\t0\t0\t2\t1\ntextures/b\t3\t0\t1\t2\n",
            &error) == false)
    {
        return 1;
    }

    if (metadata.size() != sf::Vector2u(4u, 2u) || metadata.regions().size() != 2u)
    {
        return 2;
    }

    const auto* region = metadata.find("textures/b");
    if (region == nullptr || region->x != 3u || region->height != 2u)
    {
        return 3;
    }

    const auto sidecar = l2d::AssetTextureAtlasCooker::metadataPath("cooked/main.png");
    if (sidecar != std::filesystem::path("cooked/main.png.regions"))
    {
        return 4;
    }

    return 0;
}

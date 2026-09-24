#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Assets/AssetManifestResourceLocator.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: Lorenzo2DPhase14ManifestRuntimeExample "
                     "<content-root> <manifest-file> <texture-asset-id>\n";
        return 1;
    }

    const std::filesystem::path contentRoot = argv[1];
    std::ifstream stream(argv[2], std::ios::binary);
    if (!stream)
    {
        std::cerr << "unable to open manifest\n";
        return 2;
    }

    const std::string document{std::istreambuf_iterator<char>(stream),
                               std::istreambuf_iterator<char>()};

    l2d::AssetManifest manifest;
    std::string error;
    if (!manifest.deserialize(document, &error))
    {
        std::cerr << error << '\n';
        return 3;
    }

    l2d::AssetManifestResourceLocator resources(std::move(manifest));
    if (!resources.addRoot(contentRoot, &error))
    {
        std::cerr << error << '\n';
        return 4;
    }

    const l2d::AssetId textureId = argv[3];
    l2d::AssetManager assets;
    if (!assets.loadTexture(resources, textureId))
    {
        std::cerr << "unable to resolve/load texture asset '" << textureId << "'\n";
        return 5;
    }

    const auto texture = assets.getTexture(textureId);
    std::cout << "loaded " << textureId << " (" << texture->getSize().x << 'x'
              << texture->getSize().y << ")\n";
    return 0;
}

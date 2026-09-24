#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Assets/AssetManifestResourceLocator.hpp>

#include <SFML/Graphics/Image.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

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

    l2d::AssetManifestEntry makeEntry(const l2d::AssetId& id, const l2d::AssetSourceKind kind,
                                      const std::filesystem::path& cookedPath,
                                      const std::uint64_t sourceHash)
    {
        l2d::AssetSourceDescriptor source;
        source.id = id;
        source.kind = kind;
        source.sourcePath = std::filesystem::path("sources") / (id + ".source");
        source.importer = "runtime-manifest-test";

        l2d::AssetCookRequest request;
        L2D_REQUIRE(l2d::AssetManifest::makeCookRequest(source, sourceHash, "1", request));

        l2d::AssetManifestEntry entry;
        entry.source = request.source;
        entry.sourceContentHash = request.sourceContentHash;
        entry.importerVersion = request.importerVersion;
        entry.cookKey = request.cookKey;
        entry.cookedPath = cookedPath;
        return entry;
    }

    void writeImage(const std::filesystem::path& path, const sf::Color color)
    {
        std::filesystem::create_directories(path.parent_path());
        const sf::Image image({2u, 2u}, color);
        L2D_REQUIRE(image.saveToFile(path));
    }

    void writeText(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        L2D_REQUIRE(static_cast<bool>(stream));
        stream << text;
        stream.close();
        L2D_REQUIRE(static_cast<bool>(stream));
    }

    void testManifestLookupUsesStableIdsAndOrderedRoots()
    {
        TemporaryDirectory first("lorenzo2d_manifest_runtime_first");
        TemporaryDirectory second("lorenzo2d_manifest_runtime_second");
        writeImage(first.path() / "cooked/player.png", sf::Color::Red);
        writeImage(second.path() / "cooked/player.png", sf::Color::Green);
        writeText(second.path() / "cooked/effect.frag", "void main() {}\n");

        l2d::AssetManifest manifest;
        const auto texture =
            makeEntry("textures/player", l2d::AssetSourceKind::Texture, "cooked/player.png", 1u);
        const auto shader =
            makeEntry("shaders/effect", l2d::AssetSourceKind::Shader, "cooked/effect.frag", 2u);
        L2D_REQUIRE(manifest.upsert(texture));
        L2D_REQUIRE(manifest.upsert(shader));

        l2d::AssetManifestResourceLocator locator(manifest);
        std::string error;
        L2D_REQUIRE(locator.addRoot(first.path(), &error));
        L2D_REQUIRE(locator.addRoot(second.path(), &error));
        L2D_REQUIRE(error.empty());

        l2d::AssetManifestResource resolved;
        L2D_REQUIRE(
            locator.resolve("textures/player", l2d::AssetSourceKind::Texture, resolved, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(resolved.id == "textures/player");
        L2D_REQUIRE(resolved.kind == l2d::AssetSourceKind::Texture);
        L2D_REQUIRE(resolved.path == (first.path() / "cooked/player.png").lexically_normal());
        L2D_REQUIRE(resolved.cookKey == texture.cookKey);

        l2d::AssetManifestResource preserved;
        preserved.id = "sentinel";
        L2D_REQUIRE(
            !locator.resolve("textures/player", l2d::AssetSourceKind::Font, preserved, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(preserved.id == "sentinel");
        L2D_REQUIRE(!locator.resolve("invalid id", preserved, &error));
        L2D_REQUIRE(preserved.id == "sentinel");

        L2D_REQUIRE(locator.contains("shaders/effect"));
        L2D_REQUIRE(locator.contains("shaders/effect", l2d::AssetSourceKind::Shader));
        L2D_REQUIRE(!locator.contains("shaders/effect", l2d::AssetSourceKind::Texture));

        L2D_REQUIRE(locator.removeRoot(first.path()));
        const auto fallback = locator.locate("textures/player", l2d::AssetSourceKind::Texture);
        L2D_REQUIRE(fallback.has_value());
        L2D_REQUIRE(*fallback == (second.path() / "cooked/player.png").lexically_normal());

        l2d::AssetManifest replacement;
        L2D_REQUIRE(replacement.upsert(shader));
        locator.setManifest(replacement);
        L2D_REQUIRE(!locator.contains("textures/player"));
        L2D_REQUIRE(locator.contains("shaders/effect"));
    }

    void testAssetManagerLoadsManifestTexturesTransactionally()
    {
        TemporaryDirectory root("lorenzo2d_manifest_runtime_assets");
        writeImage(root.path() / "cooked/player.png", sf::Color::Magenta);

        l2d::AssetManifest manifest;
        const auto texture =
            makeEntry("textures/player", l2d::AssetSourceKind::Texture, "cooked/player.png", 11u);
        const auto wrongKind =
            makeEntry("fonts/not-a-texture", l2d::AssetSourceKind::Font, "cooked/player.png", 12u);
        L2D_REQUIRE(manifest.upsert(texture));
        L2D_REQUIRE(manifest.upsert(wrongKind));

        l2d::AssetManifestResourceLocator locator(manifest);
        L2D_REQUIRE(locator.addRoot(root.path()));

        l2d::AssetManager assets;
        L2D_REQUIRE(assets.loadTexture(locator, "textures/player", false));
        L2D_REQUIRE(assets.hasTexture("textures/player"));
        const auto first = assets.getTexture("textures/player");
        L2D_REQUIRE(first);
        L2D_REQUIRE(first->getSize() == sf::Vector2u(2u, 2u));
        L2D_REQUIRE(!first->isSmooth());

        L2D_REQUIRE(!assets.loadTexture(locator, "fonts/not-a-texture"));
        L2D_REQUIRE(!assets.hasTexture("fonts/not-a-texture"));
        L2D_REQUIRE(!assets.loadFont(locator, "textures/player"));
        L2D_REQUIRE(!assets.loadSoundBuffer(locator, "textures/player"));

        l2d::AssetManifest missingArtifact;
        auto missingTexture = texture;
        missingTexture.cookedPath = "cooked/missing.png";
        L2D_REQUIRE(missingArtifact.upsert(missingTexture));
        locator.setManifest(missingArtifact);

        L2D_REQUIRE(!assets.loadTexture(locator, "textures/player"));
        const auto preserved = assets.getTexture("textures/player");
        L2D_REQUIRE(preserved);
        L2D_REQUIRE(preserved->getSize() == sf::Vector2u(2u, 2u));
        L2D_REQUIRE(assets.textureCount() == 1u);
    }

    void testManifestRuntimeRootsAreBounded()
    {
        TemporaryDirectory root("lorenzo2d_manifest_runtime_roots");
        l2d::AssetManifestResourceLocator locator;
        std::string error;

        for (std::size_t index = 0; index < l2d::AssetManifestResourceLocator::MaxRoots; ++index)
        {
            const auto child = root.path() / std::to_string(index);
            std::filesystem::create_directories(child);
            L2D_REQUIRE(locator.addRoot(child, &error));
            L2D_REQUIRE(error.empty());
        }

        const auto overflow = root.path() / "overflow";
        std::filesystem::create_directories(overflow);
        L2D_REQUIRE(!locator.addRoot(overflow, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(locator.roots().size() == l2d::AssetManifestResourceLocator::MaxRoots);
        L2D_REQUIRE(locator.addRoot(root.path() / "0", &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(locator.roots().size() == l2d::AssetManifestResourceLocator::MaxRoots);

        locator.clearRoots();
        L2D_REQUIRE(locator.roots().empty());
    }
}

int main()
{
    int failures = 0;
    runTest("manifest lookup uses stable ids and ordered roots",
            testManifestLookupUsesStableIdsAndOrderedRoots, failures);
    runTest("asset manager loads manifest textures transactionally",
            testAssetManagerLoadsManifestTexturesTransactionally, failures);
    runTest("manifest runtime roots are bounded", testManifestRuntimeRootsAreBounded, failures);

    if (failures != 0)
    {
        std::cerr << failures << " manifest runtime resource test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D manifest runtime resource tests passed.\n";
    return 0;
}

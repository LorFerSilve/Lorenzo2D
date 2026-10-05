#include <Lorenzo2D/Assets/AssetGeneration.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
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

    l2d::AssetManifestEntry makeEntry(const l2d::AssetId& id,
                                      const std::filesystem::path& cookedPath,
                                      const std::uint64_t sourceHash,
                                      std::vector<l2d::AssetId> dependencies = {})
    {
        l2d::AssetSourceDescriptor source;
        source.id = id;
        source.kind = l2d::AssetSourceKind::Shader;
        source.sourcePath = std::filesystem::path("sources") / (id + ".frag");
        source.importer = "generation-test";
        source.dependencies = std::move(dependencies);

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

    void writeText(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        L2D_REQUIRE(static_cast<bool>(stream));
        stream << text;
        stream.close();
        L2D_REQUIRE(static_cast<bool>(stream));
    }

    std::string readText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        L2D_REQUIRE(static_cast<bool>(stream));
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    l2d::AssetManifest makePreviousManifest()
    {
        l2d::AssetManifest manifest;
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/a", "cooked/a.frag", 1u)));
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/b", "cooked/b.frag", 2u, {"assets/a"})));
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/c", "cooked/c.frag", 3u)));
        return manifest;
    }

    l2d::AssetManifest makeCurrentManifest()
    {
        l2d::AssetManifest manifest;
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/a", "cooked/a.frag", 11u)));
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/b", "cooked/b.frag", 2u, {"assets/a"})));
        L2D_REQUIRE(manifest.upsert(makeEntry("assets/c", "cooked/c.frag", 3u)));
        return manifest;
    }

    void testIncrementalGenerationStaysHiddenUntilComplete()
    {
        TemporaryDirectory directory("lorenzo2d_asset_generation");
        l2d::AssetGenerationPublisher publisher(directory.path() / "published");

        const auto previousManifest = makePreviousManifest();
        l2d::AssetGenerationStaging previousStaging;
        std::string error;
        L2D_REQUIRE(publisher.prepare(previousManifest, previousStaging, &error));
        L2D_REQUIRE(error.empty());

        writeText(previousStaging.root / "cooked/a.frag", "old-a\n");
        writeText(previousStaging.root / "cooked/b.frag", "old-b\n");
        writeText(previousStaging.root / "cooked/c.frag", "stable-c\n");

        l2d::AssetContentGeneration previous;
        L2D_REQUIRE(publisher.publish(previousStaging, previous, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(std::filesystem::is_directory(previous.root));
        L2D_REQUIRE(readText(previous.root / "cooked/a.frag") == "old-a\n");

        l2d::AssetManifestResourceLocator runtime;
        L2D_REQUIRE(previous.configure(runtime, &error));
        const auto oldA = runtime.locate("assets/a", l2d::AssetSourceKind::Shader);
        L2D_REQUIRE(oldA.has_value());
        L2D_REQUIRE(*oldA == previous.root / "cooked/a.frag");

        const auto currentManifest = makeCurrentManifest();
        L2D_REQUIRE(l2d::AssetGenerationPublisher::generationId(currentManifest) != previous.id);

        l2d::AssetGenerationStaging currentStaging;
        L2D_REQUIRE(publisher.prepare(currentManifest, &previous, currentStaging, &error));
        L2D_REQUIRE(error.empty());

        L2D_REQUIRE(!std::filesystem::exists(currentStaging.root / "cooked/a.frag"));
        L2D_REQUIRE(!std::filesystem::exists(currentStaging.root / "cooked/b.frag"));
        L2D_REQUIRE(std::filesystem::is_regular_file(currentStaging.root / "cooked/c.frag"));
        L2D_REQUIRE(readText(currentStaging.root / "cooked/c.frag") == "stable-c\n");
        L2D_REQUIRE(std::filesystem::is_regular_file(
            currentStaging.root /
            std::filesystem::path(std::string(l2d::AssetGenerationPublisher::ManifestFilename))));

        writeText(currentStaging.root / "cooked/a.frag", "new-a\n");

        l2d::AssetContentGeneration incomplete;
        incomplete.id = "sentinel";
        L2D_REQUIRE(!publisher.publish(currentStaging, incomplete, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(incomplete.id == "sentinel");
        L2D_REQUIRE(std::filesystem::is_directory(currentStaging.root));
        L2D_REQUIRE(std::filesystem::is_directory(previous.root));

        const auto stillOld = runtime.locate("assets/a", l2d::AssetSourceKind::Shader);
        L2D_REQUIRE(stillOld.has_value());
        L2D_REQUIRE(*stillOld == previous.root / "cooked/a.frag");
        L2D_REQUIRE(readText(*stillOld) == "old-a\n");

        writeText(currentStaging.root / "cooked/b.frag", "new-b\n");

        l2d::AssetContentGeneration current;
        L2D_REQUIRE(publisher.publish(currentStaging, current, &error));
        L2D_REQUIRE(error.empty());
        L2D_REQUIRE(!std::filesystem::exists(currentStaging.root));
        L2D_REQUIRE(std::filesystem::is_directory(current.root));
        L2D_REQUIRE(std::filesystem::is_directory(previous.root));

        const auto beforeSwap = runtime.locate("assets/a", l2d::AssetSourceKind::Shader);
        L2D_REQUIRE(beforeSwap.has_value());
        L2D_REQUIRE(*beforeSwap == previous.root / "cooked/a.frag");

        L2D_REQUIRE(current.configure(runtime, &error));
        L2D_REQUIRE(error.empty());
        const auto afterSwap = runtime.locate("assets/a", l2d::AssetSourceKind::Shader);
        L2D_REQUIRE(afterSwap.has_value());
        L2D_REQUIRE(*afterSwap == current.root / "cooked/a.frag");
        L2D_REQUIRE(readText(*afterSwap) == "new-a\n");
        L2D_REQUIRE(readText(current.root / "cooked/c.frag") == "stable-c\n");
    }

    void testPrepareRejectsInvalidGraphsTransactionally()
    {
        TemporaryDirectory directory("lorenzo2d_asset_generation_invalid");
        l2d::AssetGenerationPublisher publisher(directory.path() / "published");

        l2d::AssetManifest invalid;
        L2D_REQUIRE(
            invalid.upsert(makeEntry("assets/orphan", "cooked/orphan.frag", 7u, {"missing"})));

        l2d::AssetGenerationStaging output;
        output.id = "sentinel";
        std::string error;
        L2D_REQUIRE(!publisher.prepare(invalid, output, &error));
        L2D_REQUIRE(!error.empty());
        L2D_REQUIRE(output.id == "sentinel");
        L2D_REQUIRE(!std::filesystem::exists(directory.path() / "published"));
    }

    void testPublicationMetadataPathIsReserved()
    {
        TemporaryDirectory directory("lorenzo2d_asset_generation_reserved");
        l2d::AssetGenerationPublisher publisher(directory.path() / "published");

        l2d::AssetManifest invalid;
        const auto reserved =
            std::filesystem::path(std::string(l2d::AssetGenerationPublisher::ManifestFilename));
        L2D_REQUIRE(invalid.upsert(makeEntry("assets/metadata", reserved, 9u)));

        l2d::AssetGenerationStaging output;
        std::string error;
        L2D_REQUIRE(!publisher.prepare(invalid, output, &error));
        L2D_REQUIRE(!error.empty());
    }
}

int main()
{
    int failures = 0;
    runTest("incremental generation stays hidden until complete",
            testIncrementalGenerationStaysHiddenUntilComplete, failures);
    runTest("prepare rejects invalid graphs transactionally",
            testPrepareRejectsInvalidGraphsTransactionally, failures);
    runTest("publication metadata path is reserved", testPublicationMetadataPathIsReserved,
            failures);

    if (failures != 0)
    {
        std::cerr << failures << " asset generation publication test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D asset generation publication tests passed.\n";
    return 0;
}

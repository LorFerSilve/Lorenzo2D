#include <Lorenzo2D/Assets/AssetManager.hpp>
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

    if (failures != 0)
    {
        std::cerr << failures << " resource test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D resource tests passed.\n";
    return 0;
}

#include "TestSupport.hpp"

#include <Lorenzo2D/Save/SaveGame.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>

namespace
{
    using l2d::test::runTest;

    l2d::SaveDocument makeDocument()
    {
        l2d::SaveDocument document("phase10.test", 3u);
        L2D_REQUIRE(document.setBool("door.open", true));
        L2D_REQUIRE(document.setInteger("player.coins", 42));
        L2D_REQUIRE(document.setNumber("player.health", 87.5));
        L2D_REQUIRE(document.setString("checkpoint", "harbor"));
        return document;
    }

    void testTypedRoundTripIsDeterministic()
    {
        const l2d::SaveDocument source = makeDocument();
        std::ostringstream first;
        std::ostringstream second;
        L2D_REQUIRE(l2d::SaveGameSerializer::save(first, source));
        L2D_REQUIRE(l2d::SaveGameSerializer::save(second, source));
        L2D_REQUIRE_EQUAL(first.str(), second.str());

        l2d::SaveDocument loaded;
        std::istringstream input(first.str());
        L2D_REQUIRE(l2d::SaveGameSerializer::load(input, loaded));
        L2D_REQUIRE(loaded == source);
        L2D_REQUIRE(loaded.boolean("door.open") == std::optional<bool>(true));
        L2D_REQUIRE(loaded.integer("player.coins") == std::optional<std::int64_t>(42));
        L2D_REQUIRE(loaded.number("player.health") == std::optional<double>(87.5));
        L2D_REQUIRE(loaded.string("checkpoint") == std::optional<std::string>("harbor"));
        L2D_REQUIRE(!loaded.string("player.coins").has_value());
    }

    void testInvalidInputIsTransactional()
    {
        l2d::SaveDocument destination("existing", 2u);
        L2D_REQUIRE(destination.setString("state", "kept"));
        const l2d::SaveDocument before = destination;

        std::istringstream malformed("{not-json");
        L2D_REQUIRE(!l2d::SaveGameSerializer::load(malformed, destination));
        L2D_REQUIRE(destination == before);

        std::istringstream unsupported(
            R"({"format_version":99,"schema":"x","revision":1,"values":{}})");
        L2D_REQUIRE(!l2d::SaveGameSerializer::load(unsupported, destination));
        L2D_REQUIRE(destination == before);

        L2D_REQUIRE(!destination.setNumber("bad", std::numeric_limits<double>::infinity()));
        L2D_REQUIRE(destination == before);
    }

    void testLimitsRejectOversizedDocuments()
    {
        l2d::SaveDocument document("phase10.test", 1u);
        L2D_REQUIRE(document.setString("large", std::string(32u, 'x')));

        l2d::SaveGameLimits limits;
        limits.maxStringBytes = 8u;
        std::ostringstream output;
        L2D_REQUIRE(!l2d::SaveGameSerializer::save(output, document, limits));

        limits = {};
        limits.maxEntries = 1u;
        L2D_REQUIRE(document.setBool("second", true));
        std::ostringstream secondOutput;
        L2D_REQUIRE(!l2d::SaveGameSerializer::save(secondOutput, document, limits));
    }

    void testFileRoundTripReplacesExistingSave()
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "lorenzo2d_phase10_save_test.json";
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + ".tmp", error);
        std::filesystem::remove(path.string() + ".bak", error);

        l2d::SaveDocument first("phase10.file", 1u);
        L2D_REQUIRE(first.setInteger("slot", 1));
        L2D_REQUIRE(l2d::SaveGameSerializer::saveToFile(path, first));

        l2d::SaveDocument replacement("phase10.file", 2u);
        L2D_REQUIRE(replacement.setInteger("slot", 2));
        L2D_REQUIRE(l2d::SaveGameSerializer::saveToFile(path, replacement));

        l2d::SaveDocument loaded;
        L2D_REQUIRE(l2d::SaveGameSerializer::loadFromFile(path, loaded));
        L2D_REQUIRE(loaded == replacement);

        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + ".tmp", error);
        std::filesystem::remove(path.string() + ".bak", error);
    }
}

int main()
{
    int failures = 0;
    runTest("save typed deterministic round trip", testTypedRoundTripIsDeterministic, failures);
    runTest("save invalid input is transactional", testInvalidInputIsTransactional, failures);
    runTest("save limits", testLimitsRejectOversizedDocuments, failures);
    runTest("save file replacement", testFileRoundTripReplacesExistingSave, failures);
    return failures == 0 ? 0 : 1;
}

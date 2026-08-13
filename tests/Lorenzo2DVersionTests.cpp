#include "TestSupport.hpp"

#include <Lorenzo2D/Core/Version.hpp>

#include <iostream>
#include <string_view>

namespace
{
    using l2d::test::runTest;

    void testGeneratedVersionMatchesProjectVersion()
    {
        L2D_REQUIRE_EQUAL(l2d::VersionMajor, 0u);
        L2D_REQUIRE_EQUAL(l2d::VersionMinor, 10u);
        L2D_REQUIRE_EQUAL(l2d::VersionPatch, 0u);
        L2D_REQUIRE_EQUAL(l2d::VersionString, std::string_view("0.10.0"));
    }

    void testVersionMacrosMatchConstants()
    {
        L2D_REQUIRE_EQUAL(LORENZO2D_VERSION_MAJOR, l2d::VersionMajor);
        L2D_REQUIRE_EQUAL(LORENZO2D_VERSION_MINOR, l2d::VersionMinor);
        L2D_REQUIRE_EQUAL(LORENZO2D_VERSION_PATCH, l2d::VersionPatch);
        L2D_REQUIRE_EQUAL(std::string_view(LORENZO2D_VERSION_STRING), l2d::VersionString);
    }
}

int main()
{
    int failures = 0;

    runTest("generated version matches project version", testGeneratedVersionMatchesProjectVersion,
            failures);
    runTest("version macros match constants", testVersionMacrosMatchConstants, failures);

    if (failures != 0)
    {
        std::cerr << failures << " version test(s) failed.\n";
        return 1;
    }

    std::cout << "Lorenzo2D version tests passed.\n";
    return 0;
}

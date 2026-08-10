#include "TestSupport.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
    using l2d::test::runTest;

    struct Vector2
    {
        float x;
        float y;

        friend bool operator==(Vector2 left, Vector2 right)
        {
            return left.x == right.x && left.y == right.y;
        }
    };

    enum class TestPhase
    {
        Begin = 2,
        End = 3
    };

    template <typename Function> std::string captureFailure(Function&& function)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (const std::runtime_error& exception)
        {
            return exception.what();
        }

        throw std::runtime_error("expected the assertion helper to fail");
    }

    void testRequirePreservesLegacyMessage()
    {
        const std::string message =
            captureFailure([]() { l2d::test::require(false, "value > 0", 42); });

        L2D_REQUIRE_EQUAL(message, "line 42: value > 0");
    }

    void testRequireEqualReportsExpressionsAndValues()
    {
        const std::string message = captureFailure(
            []() { l2d::test::requireEqual(3, 4, "actualCount", "expectedCount", 17); });

        L2D_REQUIRE_EQUAL(message, "line 17: expected actualCount == expectedCount "
                                   "(actual: 3, expected: 4)");
    }

    void testRequireEqualHandlesMixedIntegralSigns()
    {
        const std::size_t actualCount = 3;
        L2D_REQUIRE_EQUAL(actualCount, 3);

        const std::string message = captureFailure(
            []()
            { l2d::test::requireEqual(std::size_t{0}, -1, "actualCount", "expectedCount", 19); });

        L2D_REQUIRE_EQUAL(message, "line 19: expected actualCount == expectedCount "
                                   "(actual: 0, expected: -1)");
    }

    void testRequireEqualFormatsEnumsAndVectors()
    {
        const std::string enumMessage = captureFailure(
            []()
            {
                l2d::test::requireEqual(TestPhase::Begin, TestPhase::End, "actualPhase",
                                        "expectedPhase", 27);
            });

        L2D_REQUIRE_EQUAL(enumMessage, "line 27: expected actualPhase == expectedPhase "
                                       "(actual: 2, expected: 3)");

        const Vector2 actual{1.0f, 2.0f};
        const Vector2 expected{3.0f, 4.0f};
        const std::string vectorMessage = captureFailure(
            [&actual, &expected]()
            {
                l2d::test::requireEqual(actual, expected, "actualPosition", "expectedPosition", 35);
            });

        L2D_REQUIRE_EQUAL(vectorMessage, "line 35: expected actualPosition == expectedPosition "
                                         "(actual: (1, 2), expected: (3, 4))");
    }

    void testApproximateMacroDispatchesVectorValues()
    {
        const Vector2 actual{2.0f, 4.0005f};
        const Vector2 expected{2.0005f, 4.0f};

        L2D_REQUIRE_APPROX(actual, expected, 0.001f);
    }

    void testApproximateComparisonUsesExplicitEpsilon()
    {
        L2D_REQUIRE(l2d::test::approximatelyEqual(1.0, 1.0005, 0.001));
        L2D_REQUIRE(!l2d::test::approximatelyEqual(1.0, 1.0005, 0.0001));
    }

    void testRequireApproximateReportsValuesAndEpsilon()
    {
        const std::string message = captureFailure(
            []()
            {
                l2d::test::requireApproximatelyEqual(1.0, 1.5, 0.1, "actualTime", "expectedTime",
                                                     23);
            });

        L2D_REQUIRE_EQUAL(message, "line 23: expected actualTime ~= expectedTime "
                                   "(actual: 1, expected: 1.5, epsilon: 0.1)");
    }

    void testTwoDimensionalComparisonUsesExplicitEpsilon()
    {
        const Vector2 actual{2.0f, 4.0005f};
        const Vector2 expected{2.0005f, 4.0f};

        L2D_REQUIRE(l2d::test::approximatelyEqual2D(actual, expected, 0.001f));
        L2D_REQUIRE(!l2d::test::approximatelyEqual2D(actual, expected, 0.0001f));
    }

    void testRequireApproximate2DReportsComponents()
    {
        const Vector2 actual{2.0f, 4.0f};
        const Vector2 expected{3.0f, 5.0f};

        const std::string message = captureFailure(
            [&actual, &expected]()
            {
                l2d::test::requireApproximatelyEqual2D(actual, expected, 0.25f, "actualPosition",
                                                       "expectedPosition", 31);
            });

        L2D_REQUIRE_EQUAL(message, "line 31: expected actualPosition ~= expectedPosition "
                                   "(actual: (2, 4), expected: (3, 5), epsilon: 0.25)");
    }

    void testTemporaryFileRemovesCreatedFile()
    {
        std::filesystem::path temporaryPath;

        {
            l2d::test::TemporaryFile temporaryFile("lorenzo2d_test_support", ".tmp");
            temporaryPath = temporaryFile.path();

            std::ofstream output(temporaryPath);
            output << "temporary test data";
            output.close();

            L2D_REQUIRE(std::filesystem::exists(temporaryPath));
        }

        L2D_REQUIRE(!std::filesystem::exists(temporaryPath));
    }

    void testDeterministicReplayAcceptsMatchingSnapshots()
    {
        const auto replay = []() { return Vector2{3.f, 7.f}; };

        L2D_REQUIRE_DETERMINISTIC_REPLAY(replay);
    }

    void testDeterministicReplayReportsDivergence()
    {
        int invocation = 0;
        const auto replay = [&invocation]() { return invocation++; };

        const std::string message = captureFailure(
            [&replay]() { l2d::test::requireDeterministicReplay(replay, "replay", 61); });

        L2D_REQUIRE_EQUAL(message, "line 61: deterministic replay replay diverged "
                                   "(first: 0, second: 1)");
    }
}

int main()
{
    int failures = 0;

    runTest("require preserves legacy message", testRequirePreservesLegacyMessage, failures);
    runTest("requireEqual reports expressions and values",
            testRequireEqualReportsExpressionsAndValues, failures);
    runTest("requireEqual handles mixed integral signs", testRequireEqualHandlesMixedIntegralSigns,
            failures);
    runTest("requireEqual formats enums and vectors", testRequireEqualFormatsEnumsAndVectors,
            failures);
    runTest("approximate macro dispatches vector values",
            testApproximateMacroDispatchesVectorValues, failures);
    runTest("approximate comparison uses explicit epsilon",
            testApproximateComparisonUsesExplicitEpsilon, failures);
    runTest("requireApproximate reports values and epsilon",
            testRequireApproximateReportsValuesAndEpsilon, failures);
    runTest("two-dimensional comparison uses explicit epsilon",
            testTwoDimensionalComparisonUsesExplicitEpsilon, failures);
    runTest("requireApproximate2D reports components", testRequireApproximate2DReportsComponents,
            failures);
    runTest("TemporaryFile removes created file", testTemporaryFileRemovesCreatedFile, failures);
    runTest("deterministic replay accepts matching snapshots",
            testDeterministicReplayAcceptsMatchingSnapshots, failures);
    runTest("deterministic replay reports divergence", testDeterministicReplayReportsDivergence,
            failures);

    if (failures != 0)
    {
        std::cerr << failures << " test-support test(s) failed.\n";
        return 1;
    }

    std::cout << "Lorenzo2D test-support tests passed.\n";
    return 0;
}

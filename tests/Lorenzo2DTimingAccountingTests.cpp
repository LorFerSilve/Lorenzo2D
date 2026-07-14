#include <Lorenzo2D/Core/FixedStepScheduler.hpp>

#include <limits>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::approximatelyEqual;
    using l2d::test::runTest;

    void testFrameClampIsReportedSeparatelyFromDroppedTicks()
    {
        l2d::FixedStepConfig config;
        config.fixedDeltaTime = 0.125;
        config.maximumFrameDeltaTime = 0.6875;
        config.maximumTicksPerFrame = 3;

        l2d::FixedStepScheduler scheduler(config);

        const l2d::FixedStepFrame stalledFrame = scheduler.advance(1.0625);

        L2D_REQUIRE(approximatelyEqual(stalledFrame.rawDeltaTime, 1.0625));
        L2D_REQUIRE(approximatelyEqual(stalledFrame.frameDeltaTime, 0.6875));
        L2D_REQUIRE(approximatelyEqual(stalledFrame.clampedFrameTime, 0.375));
        L2D_REQUIRE(stalledFrame.ticksToRun == 3);
        L2D_REQUIRE(stalledFrame.droppedTicks == 2);
        L2D_REQUIRE(approximatelyEqual(
            stalledFrame.droppedSimulationTime,
            0.25
        ));

        const l2d::FixedStepFrame recoveredFrame = scheduler.advance(0.0625);

        L2D_REQUIRE(approximatelyEqual(recoveredFrame.clampedFrameTime, 0.0));
        L2D_REQUIRE(recoveredFrame.ticksToRun == 1);
        L2D_REQUIRE(recoveredFrame.droppedTicks == 0);

        L2D_REQUIRE(approximatelyEqual(scheduler.clampedFrameTime(), 0.375));
        L2D_REQUIRE(approximatelyEqual(scheduler.realElapsedTime(), 1.125));
        L2D_REQUIRE(approximatelyEqual(scheduler.simulationTime(), 0.5));
        L2D_REQUIRE(approximatelyEqual(scheduler.droppedSimulationTime(), 0.25));
        L2D_REQUIRE(approximatelyEqual(scheduler.accumulator(), 0.0));

        const double accountedTime =
            scheduler.simulationTime() +
            scheduler.droppedSimulationTime() +
            scheduler.clampedFrameTime() +
            scheduler.accumulator();

        L2D_REQUIRE(approximatelyEqual(
            accountedTime,
            scheduler.realElapsedTime()
        ));
    }

    void testInvalidFrameDeltasDoNotCreateClampTelemetry()
    {
        l2d::FixedStepScheduler scheduler;

        const l2d::FixedStepFrame negativeFrame = scheduler.advance(-1.0);
        const l2d::FixedStepFrame infiniteFrame = scheduler.advance(
            std::numeric_limits<double>::infinity()
        );

        L2D_REQUIRE(approximatelyEqual(negativeFrame.clampedFrameTime, 0.0));
        L2D_REQUIRE(approximatelyEqual(infiniteFrame.clampedFrameTime, 0.0));
        L2D_REQUIRE(approximatelyEqual(scheduler.clampedFrameTime(), 0.0));
    }

    void testResetClearsClampTelemetry()
    {
        l2d::FixedStepConfig config;
        config.maximumFrameDeltaTime = 0.1;

        l2d::FixedStepScheduler scheduler(config);
        scheduler.advance(1.0);

        L2D_REQUIRE(scheduler.clampedFrameTime() > 0.0);

        scheduler.reset();

        L2D_REQUIRE(approximatelyEqual(scheduler.clampedFrameTime(), 0.0));
        L2D_REQUIRE(approximatelyEqual(scheduler.realElapsedTime(), 0.0));
        L2D_REQUIRE(approximatelyEqual(scheduler.simulationTime(), 0.0));
        L2D_REQUIRE(approximatelyEqual(scheduler.accumulator(), 0.0));
    }
}

int main()
{
    int failures = 0;

    runTest(
        "frame clamp is reported separately from dropped ticks",
        testFrameClampIsReportedSeparatelyFromDroppedTicks,
        failures
    );
    runTest(
        "invalid frame deltas do not create clamp telemetry",
        testInvalidFrameDeltasDoNotCreateClampTelemetry,
        failures
    );
    runTest(
        "reset clears clamp telemetry",
        testResetClearsClampTelemetry,
        failures
    );

    if (failures != 0)
    {
        std::cerr << failures << " timing accounting test(s) failed.\n";
        return 1;
    }

    std::cout << "Lorenzo2D timing accounting tests passed.\n";
    return 0;
}

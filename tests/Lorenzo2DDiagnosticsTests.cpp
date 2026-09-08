#include "TestSupport.hpp"

#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    using l2d::test::runTest;

    void testConfigValidation()
    {
        L2D_REQUIRE(l2d::Profiler::isValidConfig({}));

        l2d::ProfilerConfig invalid;
        invalid.sampleWindow = 0u;
        L2D_REQUIRE(!l2d::Profiler::isValidConfig(invalid));

        invalid = {};
        invalid.maxScopes = 0u;
        L2D_REQUIRE(!l2d::Profiler::isValidConfig(invalid));

        bool threw = false;
        try
        {
            (void)l2d::Profiler(invalid);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }
        L2D_REQUIRE(threw);
    }

    void testFrameAggregationAndRollingWindow()
    {
        l2d::ProfilerConfig config;
        config.sampleWindow = 3u;
        config.maxScopes = 4u;
        config.maxNameBytes = 16u;
        l2d::Profiler profiler(config);

        L2D_REQUIRE(profiler.beginFrame());
        L2D_REQUIRE(!profiler.beginFrame());
        L2D_REQUIRE(profiler.record("physics", 1.5));
        L2D_REQUIRE(profiler.record("physics", 2.5));
        L2D_REQUIRE(profiler.record("render", 3.0));
        L2D_REQUIRE(profiler.endFrame());
        L2D_REQUIRE_EQUAL(profiler.frameIndex(), 1u);

        const auto physics = profiler.statistics("physics");
        L2D_REQUIRE(physics.has_value());
        L2D_REQUIRE_APPROX(physics->currentMilliseconds, 4.0, 0.000001);
        L2D_REQUIRE_APPROX(physics->rollingAverageMilliseconds, 4.0, 0.000001);
        L2D_REQUIRE_EQUAL(physics->sampleCount, 1u);

        L2D_REQUIRE(profiler.record("physics", 2.0));
        L2D_REQUIRE(profiler.record("physics", 6.0));
        L2D_REQUIRE(profiler.record("physics", 8.0));

        const auto rolling = profiler.statistics("physics");
        L2D_REQUIRE(rolling.has_value());
        L2D_REQUIRE_EQUAL(rolling->sampleCount, 4u);
        L2D_REQUIRE_EQUAL(rolling->samplesMilliseconds.size(), 3u);
        L2D_REQUIRE_APPROX(rolling->samplesMilliseconds[0], 2.0, 0.000001);
        L2D_REQUIRE_APPROX(rolling->samplesMilliseconds[1], 6.0, 0.000001);
        L2D_REQUIRE_APPROX(rolling->samplesMilliseconds[2], 8.0, 0.000001);
        L2D_REQUIRE_APPROX(rolling->rollingAverageMilliseconds, 16.0 / 3.0, 0.000001);
        L2D_REQUIRE_APPROX(rolling->maximumMilliseconds, 8.0, 0.000001);
    }

    void testBoundsAndDisabledProfiler()
    {
        l2d::ProfilerConfig config;
        config.sampleWindow = 2u;
        config.maxScopes = 2u;
        config.maxNameBytes = 8u;
        l2d::Profiler profiler(config);

        L2D_REQUIRE(profiler.record("one", 1.0));
        L2D_REQUIRE(profiler.record("two", 2.0));
        L2D_REQUIRE(!profiler.record("three", 3.0));
        L2D_REQUIRE(!profiler.record("", 1.0));
        L2D_REQUIRE(!profiler.record("too-long-name", 1.0));
        L2D_REQUIRE(!profiler.record("one", -1.0));
        L2D_REQUIRE(!profiler.record("one", std::numeric_limits<double>::infinity()));

        profiler.setEnabled(false);
        L2D_REQUIRE(!profiler.record("one", 5.0));
        L2D_REQUIRE(!profiler.beginFrame());

        const auto one = profiler.statistics("one");
        L2D_REQUIRE(one.has_value());
        L2D_REQUIRE_EQUAL(one->sampleCount, 1u);
    }

    void testScopedMeasurement()
    {
        l2d::Profiler profiler;
        {
            auto scope = profiler.scope("scoped");
            (void)scope;
        }

        const auto statistics = profiler.statistics("scoped");
        L2D_REQUIRE(statistics.has_value());
        L2D_REQUIRE_EQUAL(statistics->sampleCount, 1u);
        L2D_REQUIRE(statistics->currentMilliseconds >= 0.0);
    }

    void testCountersSaturate()
    {
        l2d::DiagnosticCounters counters;
        counters.set(l2d::DiagnosticCounter::PhysicsQueries,
                     std::numeric_limits<std::uint64_t>::max() - 1u);
        counters.add(l2d::DiagnosticCounter::PhysicsQueries, 100u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::PhysicsQueries),
                          std::numeric_limits<std::uint64_t>::max());

        counters.increment(l2d::DiagnosticCounter::DrawCalls);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::DrawCalls), 1u);
        L2D_REQUIRE_EQUAL(l2d::DiagnosticCounters::name(l2d::DiagnosticCounter::DrawCalls),
                          std::string_view("draw_calls"));

        counters.reset();
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::DrawCalls), 0u);
    }

    void testSnapshotReportIsDeterministic()
    {
        l2d::Profiler profiler;
        L2D_REQUIRE(profiler.beginFrame());
        L2D_REQUIRE(profiler.record("render", 4.0));
        L2D_REQUIRE(profiler.record("physics", 2.0));
        L2D_REQUIRE(profiler.endFrame());

        l2d::DiagnosticCounters counters;
        counters.set(l2d::DiagnosticCounter::ActiveEntities, 42u);
        counters.set(l2d::DiagnosticCounter::DrawCalls, 7u);

        const l2d::DiagnosticSnapshot snapshot =
            l2d::captureDiagnosticSnapshot(profiler, counters);
        const std::string first = l2d::DiagnosticReport::toJson(snapshot);
        const std::string second = l2d::DiagnosticReport::toJson(snapshot);

        L2D_REQUIRE_EQUAL(first, second);
        L2D_REQUIRE(first.find("\"format_version\": 1") != std::string::npos);
        L2D_REQUIRE(first.find("\"frame_index\": 1") != std::string::npos);
        L2D_REQUIRE(first.find("\"active_entities\"") != std::string::npos);
        L2D_REQUIRE(first.find("\"physics\"") < first.find("\"render\""));

        std::ostringstream output;
        L2D_REQUIRE(l2d::DiagnosticReport::writeJson(output, snapshot, false));
        L2D_REQUIRE(!output.str().empty());
    }
}

int main()
{
    int failures = 0;
    runTest("diagnostics config validation", testConfigValidation, failures);
    runTest("diagnostics frame aggregation", testFrameAggregationAndRollingWindow, failures);
    runTest("diagnostics bounds and disable", testBoundsAndDisabledProfiler, failures);
    runTest("diagnostics scoped measurement", testScopedMeasurement, failures);
    runTest("diagnostics counter saturation", testCountersSaturate, failures);
    runTest("diagnostics deterministic report", testSnapshotReportIsDeterministic, failures);
    return failures == 0 ? 0 : 1;
}

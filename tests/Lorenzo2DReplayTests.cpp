#include "TestSupport.hpp"

#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using l2d::test::runTest;

    std::vector<std::uint8_t> input(std::uint8_t value)
    {
        return {value};
    }

    void testHasherIsStableAndDelimited()
    {
        l2d::DeterministicHasher64 first;
        first.appendString("player");
        first.appendInt64(-42);
        first.appendUInt64(99u);

        l2d::DeterministicHasher64 second;
        second.appendString("player");
        second.appendInt64(-42);
        second.appendUInt64(99u);

        L2D_REQUIRE_EQUAL(first.value(), second.value());

        l2d::DeterministicHasher64 left;
        left.appendString("ab");
        left.appendString("c");

        l2d::DeterministicHasher64 right;
        right.appendString("a");
        right.appendString("bc");

        L2D_REQUIRE(left.value() != right.value());

        first.reset();
        L2D_REQUIRE_EQUAL(first.value(), l2d::DeterministicHasher64::OffsetBasis);
    }

    void testConfigAndBoundsAreTransactional()
    {
        l2d::ReplayTraceConfig invalid;
        invalid.maxTicks = 0u;
        L2D_REQUIRE(!l2d::ReplayTrace::isValidConfig(invalid));

        bool threw = false;
        try
        {
            (void)l2d::ReplayTrace(invalid);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }
        L2D_REQUIRE(threw);

        l2d::ReplayTraceConfig config;
        config.maxTicks = 2u;
        config.maxInputBytesPerTick = 2u;
        config.maxTotalInputBytes = 3u;
        l2d::ReplayTrace trace(config);

        L2D_REQUIRE(trace.record(10u, {std::uint8_t{1u}, std::uint8_t{2u}}, 100u));
        L2D_REQUIRE(!trace.record(10u, input(std::uint8_t{3u}), 200u));
        L2D_REQUIRE(!trace.record(
            11u, {std::uint8_t{3u}, std::uint8_t{4u}, std::uint8_t{5u}}, 200u));
        L2D_REQUIRE(trace.record(11u, input(std::uint8_t{3u}), 200u));
        L2D_REQUIRE(!trace.record(12u, {}, 300u));

        L2D_REQUIRE_EQUAL(trace.size(), 2u);
        L2D_REQUIRE_EQUAL(trace.totalInputBytes(), 3u);
        L2D_REQUIRE_EQUAL(trace.ticks()[0].tick, 10u);
        L2D_REQUIRE_EQUAL(trace.ticks()[1].stateHash, 200u);

        trace.clear();
        L2D_REQUIRE_EQUAL(trace.size(), 0u);
        L2D_REQUIRE_EQUAL(trace.totalInputBytes(), 0u);
    }

    void testEquivalentReplay()
    {
        l2d::ReplayTrace expected;
        l2d::ReplayTrace actual;

        for (std::uint64_t tick = 0u; tick < 4u; ++tick)
        {
            const auto bytes = input(static_cast<std::uint8_t>(tick));
            L2D_REQUIRE(expected.record(tick, bytes, tick * 10u));
            L2D_REQUIRE(actual.record(tick, bytes, tick * 10u));
        }

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(expected, actual);
        L2D_REQUIRE(comparison.equivalent());
        L2D_REQUIRE(!comparison.firstDivergenceTick.has_value());
        L2D_REQUIRE_EQUAL(comparison.matchedTicks, 4u);
    }

    void testFirstInputAndStateDivergence()
    {
        l2d::ReplayTrace expected;
        l2d::ReplayTrace inputDiverged;
        l2d::ReplayTrace stateDiverged;

        for (std::uint64_t tick = 0u; tick < 6u; ++tick)
        {
            const auto expectedInput = input(static_cast<std::uint8_t>(tick));
            auto alternateInput = expectedInput;
            if (tick == 3u) alternateInput[0] ^= std::uint8_t{1u};

            L2D_REQUIRE(expected.record(tick, expectedInput, tick + 100u));
            L2D_REQUIRE(inputDiverged.record(tick, alternateInput, tick + 100u));
            L2D_REQUIRE(stateDiverged.record(tick, expectedInput,
                                             tick == 4u ? 999u : tick + 100u));
        }

        const l2d::ReplayComparison inputComparison =
            l2d::compareReplayTraces(expected, inputDiverged);
        L2D_REQUIRE_EQUAL(inputComparison.divergence, l2d::ReplayDivergence::Input);
        L2D_REQUIRE_EQUAL(inputComparison.firstDivergenceTick, std::optional<std::uint64_t>(3u));
        L2D_REQUIRE_EQUAL(inputComparison.matchedTicks, 3u);

        const l2d::ReplayComparison stateComparison =
            l2d::compareReplayTraces(expected, stateDiverged);
        L2D_REQUIRE_EQUAL(stateComparison.divergence, l2d::ReplayDivergence::State);
        L2D_REQUIRE_EQUAL(stateComparison.firstDivergenceTick, std::optional<std::uint64_t>(4u));
        L2D_REQUIRE_EQUAL(stateComparison.matchedTicks, 4u);
    }

    void testTickAndLengthDivergence()
    {
        l2d::ReplayTrace expected;
        l2d::ReplayTrace shifted;

        L2D_REQUIRE(expected.record(10u, {}, 1u));
        L2D_REQUIRE(expected.record(11u, {}, 2u));
        L2D_REQUIRE(shifted.record(10u, {}, 1u));
        L2D_REQUIRE(shifted.record(12u, {}, 2u));

        const l2d::ReplayComparison tickComparison =
            l2d::compareReplayTraces(expected, shifted);
        L2D_REQUIRE_EQUAL(tickComparison.divergence, l2d::ReplayDivergence::Tick);
        L2D_REQUIRE_EQUAL(tickComparison.firstDivergenceTick, std::optional<std::uint64_t>(11u));
        L2D_REQUIRE_EQUAL(tickComparison.matchedTicks, 1u);

        l2d::ReplayTrace shorter;
        L2D_REQUIRE(shorter.record(10u, {}, 1u));

        const l2d::ReplayComparison lengthComparison =
            l2d::compareReplayTraces(expected, shorter);
        L2D_REQUIRE_EQUAL(lengthComparison.divergence, l2d::ReplayDivergence::Length);
        L2D_REQUIRE_EQUAL(lengthComparison.firstDivergenceTick,
                          std::optional<std::uint64_t>(11u));
        L2D_REQUIRE_EQUAL(lengthComparison.matchedTicks, 1u);
    }

    void testLongTraceFindsFirstDivergence()
    {
        l2d::ReplayTrace expected;
        l2d::ReplayTrace actual;
        constexpr std::uint64_t DivergenceTick = 8192u;

        for (std::uint64_t tick = 0u; tick < 10'000u; ++tick)
        {
            l2d::DeterministicHasher64 hasher;
            hasher.appendString("simulation-state");
            hasher.appendUInt64(tick);
            const std::uint64_t expectedHash = hasher.value();
            const std::uint64_t actualHash =
                tick == DivergenceTick ? expectedHash ^ 1u : expectedHash;

            const auto bytes = input(static_cast<std::uint8_t>(tick % 251u));
            L2D_REQUIRE(expected.record(tick, bytes, expectedHash));
            L2D_REQUIRE(actual.record(tick, bytes, actualHash));
        }

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(expected, actual);
        L2D_REQUIRE_EQUAL(comparison.divergence, l2d::ReplayDivergence::State);
        L2D_REQUIRE_EQUAL(comparison.firstDivergenceTick,
                          std::optional<std::uint64_t>(DivergenceTick));
        L2D_REQUIRE_EQUAL(comparison.matchedTicks,
                          static_cast<std::size_t>(DivergenceTick));
    }
}

int main()
{
    int failures = 0;
    runTest("replay stable hasher", testHasherIsStableAndDelimited, failures);
    runTest("replay bounded trace", testConfigAndBoundsAreTransactional, failures);
    runTest("replay equivalent traces", testEquivalentReplay, failures);
    runTest("replay input and state divergence", testFirstInputAndStateDivergence, failures);
    runTest("replay tick and length divergence", testTickAndLengthDivergence, failures);
    runTest("replay long first divergence", testLongTraceFindsFirstDivergence, failures);
    return failures == 0 ? 0 : 1;
}

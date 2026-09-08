#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>

#include <iostream>

int main()
{
    l2d::Profiler profiler;
    l2d::DiagnosticCounters counters;

    if (!profiler.beginFrame()) return 1;

    (void)profiler.record("fixed-step", 1.8);
    (void)profiler.record("physics", 0.7);
    (void)profiler.record("render", 3.4);

    if (!profiler.endFrame()) return 1;

    counters.set(l2d::DiagnosticCounter::ActiveEntities, 128u);
    counters.set(l2d::DiagnosticCounter::Colliders, 96u);
    counters.set(l2d::DiagnosticCounter::DrawCalls, 14u);

    const l2d::DiagnosticSnapshot snapshot =
        l2d::captureDiagnosticSnapshot(profiler, counters);

    l2d::DeterministicHasher64 stateHasher;
    stateHasher.appendString("phase11-example");
    stateHasher.appendUInt64(snapshot.frameIndex);

    l2d::ReplayTrace replay;
    if (!replay.record(0u, {}, stateHasher.value())) return 1;

    const l2d::ReplayComparison replayCheck = l2d::compareReplayTraces(replay, replay);
    if (!replayCheck.equivalent()) return 1;

    std::cout << l2d::DiagnosticReport::toJson(snapshot);
    return 0;
}

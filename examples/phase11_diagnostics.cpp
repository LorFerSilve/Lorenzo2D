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
    std::cout << l2d::DiagnosticReport::toJson(snapshot);
    return 0;
}

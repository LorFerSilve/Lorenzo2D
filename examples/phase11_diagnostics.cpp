#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <iostream>

int main()
{
    l2d::Profiler profiler;
    l2d::DiagnosticCounters counters;

    if (!profiler.beginFrame()) return 1;

    (void)profiler.record(l2d::diagnostic_scope::FixedStep, 1.8);
    (void)profiler.record(l2d::diagnostic_scope::Physics, 0.7);
    (void)profiler.record(l2d::diagnostic_scope::Render, 3.4);

    if (!profiler.endFrame()) return 1;

    l2d::Scene scene("diagnostics-example");
    l2d::GameObject& object = scene.createGameObject("player");
    object.addComponent<l2d::Component>();

    l2d::RenderQueue2D queue;
    queue.build(scene);

    l2d::accumulateSceneDiagnostics(scene, counters);
    l2d::accumulateRenderQueueDiagnostics(queue, counters);
    l2d::recordPhysicsQueries(counters, 3u);
    l2d::recordSaveWriteDiagnostics(counters, 2048u);

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

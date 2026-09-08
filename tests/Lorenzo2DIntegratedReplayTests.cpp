#include "TestSupport.hpp"

#include <Lorenzo2D/Core/FixedStepScheduler.hpp>
#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using l2d::test::runTest;

    constexpr std::size_t IntegratedTicks = 8192u;
    constexpr std::uint64_t InputDivergenceTick = 3072u;
    constexpr std::uint64_t StateDivergenceTick = 6144u;
    constexpr double FixedDeltaTime = 0.015625; // 1 / 64, exactly representable in binary.

    enum class ReplayCadence
    {
        Steady,
        Chunked
    };

    struct ReplayOptions
    {
        ReplayCadence cadence = ReplayCadence::Steady;
        std::optional<std::uint64_t> inputMutationTick;
        std::optional<std::uint64_t> statePerturbationTick;
    };

    struct CanonicalInput
    {
        std::uint8_t horizontal = 0u;
        std::uint8_t uiAction = 0u;
        std::uint8_t navigationAction = 0u;
    };

    class ReplayCounterComponent final : public l2d::Component
    {
      public:
        void onUpdate(float) override
        {
            ++m_updates;
            m_state = m_state * 6364136223846793005ull + 1442695040888963407ull;
            m_state ^= m_updates + 0x9e3779b97f4a7c15ull;
        }

        std::uint64_t updates() const noexcept
        {
            return m_updates;
        }

        std::uint64_t state() const noexcept
        {
            return m_state;
        }

      private:
        std::uint64_t m_updates = 0u;
        std::uint64_t m_state = 0x243f6a8885a308d3ull;
    };

    CanonicalInput inputForTick(std::uint64_t tick)
    {
        CanonicalInput input;
        input.horizontal = ((tick / 64u) % 2u == 0u) ? 1u : 2u;

        const std::uint64_t uiPhase = tick % 96u;
        if (uiPhase == 10u)
            input.uiAction = 1u;
        else if (uiPhase == 11u)
            input.uiAction = 2u;

        if (tick % 128u == 0u) input.navigationAction = ((tick / 128u) % 2u == 0u) ? 1u : 2u;

        return input;
    }

    std::vector<std::uint8_t> encodeInput(const CanonicalInput& input)
    {
        return {input.horizontal, input.uiAction, input.navigationAction};
    }

    std::int64_t quantize(float value)
    {
        L2D_REQUIRE(std::isfinite(value));
        return static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 4096.0));
    }

    void appendVector(l2d::DeterministicHasher64& hasher, sf::Vector2f value)
    {
        hasher.appendInt64(quantize(value.x));
        hasher.appendInt64(quantize(value.y));
    }

    void appendPath(l2d::DeterministicHasher64& hasher, const l2d::NavigationPath2D& path)
    {
        hasher.appendUInt64(static_cast<std::uint64_t>(path.status));
        hasher.appendUInt64(path.gridRevision);
        hasher.appendUInt64(static_cast<std::uint64_t>(path.visitedNodes));
        hasher.appendInt64(quantize(path.totalCost));
        hasher.appendUInt64(static_cast<std::uint64_t>(path.cells.size()));

        for (const sf::Vector2i cell : path.cells)
        {
            hasher.appendInt64(static_cast<std::int64_t>(cell.x));
            hasher.appendInt64(static_cast<std::int64_t>(cell.y));
        }
    }

    l2d::ReplayTrace runIntegratedReplay(const ReplayOptions& options)
    {
        l2d::Scene scene("phase11-integrated-replay");

        l2d::GameObject& mover = scene.createGameObject("replay-mover");
        mover.transform.setPosition({0.f, 0.f});
        l2d::RigidBody2D& body = mover.addComponent<l2d::RigidBody2D>();
        body.setBodyType(l2d::BodyType2D::Dynamic);
        body.setUseGravity(false);
        body.setAllowsSleep(false);
        mover.addComponent<l2d::BoxCollider2D>(sf::Vector2f{8.f, 8.f});
        ReplayCounterComponent& counter = mover.addComponent<ReplayCounterComponent>();

        l2d::GameObject& queryObstacle = scene.createGameObject("replay-query-obstacle");
        queryObstacle.transform.setPosition({100.f, 0.f});
        queryObstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{10.f, 10.f});

        l2d::PhysicsWorld2DConfig physicsConfig;
        physicsConfig.gravity = {0.f, 0.f};
        physicsConfig.continuousCollisionDetection = false;
        physicsConfig.sleeping = false;
        l2d::PhysicsWorld2D world(physicsConfig);

        l2d::NavigationGridConfig2D navigationConfig;
        navigationConfig.size = {9u, 9u};
        navigationConfig.cellSize = {8.f, 8.f};
        navigationConfig.connectivity = l2d::NavigationConnectivity2D::FourWay;
        l2d::NavigationGrid2D navigation(navigationConfig);
        const l2d::AStarPathfinder2D pathfinder;
        l2d::NavigationPath2D path = pathfinder.findPath(navigation, {0, 4}, {8, 4});
        L2D_REQUIRE(path.succeeded());

        l2d::UiCanvas2D ui;
        l2d::UiButton2D button;
        button.id = "replay-button";
        button.label = "Replay";
        button.position = {8.f, 8.f};
        button.size = {64.f, 32.f};
        L2D_REQUIRE(ui.addButton(std::move(button)));

        l2d::ReplayTraceConfig traceConfig;
        traceConfig.maxTicks = IntegratedTicks;
        traceConfig.maxInputBytesPerTick = 3u;
        traceConfig.maxTotalInputBytes = IntegratedTicks * 3u;
        l2d::ReplayTrace trace(traceConfig);

        l2d::FixedStepConfig schedulerConfig;
        schedulerConfig.fixedDeltaTime = FixedDeltaTime;
        schedulerConfig.maximumFrameDeltaTime = FixedDeltaTime * 4.0;
        schedulerConfig.maximumTicksPerFrame = 4u;
        l2d::FixedStepScheduler scheduler(schedulerConfig);

        constexpr std::array<double, 4u> ChunkedFramePattern = {
            FixedDeltaTime * 0.5, FixedDeltaTime * 0.5, FixedDeltaTime, FixedDeltaTime * 2.0};

        std::size_t recordedTicks = 0u;
        std::size_t frameIndex = 0u;

        while (recordedTicks < IntegratedTicks)
        {
            double frameDelta = FixedDeltaTime;
            if (options.cadence == ReplayCadence::Chunked)
                frameDelta = ChunkedFramePattern[frameIndex % ChunkedFramePattern.size()];
            ++frameIndex;

            const l2d::FixedStepFrame frame = scheduler.advance(frameDelta);
            L2D_REQUIRE(frame.droppedTicks == 0u);

            for (std::uint32_t frameTick = 0u; frameTick < frame.ticksToRun; ++frameTick)
            {
                const std::uint64_t tick = static_cast<std::uint64_t>(recordedTicks);
                CanonicalInput input = inputForTick(tick);

                if (options.inputMutationTick && tick == *options.inputMutationTick)
                    input.horizontal = input.horizontal == 1u ? 2u : 1u;

                body.setVelocity({input.horizontal == 1u ? 16.f : -16.f, 0.f});

                if (input.navigationAction == 1u)
                    L2D_REQUIRE(navigation.setTraversalCost({4, 4}, 4.f));
                else if (input.navigationAction == 2u)
                    L2D_REQUIRE(navigation.setTraversalCost({4, 4}, 1.f));

                l2d::PointerState pointer;
                pointer.screenPosition = {16, 16};
                if (input.uiAction == 1u)
                {
                    pointer.down = true;
                    pointer.pressed = true;
                }
                else if (input.uiAction == 2u)
                {
                    pointer.released = true;
                }
                ui.update(pointer);

                scene.fixedUpdate(static_cast<float>(FixedDeltaTime));
                world.step(scene, static_cast<float>(FixedDeltaTime));

                if (options.statePerturbationTick && tick == *options.statePerturbationTick)
                    body.addVelocity({0.5f, 0.f});

                if (tick % 16u == 0u || input.navigationAction != 0u)
                {
                    path = pathfinder.findPath(navigation, {0, 4}, {8, 4});
                    L2D_REQUIRE(path.succeeded());
                    L2D_REQUIRE(path.gridRevision == navigation.revision());
                }

                const l2d::PhysicsQueryContext2D queries = world.createQueryContext(scene);
                const auto hit = queries.raycast({90.f, 0.f}, {110.f, 0.f});
                L2D_REQUIRE(hit.has_value());

                l2d::DeterministicHasher64 state;
                state.appendString("phase11.6-integrated-replay-v1");
                state.appendUInt64(tick);
                state.appendUInt64(counter.updates());
                state.appendUInt64(counter.state());

                appendVector(state, mover.transform.position());
                state.appendInt64(quantize(mover.transform.rotation()));
                appendVector(state, body.velocity());
                state.appendUInt64(body.isGrounded() ? 1u : 0u);

                state.appendUInt64(navigation.revision());
                appendPath(state, path);

                const bool hovered =
                    ui.hoveredButton() == std::optional<std::string>("replay-button");
                const bool pressed =
                    ui.pressedButton() == std::optional<std::string>("replay-button");
                state.appendUInt64(hovered ? 1u : 0u);
                state.appendUInt64(pressed ? 1u : 0u);
                state.appendUInt64(ui.wasActivated("replay-button") ? 1u : 0u);

                state.appendUInt64(static_cast<std::uint64_t>(world.broadPhaseStats().proxyCount));
                state.appendUInt64(static_cast<std::uint64_t>(world.contacts().size()));
                state.appendUInt64(static_cast<std::uint64_t>(queries.proxyCount()));
                state.appendUInt64(hit->categoryBits);
                appendVector(state, hit->point);
                appendVector(state, hit->normal);
                state.appendInt64(quantize(hit->distance));
                state.appendInt64(quantize(hit->fraction));

                L2D_REQUIRE(trace.record(tick, encodeInput(input), state.value()));
                ++recordedTicks;
            }
        }

        L2D_REQUIRE(scheduler.tickCount() == IntegratedTicks);
        L2D_REQUIRE(scheduler.droppedTickCount() == 0u);
        L2D_REQUIRE(counter.updates() == IntegratedTicks);
        L2D_REQUIRE(trace.size() == IntegratedTicks);
        return trace;
    }

    void testIntegratedReplayIsStableAcrossFrameCadence()
    {
        const l2d::ReplayTrace steady = runIntegratedReplay({});
        ReplayOptions chunkedOptions;
        chunkedOptions.cadence = ReplayCadence::Chunked;
        const l2d::ReplayTrace chunked = runIntegratedReplay(chunkedOptions);

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(steady, chunked);
        L2D_REQUIRE(comparison.equivalent());
        L2D_REQUIRE(!comparison.firstDivergenceTick.has_value());
        L2D_REQUIRE_EQUAL(comparison.matchedTicks, IntegratedTicks);
    }

    void testIntegratedReplayFindsInputDivergence()
    {
        const l2d::ReplayTrace expected = runIntegratedReplay({});

        ReplayOptions changed;
        changed.cadence = ReplayCadence::Chunked;
        changed.inputMutationTick = InputDivergenceTick;
        const l2d::ReplayTrace actual = runIntegratedReplay(changed);

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(expected, actual);
        L2D_REQUIRE_EQUAL(comparison.divergence, l2d::ReplayDivergence::Input);
        L2D_REQUIRE_EQUAL(comparison.firstDivergenceTick,
                          std::optional<std::uint64_t>(InputDivergenceTick));
        L2D_REQUIRE_EQUAL(comparison.matchedTicks, static_cast<std::size_t>(InputDivergenceTick));
    }

    void testIntegratedReplayFindsStateDivergence()
    {
        const l2d::ReplayTrace expected = runIntegratedReplay({});

        ReplayOptions changed;
        changed.cadence = ReplayCadence::Chunked;
        changed.statePerturbationTick = StateDivergenceTick;
        const l2d::ReplayTrace actual = runIntegratedReplay(changed);

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(expected, actual);
        L2D_REQUIRE_EQUAL(comparison.divergence, l2d::ReplayDivergence::State);
        L2D_REQUIRE_EQUAL(comparison.firstDivergenceTick,
                          std::optional<std::uint64_t>(StateDivergenceTick));
        L2D_REQUIRE_EQUAL(comparison.matchedTicks, static_cast<std::size_t>(StateDivergenceTick));
    }
}

int main()
{
    int failures = 0;
    runTest("integrated replay ignores frame cadence",
            testIntegratedReplayIsStableAcrossFrameCadence, failures);
    runTest("integrated replay finds input divergence", testIntegratedReplayFindsInputDivergence,
            failures);
    runTest("integrated replay finds state divergence", testIntegratedReplayFindsStateDivergence,
            failures);
    return failures == 0 ? 0 : 1;
}

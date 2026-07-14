#include <Lorenzo2D/Core/FixedStepScheduler.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Scene/SceneManager.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::approximatelyEqual;
    using l2d::test::runTest;

    class TransformSequenceComponent final : public l2d::Component
    {
    public:
        void onUpdate(float) override
        {
            l2d::GameObject* gameObject = owner();

            if (gameObject == nullptr)
                return;

            if (m_updateCount == 0)
            {
                gameObject->transform.setPosition({ 20.f, 30.f });
                gameObject->transform.setRotation(10.f);
                gameObject->transform.setScale({ 3.f, 5.f });
            }
            else
            {
                gameObject->transform.move({ 10.f, 4.f });
                gameObject->transform.rotate(20.f);

                const sf::Vector2f currentScale =
                    gameObject->transform.scale();

                gameObject->transform.setScale({
                    currentScale.x + 2.f,
                    currentScale.y + 2.f
                });
            }

            ++m_updateCount;
        }

    private:
        int m_updateCount = 0;
    };

    void testIdentityTypesCannotBeMovedOrCopied()
    {
        static_assert(!std::is_copy_constructible_v<l2d::Component>);
        static_assert(!std::is_move_constructible_v<l2d::Component>);
        static_assert(!std::is_copy_constructible_v<l2d::GameObject>);
        static_assert(!std::is_move_constructible_v<l2d::GameObject>);
        static_assert(!std::is_copy_constructible_v<l2d::Scene>);
        static_assert(!std::is_move_constructible_v<l2d::Scene>);
        static_assert(!std::is_copy_constructible_v<l2d::SceneManager>);
        static_assert(!std::is_move_constructible_v<l2d::SceneManager>);

        L2D_REQUIRE(true);
    }

    void testFixedStepSchedulerAccumulatesExactSubsteps()
    {
        l2d::FixedStepConfig config;
        config.fixedDeltaTime = 0.125;
        config.maximumFrameDeltaTime = 1.0;
        config.maximumTicksPerFrame = 8;

        l2d::FixedStepScheduler scheduler(config);

        const l2d::FixedStepFrame first = scheduler.advance(0.0625);
        L2D_REQUIRE(first.ticksToRun == 0);
        L2D_REQUIRE(approximatelyEqual(first.interpolationAlpha, 0.5));

        const l2d::FixedStepFrame second = scheduler.advance(0.0625);
        L2D_REQUIRE(second.ticksToRun == 1);
        L2D_REQUIRE(approximatelyEqual(second.interpolationAlpha, 0.0));

        const l2d::FixedStepFrame third = scheduler.advance(0.3125);
        L2D_REQUIRE(third.ticksToRun == 2);
        L2D_REQUIRE(third.droppedTicks == 0);
        L2D_REQUIRE(approximatelyEqual(third.interpolationAlpha, 0.5));

        L2D_REQUIRE(scheduler.frameCount() == 3);
        L2D_REQUIRE(scheduler.tickCount() == 3);
        L2D_REQUIRE(scheduler.droppedTickCount() == 0);
        L2D_REQUIRE(approximatelyEqual(scheduler.accumulator(), 0.0625));
        L2D_REQUIRE(approximatelyEqual(scheduler.realElapsedTime(), 0.4375));
        L2D_REQUIRE(approximatelyEqual(scheduler.simulationTime(), 0.375));
    }

    void testFixedStepSchedulerSnapsFloatingPointBoundaries()
    {
        l2d::FixedStepConfig decimalConfig;
        decimalConfig.fixedDeltaTime = 0.1;
        decimalConfig.maximumFrameDeltaTime = 1.0;
        decimalConfig.maximumTicksPerFrame = 8;

        l2d::FixedStepScheduler decimalScheduler(decimalConfig);
        std::uint64_t decimalTicks = 0;

        for (std::size_t frame = 0; frame < 10; ++frame)
            decimalTicks += decimalScheduler.advance(0.01).ticksToRun;

        L2D_REQUIRE(decimalTicks == 1);
        L2D_REQUIRE(approximatelyEqual(decimalScheduler.accumulator(), 0.0));

        l2d::FixedStepScheduler highRefreshScheduler;
        std::uint64_t highRefreshTicks = 0;

        for (std::size_t frame = 0; frame < 144; ++frame)
        {
            highRefreshTicks += highRefreshScheduler.advance(
                1.0 / 144.0
            ).ticksToRun;
        }

        L2D_REQUIRE(highRefreshTicks == 60);
        L2D_REQUIRE(approximatelyEqual(
            highRefreshScheduler.accumulator(),
            0.0
        ));

        l2d::FixedStepConfig exactRatioConfig;
        exactRatioConfig.fixedDeltaTime = 1.0 / 60.0;
        exactRatioConfig.maximumFrameDeltaTime = 1.0;
        exactRatioConfig.maximumTicksPerFrame = 16;

        l2d::FixedStepScheduler exactRatioScheduler(exactRatioConfig);
        const l2d::FixedStepFrame exactRatioFrame =
            exactRatioScheduler.advance(0.15);

        L2D_REQUIRE(exactRatioFrame.ticksToRun == 9);
        L2D_REQUIRE(approximatelyEqual(
            exactRatioFrame.interpolationAlpha,
            0.0
        ));
        L2D_REQUIRE(approximatelyEqual(exactRatioScheduler.accumulator(), 0.0));

        const l2d::FixedStepFrame followingTinyFrame =
            exactRatioScheduler.advance(0.000001);

        L2D_REQUIRE(followingTinyFrame.ticksToRun == 0);
    }

    void testFixedStepSchedulerBoundsCatchUpAndRecovers()
    {
        l2d::FixedStepConfig config;
        config.fixedDeltaTime = 0.125;
        config.maximumFrameDeltaTime = 0.6875;
        config.maximumTicksPerFrame = 3;

        l2d::FixedStepScheduler scheduler(config);

        const l2d::FixedStepFrame stalledFrame = scheduler.advance(1.0625);

        L2D_REQUIRE(approximatelyEqual(stalledFrame.rawDeltaTime, 1.0625));
        L2D_REQUIRE(approximatelyEqual(stalledFrame.frameDeltaTime, 0.6875));
        L2D_REQUIRE(stalledFrame.ticksToRun == 3);
        L2D_REQUIRE(stalledFrame.droppedTicks == 2);
        L2D_REQUIRE(approximatelyEqual(stalledFrame.droppedSimulationTime, 0.25));
        L2D_REQUIRE(approximatelyEqual(stalledFrame.interpolationAlpha, 0.5));

        const l2d::FixedStepFrame recoveredFrame = scheduler.advance(0.0625);

        L2D_REQUIRE(recoveredFrame.ticksToRun == 1);
        L2D_REQUIRE(recoveredFrame.droppedTicks == 0);
        L2D_REQUIRE(approximatelyEqual(recoveredFrame.interpolationAlpha, 0.0));

        L2D_REQUIRE(scheduler.frameCount() == 2);
        L2D_REQUIRE(scheduler.tickCount() == 4);
        L2D_REQUIRE(scheduler.droppedTickCount() == 2);
        L2D_REQUIRE(approximatelyEqual(scheduler.droppedSimulationTime(), 0.25));
        L2D_REQUIRE(approximatelyEqual(scheduler.realElapsedTime(), 1.125));
        L2D_REQUIRE(approximatelyEqual(scheduler.simulationTime(), 0.5));
        L2D_REQUIRE(approximatelyEqual(scheduler.accumulator(), 0.0));
    }

    void testFixedStepSchedulerSanitizesInvalidInputs()
    {
        l2d::FixedStepConfig invalidConfig;
        invalidConfig.fixedDeltaTime =
            std::numeric_limits<double>::quiet_NaN();
        invalidConfig.maximumFrameDeltaTime =
            -std::numeric_limits<double>::infinity();
        invalidConfig.maximumTicksPerFrame = 0;

        l2d::FixedStepScheduler scheduler(invalidConfig);

        L2D_REQUIRE(approximatelyEqual(
            scheduler.config().fixedDeltaTime,
            1.0 / 60.0
        ));
        L2D_REQUIRE(approximatelyEqual(
            scheduler.config().maximumFrameDeltaTime,
            0.1
        ));
        L2D_REQUIRE(scheduler.config().maximumTicksPerFrame == 8);

        l2d::FixedStepConfig unrepresentableConfig;
        unrepresentableConfig.fixedDeltaTime =
            std::numeric_limits<double>::denorm_min();

        const l2d::FixedStepScheduler unrepresentableScheduler(
            unrepresentableConfig
        );

        L2D_REQUIRE(approximatelyEqual(
            unrepresentableScheduler.config().fixedDeltaTime,
            1.0 / 60.0
        ));

        const std::vector<double> invalidDeltas = {
            -1.0,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity()
        };

        for (double invalidDelta : invalidDeltas)
        {
            const l2d::FixedStepFrame frame = scheduler.advance(invalidDelta);

            L2D_REQUIRE(approximatelyEqual(frame.rawDeltaTime, 0.0));
            L2D_REQUIRE(approximatelyEqual(frame.frameDeltaTime, 0.0));
            L2D_REQUIRE(frame.ticksToRun == 0);
            L2D_REQUIRE(frame.droppedTicks == 0);
            L2D_REQUIRE(frame.interpolationAlpha >= 0.0);
            L2D_REQUIRE(frame.interpolationAlpha < 1.0);
        }

        L2D_REQUIRE(scheduler.frameCount() == invalidDeltas.size());
        L2D_REQUIRE(scheduler.tickCount() == 0);
        L2D_REQUIRE(approximatelyEqual(scheduler.realElapsedTime(), 0.0));
        L2D_REQUIRE(approximatelyEqual(scheduler.simulationTime(), 0.0));

        scheduler.reset();

        L2D_REQUIRE(scheduler.frameCount() == 0);
        L2D_REQUIRE(scheduler.tickCount() == 0);
        L2D_REQUIRE(scheduler.droppedTickCount() == 0);
        L2D_REQUIRE(approximatelyEqual(scheduler.accumulator(), 0.0));
    }

    void testTransformInterpolationTracksFixedSnapshots()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Interpolated");

        object.transform.setPosition({ 10.f, 20.f });
        object.transform.setRotation(350.f);
        object.transform.setScale({ 1.f, 1.f });
        object.addComponent<TransformSequenceComponent>();

        const l2d::TransformState initial = object.transform.interpolated(0.f);
        L2D_REQUIRE(approximatelyEqual(initial.position.x, 10.f));
        L2D_REQUIRE(approximatelyEqual(initial.position.y, 20.f));
        L2D_REQUIRE(approximatelyEqual(initial.rotation, 350.f));
        L2D_REQUIRE(approximatelyEqual(initial.scale.x, 1.f));
        L2D_REQUIRE(approximatelyEqual(initial.scale.y, 1.f));

        scene.fixedUpdate(0.125f);

        const l2d::TransformState previous = object.transform.interpolated(0.f);
        const l2d::TransformState halfway = object.transform.interpolated(0.5f);
        const l2d::TransformState current = object.transform.interpolated(1.f);

        L2D_REQUIRE(approximatelyEqual(previous.position.x, 10.f));
        L2D_REQUIRE(approximatelyEqual(previous.position.y, 20.f));
        L2D_REQUIRE(approximatelyEqual(previous.rotation, 350.f));
        L2D_REQUIRE(approximatelyEqual(previous.scale.x, 1.f));
        L2D_REQUIRE(approximatelyEqual(previous.scale.y, 1.f));

        L2D_REQUIRE(approximatelyEqual(halfway.position.x, 15.f));
        L2D_REQUIRE(approximatelyEqual(halfway.position.y, 25.f));
        L2D_REQUIRE(approximatelyEqual(halfway.rotation, 360.f));
        L2D_REQUIRE(approximatelyEqual(halfway.scale.x, 2.f));
        L2D_REQUIRE(approximatelyEqual(halfway.scale.y, 3.f));

        L2D_REQUIRE(approximatelyEqual(current.position.x, 20.f));
        L2D_REQUIRE(approximatelyEqual(current.position.y, 30.f));
        L2D_REQUIRE(approximatelyEqual(current.rotation, 10.f));
        L2D_REQUIRE(approximatelyEqual(current.scale.x, 3.f));
        L2D_REQUIRE(approximatelyEqual(current.scale.y, 5.f));

        const l2d::TransformState belowRange =
            object.transform.interpolated(-1.f);
        const l2d::TransformState aboveRange =
            object.transform.interpolated(2.f);
        const l2d::TransformState nanAlpha = object.transform.interpolated(
            std::numeric_limits<float>::quiet_NaN()
        );
        const l2d::TransformState negativeInfinity =
            object.transform.interpolated(
                -std::numeric_limits<float>::infinity()
            );
        const l2d::TransformState positiveInfinity =
            object.transform.interpolated(
                std::numeric_limits<float>::infinity()
            );

        L2D_REQUIRE(approximatelyEqual(belowRange.position.x, 10.f));
        L2D_REQUIRE(approximatelyEqual(aboveRange.position.x, 20.f));
        L2D_REQUIRE(approximatelyEqual(nanAlpha.position.x, 20.f));
        L2D_REQUIRE(approximatelyEqual(negativeInfinity.position.x, 10.f));
        L2D_REQUIRE(approximatelyEqual(positiveInfinity.position.x, 20.f));

        scene.fixedUpdate(0.125f);

        const l2d::TransformState secondHalfway =
            object.transform.interpolated(0.5f);

        L2D_REQUIRE(approximatelyEqual(secondHalfway.position.x, 25.f));
        L2D_REQUIRE(approximatelyEqual(secondHalfway.position.y, 32.f));
        L2D_REQUIRE(approximatelyEqual(secondHalfway.rotation, 20.f));
        L2D_REQUIRE(approximatelyEqual(secondHalfway.scale.x, 4.f));
        L2D_REQUIRE(approximatelyEqual(secondHalfway.scale.y, 6.f));

        object.transform.setPosition({ 100.f, 200.f });
        object.transform.setRotation(270.f);
        object.transform.setScale({ 8.f, 9.f });
        object.transform.resetInterpolation();

        const l2d::TransformState teleportedPrevious =
            object.transform.interpolated(0.f);
        const l2d::TransformState teleportedHalfway =
            object.transform.interpolated(0.5f);

        L2D_REQUIRE(approximatelyEqual(teleportedPrevious.position.x, 100.f));
        L2D_REQUIRE(approximatelyEqual(teleportedPrevious.position.y, 200.f));
        L2D_REQUIRE(approximatelyEqual(teleportedPrevious.rotation, 270.f));
        L2D_REQUIRE(approximatelyEqual(teleportedHalfway.position.x, 100.f));
        L2D_REQUIRE(approximatelyEqual(teleportedHalfway.position.y, 200.f));
        L2D_REQUIRE(approximatelyEqual(teleportedHalfway.scale.x, 8.f));
        L2D_REQUIRE(approximatelyEqual(teleportedHalfway.scale.y, 9.f));
    }
}

int main()
{
    int failures = 0;

    runTest("identity types cannot be moved or copied", testIdentityTypesCannotBeMovedOrCopied, failures);
    runTest("fixed scheduler accumulates exact substeps",
        testFixedStepSchedulerAccumulatesExactSubsteps, failures);
    runTest("fixed scheduler snaps floating boundaries",
        testFixedStepSchedulerSnapsFloatingPointBoundaries, failures);
    runTest("fixed scheduler bounds catch-up and recovers",
        testFixedStepSchedulerBoundsCatchUpAndRecovers, failures);
    runTest("fixed scheduler sanitizes invalid input",
        testFixedStepSchedulerSanitizesInvalidInputs, failures);
    runTest("transforms interpolate fixed snapshots",
        testTransformInterpolationTracksFixedSnapshots, failures);

    if (failures != 0)
    {
        std::cerr << failures << " core and timing test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D core and timing tests passed.\n";
    return 0;
}

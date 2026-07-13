#include <Lorenzo2D/Core/FixedStepScheduler.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Scene/SceneManager.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    void require(bool condition, const char* expression, int line)
    {
        if (condition)
            return;

        throw std::runtime_error(
            "line " + std::to_string(line) + ": " + expression
        );
    }

#define L2D_REQUIRE(expression) \
    require(static_cast<bool>(expression), #expression, __LINE__)

    bool approximatelyEqual(float left, float right, float epsilon = 0.0001f)
    {
        return std::fabs(left - right) <= epsilon;
    }

    bool approximatelyEqual(
        double left,
        double right,
        double epsilon = 0.000000001
    )
    {
        return std::fabs(left - right) <= epsilon;
    }

    class TemporaryFile final
    {
    public:
        explicit TemporaryFile(const std::string& prefix)
        {
            const auto timestamp =
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count();
            const auto randomValue = std::random_device{}();

            m_path = std::filesystem::temp_directory_path() /
                (prefix + "_" + std::to_string(timestamp) + "_" +
                    std::to_string(randomValue) + ".txt");
        }

        ~TemporaryFile()
        {
            std::error_code error;
            std::filesystem::remove(m_path, error);
        }

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;

        const std::filesystem::path& path() const
        {
            return m_path;
        }

    private:
        std::filesystem::path m_path;
    };

    class CounterComponent final : public l2d::Component
    {
    public:
        explicit CounterComponent(int& counter)
            : m_counter(&counter)
        {
        }

        void onUpdate(float) override
        {
            ++(*m_counter);
        }

    private:
        int* m_counter;
    };

    class AppendComponentOnce final : public l2d::Component
    {
    public:
        explicit AppendComponentOnce(int& counter)
            : m_counter(&counter)
        {
        }

        void onUpdate(float) override
        {
            if (m_appended || owner() == nullptr)
                return;

            owner()->addComponent<CounterComponent>(*m_counter);
            m_appended = true;
        }

    private:
        int* m_counter;
        bool m_appended = false;
    };

    class DeactivateOwner final : public l2d::Component
    {
    public:
        void onUpdate(float) override
        {
            if (owner() != nullptr)
                owner()->setActive(false);
        }
    };

    class SpawnObjectOnce final : public l2d::Component
    {
    public:
        SpawnObjectOnce(l2d::Scene& scene, int& counter)
            : m_scene(&scene),
            m_counter(&counter)
        {
        }

        void onUpdate(float) override
        {
            if (m_spawned)
                return;

            l2d::GameObject& spawned = m_scene->createGameObject("Spawned");
            spawned.addComponent<CounterComponent>(*m_counter);
            m_spawned = true;
        }

    private:
        l2d::Scene* m_scene;
        int* m_counter;
        bool m_spawned = false;
    };

    class ClearSceneOnUpdate final : public l2d::Component
    {
    public:
        explicit ClearSceneOnUpdate(l2d::Scene& scene)
            : m_scene(&scene)
        {
        }

        void onUpdate(float) override
        {
            m_scene->clear();
        }

    private:
        l2d::Scene* m_scene;
    };

    class ClearSceneManagerOnUpdate final : public l2d::Component
    {
    public:
        explicit ClearSceneManagerOnUpdate(l2d::SceneManager& sceneManager)
            : m_sceneManager(&sceneManager)
        {
        }

        void onUpdate(float) override
        {
            m_sceneManager->clear();
        }

    private:
        l2d::SceneManager* m_sceneManager;
    };

    class ClearAndCreateSceneOnUpdate final : public l2d::Component
    {
    public:
        explicit ClearAndCreateSceneOnUpdate(
            l2d::SceneManager& sceneManager
        )
            : m_sceneManager(&sceneManager)
        {
        }

        void onUpdate(float) override
        {
            m_sceneManager->clear();
            m_sceneManager->createScene("Replacement");
        }

    private:
        l2d::SceneManager* m_sceneManager;
    };

    class ActivateObjectOnce final : public l2d::Component
    {
    public:
        explicit ActivateObjectOnce(l2d::GameObject& target)
            : m_target(&target)
        {
        }

        void onUpdate(float) override
        {
            if (m_activated)
                return;

            m_target->setActive(true);
            m_activated = true;
        }

    private:
        l2d::GameObject* m_target;
        bool m_activated = false;
    };

    class ReenterFixedUpdateOnce final : public l2d::Component
    {
    public:
        explicit ReenterFixedUpdateOnce(l2d::Scene& scene)
            : m_scene(&scene)
        {
        }

        void onUpdate(float deltaTime) override
        {
            if (m_reentered)
                return;

            m_reentered = true;
            m_scene->fixedUpdate(deltaTime);
        }

    private:
        l2d::Scene* m_scene;
        bool m_reentered = false;
    };

    class DestroyAndSweepOnUpdate final : public l2d::Component
    {
    public:
        explicit DestroyAndSweepOnUpdate(l2d::Scene& scene)
            : m_scene(&scene)
        {
        }

        void onUpdate(float) override
        {
            if (owner() != nullptr)
                owner()->destroy();

            m_scene->destroyQueuedGameObjects();
        }

    private:
        l2d::Scene* m_scene;
    };

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

    class MoveOtherTransformOnce final : public l2d::Component
    {
    public:
        explicit MoveOtherTransformOnce(l2d::GameObject& target)
            : m_target(&target)
        {
        }

        void onUpdate(float) override
        {
            if (m_moved)
                return;

            m_target->transform.move({ 10.f, 0.f });
            m_moved = true;
        }

    private:
        l2d::GameObject* m_target;
        bool m_moved = false;
    };

    class SpawnRigidBodyOnce final : public l2d::Component
    {
    public:
        SpawnRigidBodyOnce(
            l2d::Scene& scene,
            l2d::GameObject*& spawnedObject,
            int& spawnedUpdates
        )
            : m_scene(&scene),
            m_spawnedObject(&spawnedObject),
            m_spawnedUpdates(&spawnedUpdates)
        {
        }

        void onUpdate(float) override
        {
            if (*m_spawnedObject != nullptr)
                return;

            l2d::GameObject& spawned =
                m_scene->createGameObject("SpawnedBody");

            spawned.addComponent<CounterComponent>(*m_spawnedUpdates);
            l2d::RigidBody2D& body =
                spawned.addComponent<l2d::RigidBody2D>();
            body.setVelocity({ 10.f, 0.f });

            *m_spawnedObject = &spawned;
        }

    private:
        l2d::Scene* m_scene;
        l2d::GameObject** m_spawnedObject;
        int* m_spawnedUpdates;
    };

    class ApplyForceOnceComponent final : public l2d::Component
    {
    public:
        explicit ApplyForceOnceComponent(sf::Vector2f force)
            : m_force(force)
        {
        }

        void onUpdate(float) override
        {
            if (m_applied || owner() == nullptr)
                return;

            l2d::RigidBody2D* rigidBody =
                owner()->getComponent<l2d::RigidBody2D>();

            if (rigidBody == nullptr)
                return;

            rigidBody->addForce(m_force);
            m_applied = true;
        }

    private:
        sf::Vector2f m_force;
        bool m_applied = false;
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

    void testSceneSnapshotsAllTransformsBeforeComponentUpdates()
    {
        l2d::Scene scene;
        l2d::GameObject& mutator = scene.createGameObject("Mutator");
        l2d::GameObject& target = scene.createGameObject("Target");

        target.transform.setPosition({ 5.f, 0.f });
        mutator.addComponent<MoveOtherTransformOnce>(target);

        scene.fixedUpdate(0.125f);

        L2D_REQUIRE(approximatelyEqual(
            target.transform.interpolated(0.f).position.x,
            5.f
        ));
        L2D_REQUIRE(approximatelyEqual(
            target.transform.interpolated(1.f).position.x,
            15.f
        ));
    }

    void testComponentMutationIsDeferredUntilNextUpdate()
    {
        int updates = 0;
        l2d::GameObject gameObject;
        gameObject.addComponent<AppendComponentOnce>(updates);

        gameObject.update(1.f / 60.f);
        L2D_REQUIRE(updates == 0);

        gameObject.update(1.f / 60.f);
        L2D_REQUIRE(updates == 1);
    }

    void testDeactivationStopsRemainingComponents()
    {
        int updates = 0;
        l2d::GameObject gameObject;
        gameObject.addComponent<DeactivateOwner>();
        gameObject.addComponent<CounterComponent>(updates);

        gameObject.update(1.f / 60.f);

        L2D_REQUIRE(!gameObject.isActive());
        L2D_REQUIRE(updates == 0);
    }

    void testSceneMutationAndDeferredClearAreSafe()
    {
        int spawnedUpdates = 0;
        l2d::Scene spawningScene;
        l2d::GameObject& spawner = spawningScene.createGameObject("Spawner");
        spawner.addComponent<SpawnObjectOnce>(spawningScene, spawnedUpdates);

        spawningScene.update(1.f / 60.f);
        L2D_REQUIRE(spawningScene.gameObjectCount() == 2);
        L2D_REQUIRE(spawnedUpdates == 0);

        spawningScene.update(1.f / 60.f);
        L2D_REQUIRE(spawnedUpdates == 1);

        int shouldNotRun = 0;
        l2d::Scene clearingScene;
        l2d::GameObject& clearer = clearingScene.createGameObject("Clearer");
        clearer.addComponent<ClearSceneOnUpdate>(clearingScene);
        clearer.addComponent<CounterComponent>(shouldNotRun);
        clearingScene.createGameObject("Later")
            .addComponent<CounterComponent>(shouldNotRun);

        clearingScene.update(1.f / 60.f);
        L2D_REQUIRE(clearingScene.gameObjectCount() == 0);
        L2D_REQUIRE(shouldNotRun == 0);

        l2d::Scene sweepingScene;
        sweepingScene.createGameObject("SelfDestroyer")
            .addComponent<DestroyAndSweepOnUpdate>(sweepingScene);

        sweepingScene.update(1.f / 60.f);
        L2D_REQUIRE(sweepingScene.gameObjectCount() == 0);
    }

    void testSceneManagerClearIsDeferredDuringDispatch()
    {
        l2d::SceneManager sceneManager;
        l2d::Scene& scene = sceneManager.createScene("Active");

        int shouldNotRun = 0;

        l2d::GameObject& clearer =
            scene.createGameObject("ManagerClearer");
        const l2d::GameObjectHandle handle = scene.createHandle(clearer);

        clearer.addComponent<ClearSceneManagerOnUpdate>(sceneManager);
        clearer.addComponent<CounterComponent>(shouldNotRun);

        scene.createGameObject("Later")
            .addComponent<CounterComponent>(shouldNotRun);

        sceneManager.fixedUpdate(1.f / 60.f);

        L2D_REQUIRE(shouldNotRun == 0);
        L2D_REQUIRE(sceneManager.sceneCount() == 0);
        L2D_REQUIRE(sceneManager.activeScene() == nullptr);
        L2D_REQUIRE(!handle.isValid());
    }

    void testDeferredManagerClearResetsReplacementActiveScene()
    {
        l2d::SceneManager sceneManager;
        l2d::Scene& scene = sceneManager.createScene("Active");

        scene.createGameObject("ManagerClearer")
            .addComponent<ClearAndCreateSceneOnUpdate>(sceneManager);

        sceneManager.fixedUpdate(1.f / 60.f);

        L2D_REQUIRE(sceneManager.sceneCount() == 0);
        L2D_REQUIRE(sceneManager.activeScene() == nullptr);

        sceneManager.fixedUpdate(1.f / 60.f);
    }

    void testManagerClearIsSafeDuringDirectSceneDispatch()
    {
        l2d::SceneManager sceneManager;
        l2d::Scene& scene = sceneManager.createScene("Active");
        l2d::GameObject& clearer =
            scene.createGameObject("ManagerClearer");
        const l2d::GameObjectHandle handle = scene.createHandle(clearer);

        clearer.addComponent<ClearSceneManagerOnUpdate>(sceneManager);

        scene.fixedUpdate(1.f / 60.f);

        L2D_REQUIRE(sceneManager.sceneCount() == 0);
        L2D_REQUIRE(sceneManager.activeScene() == nullptr);
        L2D_REQUIRE(!handle.isValid());
    }

    void testActivationJoinsFixedPhasesOnTheNextTick()
    {
        for (bool activatorFirst : { false, true })
        {
            l2d::Scene scene;
            l2d::PhysicsWorld2D world;

            l2d::GameObject* activator = nullptr;
            l2d::GameObject* target = nullptr;

            if (activatorFirst)
            {
                activator = &scene.createGameObject("Activator");
                target = &scene.createGameObject("Target");
            }
            else
            {
                target = &scene.createGameObject("Target");
                activator = &scene.createGameObject("Activator");
            }

            int targetUpdates = 0;

            target->setActive(false);
            target->addComponent<CounterComponent>(targetUpdates);

            l2d::RigidBody2D& body =
                target->addComponent<l2d::RigidBody2D>();
            body.setVelocity({ 10.f, 0.f });

            activator->addComponent<ActivateObjectOnce>(*target);

            scene.fixedUpdate(0.5f);
            world.step(scene, 0.5f);

            L2D_REQUIRE(target->isActive());
            L2D_REQUIRE(targetUpdates == 0);
            L2D_REQUIRE(approximatelyEqual(
                target->transform.position().x,
                0.f
            ));

            scene.fixedUpdate(0.5f);
            world.step(scene, 0.5f);

            L2D_REQUIRE(targetUpdates == 1);
            L2D_REQUIRE(approximatelyEqual(
                target->transform.position().x,
                5.f
            ));
        }
    }

    void testFixedUpdateIsNonReentrant()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world;
        l2d::GameObject& activator =
            scene.createGameObject("Activator");
        l2d::GameObject& target = scene.createGameObject("Target");
        int targetUpdates = 0;

        target.setActive(false);
        target.addComponent<CounterComponent>(targetUpdates);
        l2d::RigidBody2D& body =
            target.addComponent<l2d::RigidBody2D>();
        body.setVelocity({ 10.f, 0.f });

        activator.addComponent<ActivateObjectOnce>(target);
        activator.addComponent<ReenterFixedUpdateOnce>(scene);

        scene.fixedUpdate(0.5f);
        world.step(scene, 0.5f);

        L2D_REQUIRE(target.isActive());
        L2D_REQUIRE(targetUpdates == 0);
        L2D_REQUIRE(approximatelyEqual(
            target.transform.position().x,
            0.f
        ));

        scene.fixedUpdate(0.5f);
        world.step(scene, 0.5f);

        L2D_REQUIRE(targetUpdates == 1);
        L2D_REQUIRE(approximatelyEqual(
            target.transform.position().x,
            5.f
        ));
    }

    void testHandlesExpireWithTheirScene()
    {
        l2d::GameObjectHandle handle;

        {
            l2d::Scene scene;
            l2d::GameObject& object = scene.createGameObject("Handled");
            handle = scene.createHandle(object);

            L2D_REQUIRE(handle.isValid());
            L2D_REQUIRE(handle.get() == &object);
            L2D_REQUIRE(handle.scene() == &scene);
        }

        L2D_REQUIRE(!handle.isValid());
        L2D_REQUIRE(handle.get() == nullptr);
        L2D_REQUIRE(handle.scene() == nullptr);
    }

    void testQueuedObjectsAreNotReportedAsActive()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Queued");
        object.setTag("target");
        object.destroy();

        L2D_REQUIRE(scene.gameObjectCount() == 1);
        L2D_REQUIRE(scene.activeGameObjectCount() == 0);
        L2D_REQUIRE(scene.findGameObjectById(object.id()) == nullptr);
        L2D_REQUIRE(scene.findGameObjectByName("Queued") == nullptr);
        L2D_REQUIRE(scene.countGameObjectsByTag("target") == 0);
        L2D_REQUIRE(scene.findGameObjectsByTag("target").empty());
        L2D_REQUIRE(scene.countActiveGameObjectsByTag("target") == 0);
        L2D_REQUIRE(scene.findActiveGameObjectsByTag("target").empty());
    }

    void testPhysicsInputsAreFiniteAndValid()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::BoxCollider2D box({ -10.f, nan });
        L2D_REQUIRE(std::isfinite(box.size().x));
        L2D_REQUIRE(std::isfinite(box.size().y));
        L2D_REQUIRE(box.size().x > 0.f);
        L2D_REQUIRE(box.size().y > 0.f);

        box.setSize({ infinity, 0.f });
        L2D_REQUIRE(std::isfinite(box.size().x));
        L2D_REQUIRE(std::isfinite(box.size().y));
        L2D_REQUIRE(box.size().x > 0.f);
        L2D_REQUIRE(box.size().y > 0.f);

        l2d::CircleCollider2D circle(-20.f);
        L2D_REQUIRE(circle.radius() == 0.f);
        circle.setRadius(nan);
        L2D_REQUIRE(circle.radius() == 0.f);

        l2d::RigidBody2D rigidBody;
        rigidBody.setMass(0.0000001f);
        L2D_REQUIRE(std::isfinite(rigidBody.mass()));
        L2D_REQUIRE(rigidBody.mass() >= 0.0001f);
        rigidBody.setMass(nan);
        L2D_REQUIRE(std::isfinite(rigidBody.mass()));
        L2D_REQUIRE(rigidBody.mass() >= 0.0001f);
    }

    struct FloorContactFixture
    {
        l2d::Scene scene;
        l2d::GameObject& mover;
        l2d::RigidBody2D& body;
        l2d::CircleCollider2D& circle;
        l2d::GameObject& floor;
        l2d::BoxCollider2D& box;

        FloorContactFixture()
            : mover(scene.createGameObject("Mover")),
            body(mover.addComponent<l2d::RigidBody2D>()),
            circle(mover.addComponent<l2d::CircleCollider2D>(10.f)),
            floor(scene.createGameObject("Floor")),
            box(floor.addComponent<l2d::BoxCollider2D>(sf::Vector2f{ 20.f, 20.f }))
        {
            mover.transform.setPosition({ 0.f, 0.f });
            floor.transform.setPosition({ 0.f, 19.f });
        }
    };

    struct PhysicsSimulationResult
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        std::uint64_t tickCount = 0;
    };

    PhysicsSimulationResult simulatePhysicsFrames(
        const std::vector<double>& frameDeltas
    )
    {
        l2d::FixedStepConfig config;
        config.fixedDeltaTime = 0.125;
        config.maximumFrameDeltaTime = 1.0;
        config.maximumTicksPerFrame = 8;

        l2d::FixedStepScheduler scheduler(config);
        l2d::PhysicsWorld2D world;
        l2d::Scene scene;

        l2d::GameObject& object = scene.createGameObject("Simulated");
        l2d::RigidBody2D& body = object.addComponent<l2d::RigidBody2D>();
        body.setVelocity({ 1.f, -2.f });
        body.setAcceleration({ 4.f, 8.f });

        for (double frameDelta : frameDeltas)
        {
            const l2d::FixedStepFrame frame = scheduler.advance(frameDelta);

            for (std::uint32_t tick = 0; tick < frame.ticksToRun; ++tick)
            {
                scene.fixedUpdate(static_cast<float>(config.fixedDeltaTime));
                world.step(scene, static_cast<float>(config.fixedDeltaTime));
            }
        }

        return {
            object.transform.position(),
            body.velocity(),
            scheduler.tickCount()
        };
    }

    void testPhysicsWorldIntegratesOncePerFixedTick()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world;

        l2d::GameObject& bodyFirst = scene.createGameObject("BodyFirst");
        l2d::RigidBody2D& firstBody =
            bodyFirst.addComponent<l2d::RigidBody2D>();
        firstBody.setMass(2.f);
        bodyFirst.addComponent<ApplyForceOnceComponent>(
            sf::Vector2f{ 4.f, 8.f }
        );

        l2d::GameObject& forceFirst = scene.createGameObject("ForceFirst");
        forceFirst.addComponent<ApplyForceOnceComponent>(
            sf::Vector2f{ 4.f, 8.f }
        );
        l2d::RigidBody2D& secondBody =
            forceFirst.addComponent<l2d::RigidBody2D>();
        secondBody.setMass(2.f);

        scene.fixedUpdate(0.5f);

        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().x, 0.f));
        L2D_REQUIRE(approximatelyEqual(forceFirst.transform.position().x, 0.f));

        world.step(scene, 0.5f);

        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(
            bodyFirst.transform.position().x,
            0.5f
        ));
        L2D_REQUIRE(approximatelyEqual(
            bodyFirst.transform.position().y,
            1.f
        ));

        L2D_REQUIRE(approximatelyEqual(
            secondBody.velocity().x,
            firstBody.velocity().x
        ));
        L2D_REQUIRE(approximatelyEqual(
            secondBody.velocity().y,
            firstBody.velocity().y
        ));
        L2D_REQUIRE(approximatelyEqual(
            forceFirst.transform.position().x,
            bodyFirst.transform.position().x
        ));
        L2D_REQUIRE(approximatelyEqual(
            forceFirst.transform.position().y,
            bodyFirst.transform.position().y
        ));

        scene.fixedUpdate(0.5f);
        world.step(scene, 0.5f);

        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(
            secondBody.velocity().x,
            firstBody.velocity().x
        ));
        L2D_REQUIRE(approximatelyEqual(
            secondBody.velocity().y,
            firstBody.velocity().y
        ));
    }

    void testSpawnedBodiesJoinPhysicsOnTheNextFixedTick()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world;
        l2d::GameObject* spawned = nullptr;
        int spawnedUpdates = 0;

        l2d::GameObject& spawner = scene.createGameObject("Spawner");
        spawner.addComponent<SpawnRigidBodyOnce>(
            scene,
            spawned,
            spawnedUpdates
        );

        scene.fixedUpdate(0.5f);

        L2D_REQUIRE(spawned != nullptr);
        L2D_REQUIRE(spawnedUpdates == 0);
        L2D_REQUIRE(approximatelyEqual(spawned->transform.position().x, 0.f));

        world.step(scene, 0.5f);

        L2D_REQUIRE(approximatelyEqual(spawned->transform.position().x, 0.f));

        scene.fixedUpdate(0.5f);
        world.step(scene, 0.5f);

        L2D_REQUIRE(spawnedUpdates == 1);
        L2D_REQUIRE(approximatelyEqual(spawned->transform.position().x, 5.f));
        L2D_REQUIRE(approximatelyEqual(
            spawned->transform.interpolated(0.f).position.x,
            0.f
        ));
    }

    void testFixedPhysicsIsIndependentOfRenderCadence()
    {
        const PhysicsSimulationResult steady = simulatePhysicsFrames(
            std::vector<double>(8, 0.125)
        );
        const PhysicsSimulationResult chunky = simulatePhysicsFrames(
            { 0.25, 0.5, 0.25 }
        );
        const PhysicsSimulationResult irregular = simulatePhysicsFrames(
            { 0.0625, 0.1875, 0.375, 0.375 }
        );

        L2D_REQUIRE(steady.tickCount == 8);
        L2D_REQUIRE(chunky.tickCount == steady.tickCount);
        L2D_REQUIRE(irregular.tickCount == steady.tickCount);

        L2D_REQUIRE(approximatelyEqual(chunky.position.x, steady.position.x));
        L2D_REQUIRE(approximatelyEqual(chunky.position.y, steady.position.y));
        L2D_REQUIRE(approximatelyEqual(chunky.velocity.x, steady.velocity.x));
        L2D_REQUIRE(approximatelyEqual(chunky.velocity.y, steady.velocity.y));

        L2D_REQUIRE(approximatelyEqual(irregular.position.x, steady.position.x));
        L2D_REQUIRE(approximatelyEqual(irregular.position.y, steady.position.y));
        L2D_REQUIRE(approximatelyEqual(irregular.velocity.x, steady.velocity.x));
        L2D_REQUIRE(approximatelyEqual(irregular.velocity.y, steady.velocity.y));
    }

    void testPhysicsPreservesTangentAndOutwardVelocity()
    {
        l2d::PhysicsWorld2D world;

        FloorContactFixture inward;
        inward.body.setVelocity({ 25.f, 10.f });
        inward.scene.fixedUpdate(1.f / 60.f);
        world.step(inward.scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().x, 25.f));
        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().y, 0.f));
        L2D_REQUIRE(inward.body.isGrounded());
        L2D_REQUIRE(inward.circle.isColliding());
        L2D_REQUIRE(inward.box.isColliding());
        L2D_REQUIRE(inward.mover.transform.position().y < 0.f);

        FloorContactFixture outward;
        outward.body.setVelocity({ 25.f, -10.f });
        outward.scene.fixedUpdate(1.f / 60.f);
        world.step(outward.scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(outward.body.velocity().x, 25.f));
        L2D_REQUIRE(approximatelyEqual(outward.body.velocity().y, -10.f));
        L2D_REQUIRE(outward.circle.isColliding());
        L2D_REQUIRE(outward.box.isColliding());
        L2D_REQUIRE(outward.mover.transform.position().y < 0.f);
    }

    void testInactivePhysicsComponentsDoNotParticipate()
    {
        l2d::Scene scene;
        l2d::GameObject& mover = scene.createGameObject("Mover");
        l2d::RigidBody2D& body = mover.addComponent<l2d::RigidBody2D>();
        l2d::CircleCollider2D& circle =
            mover.addComponent<l2d::CircleCollider2D>(10.f);

        l2d::GameObject& obstacle = scene.createGameObject("Obstacle");
        obstacle.transform.setPosition({ 0.f, 5.f });
        l2d::BoxCollider2D& box =
            obstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{ 20.f, 20.f });
        box.setActive(false);

        body.setVelocity({ 3.f, 4.f });
        const sf::Vector2f originalPosition = mover.transform.position();

        l2d::PhysicsWorld2D world;
        world.step(scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(
            mover.transform.position().x,
            originalPosition.x + 3.f / 60.f
        ));
        L2D_REQUIRE(approximatelyEqual(
            mover.transform.position().y,
            originalPosition.y + 4.f / 60.f
        ));
        L2D_REQUIRE(approximatelyEqual(body.velocity().x, 3.f));
        L2D_REQUIRE(approximatelyEqual(body.velocity().y, 4.f));
        L2D_REQUIRE(!circle.isColliding());
        L2D_REQUIRE(!box.isColliding());

        body.setActive(false);
        const sf::Vector2f positionBeforeInactiveStep =
            mover.transform.position();

        world.step(scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(
            mover.transform.position().x,
            positionBeforeInactiveStep.x
        ));
        L2D_REQUIRE(approximatelyEqual(
            mover.transform.position().y,
            positionBeforeInactiveStep.y
        ));
    }

    void testTileMapReloadAndUnloadOwnGeneratedTiles()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;

        tileMap.setTileSize({
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity()
        });
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.tileSize().y, 1.f));

        tileMap.setTileSize({ 10.f, 20.f });

        tileMap.loadFromLayout(scene, { "#P", "", "C#" });
        L2D_REQUIRE(tileMap.layout().size() == 3);
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, 20.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 60.f));
        L2D_REQUIRE(scene.gameObjectCount() == 2);

        sf::Vector2f playerPosition;
        L2D_REQUIRE(tileMap.findFirstTilePosition('P', playerPosition, true));
        L2D_REQUIRE(approximatelyEqual(playerPosition.x, 15.f));
        L2D_REQUIRE(approximatelyEqual(playerPosition.y, 10.f));

        tileMap.setTileSize({ 5.f, 5.f });
        L2D_REQUIRE(approximatelyEqual(tileMap.loadedTileSize().x, 10.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.loadedTileSize().y, 20.f));

        tileMap.loadFromLayout(scene, { "##" });
        L2D_REQUIRE(scene.gameObjectCount() == 4);
        L2D_REQUIRE(scene.activeGameObjectCount() == 2);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 2);
        L2D_REQUIRE(scene.findGameObjectByName("Tile_0") != nullptr);
        L2D_REQUIRE(!scene.findGameObjectByName("Tile_0")->isDestroyQueued());

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE(scene.gameObjectCount() == 2);

        tileMap.unload();
        L2D_REQUIRE(tileMap.layout().empty());
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, 0.f));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 0.f));
        L2D_REQUIRE(scene.activeGameObjectCount() == 0);

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE(scene.gameObjectCount() == 0);
    }

    void testTileMapMoveTransfersGeneratedTileOwnership()
    {
        l2d::Scene sourceScene;
        l2d::TileMap source;
        source.loadFromLayout(sourceScene, { "#" });

        l2d::TileMap moved(std::move(source));
        L2D_REQUIRE(source.layout().empty());
        L2D_REQUIRE(moved.layout().size() == 1);
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 1);

        l2d::Scene destinationScene;
        l2d::TileMap destination;
        destination.loadFromLayout(destinationScene, { "#" });
        destination = std::move(moved);

        L2D_REQUIRE(destinationScene.activeGameObjectCount() == 0);
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 1);
        L2D_REQUIRE(moved.layout().empty());
        L2D_REQUIRE(destination.layout().size() == 1);

        destinationScene.destroyQueuedGameObjects();
        L2D_REQUIRE(destinationScene.gameObjectCount() == 0);

        destination.unload();
        L2D_REQUIRE(sourceScene.activeGameObjectCount() == 0);
        sourceScene.destroyQueuedGameObjects();
        L2D_REQUIRE(sourceScene.gameObjectCount() == 0);
    }

    void testTileMapFileLoadingPreservesBlankRowsAndIsTransactional()
    {
        const TemporaryFile temporaryFile("lorenzo2d_blank_rows_test");
        const std::filesystem::path& path = temporaryFile.path();

        {
            std::ofstream file(path, std::ios::trunc);
            file << "#\n\n#\n";
        }

        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({ 10.f, 20.f });

        L2D_REQUIRE(tileMap.loadFromFile(scene, path.string()));
        L2D_REQUIRE(tileMap.layout().size() == 3);
        L2D_REQUIRE(tileMap.layout()[1].empty());
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, 60.f));

        const l2d::TileMap::Layout previousLayout = tileMap.layout();
        const sf::Vector2f previousWorldSize = tileMap.worldSize();
        const std::size_t previousObjectCount = scene.gameObjectCount();

        L2D_REQUIRE(!tileMap.loadFromFile(scene, path.string() + ".missing"));
        L2D_REQUIRE(tileMap.layout() == previousLayout);
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().x, previousWorldSize.x));
        L2D_REQUIRE(approximatelyEqual(tileMap.worldSize().y, previousWorldSize.y));
        L2D_REQUIRE(scene.gameObjectCount() == previousObjectCount);

    }

    template <typename Function>
    void runTest(const char* name, Function&& function, int& failures)
    {
        try
        {
            std::forward<Function>(function)();
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& exception)
        {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
        }
    }
}

int main()
{
    int failures = 0;

    runTest("identity types cannot be moved or copied", testIdentityTypesCannotBeMovedOrCopied, failures);
    runTest("fixed scheduler accumulates exact substeps", testFixedStepSchedulerAccumulatesExactSubsteps, failures);
    runTest("fixed scheduler snaps floating boundaries", testFixedStepSchedulerSnapsFloatingPointBoundaries, failures);
    runTest("fixed scheduler bounds catch-up and recovers", testFixedStepSchedulerBoundsCatchUpAndRecovers, failures);
    runTest("fixed scheduler sanitizes invalid input", testFixedStepSchedulerSanitizesInvalidInputs, failures);
    runTest("transforms interpolate fixed snapshots", testTransformInterpolationTracksFixedSnapshots, failures);
    runTest("scenes snapshot transforms before updates", testSceneSnapshotsAllTransformsBeforeComponentUpdates, failures);
    runTest("component mutation is deferred", testComponentMutationIsDeferredUntilNextUpdate, failures);
    runTest("deactivation stops remaining components", testDeactivationStopsRemainingComponents, failures);
    runTest("scene mutation and deferred clear are safe", testSceneMutationAndDeferredClearAreSafe, failures);
    runTest("scene manager clear is dispatch-safe", testSceneManagerClearIsDeferredDuringDispatch, failures);
    runTest("deferred manager clear resets replacement active scene",
        testDeferredManagerClearResetsReplacementActiveScene, failures);
    runTest("manager clear is safe during direct scene dispatch",
        testManagerClearIsSafeDuringDirectSceneDispatch, failures);
    runTest("activation joins all fixed phases next tick", testActivationJoinsFixedPhasesOnTheNextTick, failures);
    runTest("fixed updates are non-reentrant",
        testFixedUpdateIsNonReentrant, failures);
    runTest("handles expire with their scene", testHandlesExpireWithTheirScene, failures);
    runTest("queued objects are not active", testQueuedObjectsAreNotReportedAsActive, failures);
    runTest("physics inputs are finite and valid", testPhysicsInputsAreFiniteAndValid, failures);
    runTest("physics integrates once per fixed tick", testPhysicsWorldIntegratesOncePerFixedTick, failures);
    runTest("spawned bodies join physics next tick", testSpawnedBodiesJoinPhysicsOnTheNextFixedTick, failures);
    runTest("fixed physics ignores render cadence", testFixedPhysicsIsIndependentOfRenderCadence, failures);
    runTest("physics preserves tangent and outward velocity", testPhysicsPreservesTangentAndOutwardVelocity, failures);
    runTest("inactive physics components do not participate", testInactivePhysicsComponentsDoNotParticipate, failures);
    runTest("tilemap reload and unload own generated tiles", testTileMapReloadAndUnloadOwnGeneratedTiles, failures);
    runTest("tilemap move transfers ownership", testTileMapMoveTransfersGeneratedTileOwnership, failures);
    runTest("tilemap files preserve blank rows", testTileMapFileLoadingPreservesBlankRowsAndIsTransactional, failures);

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D tests passed.\n";
    return 0;
}

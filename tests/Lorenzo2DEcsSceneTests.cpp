#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Scene/SceneManager.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::approximatelyEqual;
    using l2d::test::runTest;

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

    class DestroyTargetOnDestruction final : public l2d::Component
    {
    public:
        explicit DestroyTargetOnDestruction(l2d::GameObject& target)
            : m_target(&target)
        {
        }

        ~DestroyTargetOnDestruction() override
        {
            if (m_target != nullptr)
                m_target->destroy();
        }

    private:
        l2d::GameObject* m_target;
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
        const l2d::GameObjectId clearerId = clearer.id();
        const l2d::GameObjectHandle clearerHandle =
            clearingScene.createHandle(clearer);
        clearer.addComponent<ClearSceneOnUpdate>(clearingScene);
        clearer.addComponent<CounterComponent>(shouldNotRun);

        l2d::GameObject& later =
            clearingScene.createGameObject("Later");
        const l2d::GameObjectId laterId = later.id();
        const l2d::GameObjectHandle laterHandle =
            clearingScene.createHandle(later);
        later.addComponent<CounterComponent>(shouldNotRun);

        clearingScene.update(1.f / 60.f);
        L2D_REQUIRE(clearingScene.gameObjectCount() == 0);
        L2D_REQUIRE(shouldNotRun == 0);
        L2D_REQUIRE(clearingScene.findGameObjectById(clearerId) == nullptr);
        L2D_REQUIRE(clearingScene.findGameObjectById(laterId) == nullptr);
        L2D_REQUIRE(!clearerHandle.isValid());
        L2D_REQUIRE(!laterHandle.isValid());

        l2d::GameObject& replacement =
            clearingScene.createGameObject("Replacement");
        L2D_REQUIRE(
            clearingScene.findGameObjectById(replacement.id()) ==
            &replacement
        );

        l2d::Scene sweepingScene;
        l2d::GameObject& selfDestroyer =
            sweepingScene.createGameObject("SelfDestroyer");
        const l2d::GameObjectId selfDestroyerId = selfDestroyer.id();
        const l2d::GameObjectHandle selfDestroyerHandle =
            sweepingScene.createHandle(selfDestroyer);
        selfDestroyer.addComponent<DestroyAndSweepOnUpdate>(sweepingScene);

        sweepingScene.update(1.f / 60.f);
        L2D_REQUIRE(sweepingScene.gameObjectCount() == 0);
        L2D_REQUIRE(
            sweepingScene.findGameObjectById(selfDestroyerId) == nullptr
        );
        L2D_REQUIRE(!selfDestroyerHandle.isValid());
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

    void testSceneIdIndexTracksOwnedObjectLifetime()
    {
        constexpr std::size_t objectCount = 1024;

        l2d::Scene scene;
        std::vector<l2d::GameObjectId> ids;
        std::vector<l2d::GameObject*> objects;
        std::vector<l2d::GameObjectHandle> handles;

        ids.reserve(objectCount);
        objects.reserve(objectCount);
        handles.reserve(objectCount);

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            l2d::GameObject& object = scene.createGameObject(
                "Indexed_" + std::to_string(index)
            );

            ids.push_back(object.id());
            objects.push_back(&object);
            handles.push_back(scene.createHandle(object));
        }

        L2D_REQUIRE(scene.gameObjectCount() == objectCount);
        L2D_REQUIRE(scene.gameObjects().size() == objectCount);
        L2D_REQUIRE(
            scene.findGameObjectById(l2d::InvalidGameObjectId) == nullptr
        );

        const l2d::Scene& constScene = scene;
        L2D_REQUIRE(
            constScene.findGameObjectById(l2d::InvalidGameObjectId) ==
            nullptr
        );

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            L2D_REQUIRE(scene.gameObjects()[index].get() == objects[index]);
            L2D_REQUIRE(scene.findGameObjectById(ids[index]) == objects[index]);
            L2D_REQUIRE(
                constScene.findGameObjectById(ids[index]) == objects[index]
            );
            L2D_REQUIRE(handles[index].get() == objects[index]);
            L2D_REQUIRE(
                scene.createHandle(ids[index]).get() == objects[index]
            );
        }

        l2d::GameObject standaloneObject("Standalone");
        const l2d::GameObjectHandle standaloneHandle =
            scene.createHandle(standaloneObject);

        L2D_REQUIRE(!standaloneHandle.isValid());
        L2D_REQUIRE(standaloneHandle.scene() == nullptr);
        L2D_REQUIRE(
            scene.findGameObjectById(standaloneObject.id()) == nullptr
        );

        scene.destroyGameObject(standaloneObject);
        L2D_REQUIRE(!standaloneObject.isDestroyQueued());

        l2d::Scene foreignScene;
        l2d::GameObject& foreignObject =
            foreignScene.createGameObject("Foreign");

        L2D_REQUIRE(!scene.createHandle(foreignObject).isValid());
        scene.destroyGameObject(foreignObject);
        L2D_REQUIRE(!foreignObject.isDestroyQueued());
        L2D_REQUIRE(
            foreignScene.findGameObjectById(foreignObject.id()) ==
            &foreignObject
        );

        std::vector<bool> queued(objectCount, false);
        std::size_t queuedCount = 0;

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            if (index % 4 == 0)
            {
                objects[index]->destroy();
            }
            else if (index % 4 == 1)
            {
                scene.destroyGameObject(*objects[index]);
            }
            else
            {
                continue;
            }

            queued[index] = true;
            ++queuedCount;
        }

        L2D_REQUIRE(scene.gameObjectCount() == objectCount);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == queuedCount);

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            if (queued[index])
            {
                L2D_REQUIRE(scene.findGameObjectById(ids[index]) == nullptr);
                L2D_REQUIRE(
                    constScene.findGameObjectById(ids[index]) == nullptr
                );
                L2D_REQUIRE(!handles[index].isValid());
                L2D_REQUIRE(!scene.createHandle(ids[index]).isValid());
                L2D_REQUIRE(!scene.createHandle(*objects[index]).isValid());
            }
            else
            {
                L2D_REQUIRE(
                    scene.findGameObjectById(ids[index]) == objects[index]
                );
                L2D_REQUIRE(handles[index].get() == objects[index]);
            }
        }

        scene.destroyQueuedGameObjects();

        L2D_REQUIRE(scene.gameObjectCount() == objectCount - queuedCount);
        L2D_REQUIRE(scene.destroyQueuedGameObjectCount() == 0);

        std::size_t survivorIndex = 0;

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            if (queued[index])
            {
                L2D_REQUIRE(scene.findGameObjectById(ids[index]) == nullptr);
                L2D_REQUIRE(!handles[index].isValid());
                continue;
            }

            L2D_REQUIRE(
                scene.gameObjects()[survivorIndex].get() == objects[index]
            );
            L2D_REQUIRE(
                constScene.findGameObjectById(ids[index]) == objects[index]
            );
            L2D_REQUIRE(handles[index].get() == objects[index]);
            ++survivorIndex;
        }

        L2D_REQUIRE(survivorIndex == scene.gameObjectCount());

        scene.clear();

        L2D_REQUIRE(scene.gameObjectCount() == 0);

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            L2D_REQUIRE(scene.findGameObjectById(ids[index]) == nullptr);
            L2D_REQUIRE(!handles[index].isValid());
        }

        l2d::GameObject& replacement =
            scene.createGameObject("Replacement");
        const l2d::GameObjectHandle replacementHandle =
            scene.createHandle(replacement.id());

        L2D_REQUIRE(scene.gameObjectCount() == 1);
        L2D_REQUIRE(scene.findGameObjectById(replacement.id()) == &replacement);
        L2D_REQUIRE(
            constScene.findGameObjectById(replacement.id()) == &replacement
        );
        L2D_REQUIRE(replacementHandle.get() == &replacement);

        for (const l2d::GameObjectHandle& handle : handles)
        {
            L2D_REQUIRE(!handle.isValid());
        }
    }

    void testSceneIndexTracksObjectsQueuedDuringSweep()
    {
        l2d::Scene scene;
        l2d::GameObject& first = scene.createGameObject("FirstRemoved");
        l2d::GameObject& survivor = scene.createGameObject("Survivor");
        l2d::GameObject& queuedByDestructor =
            scene.createGameObject("QueuedByDestructor");

        const l2d::GameObjectId firstId = first.id();
        const l2d::GameObjectId survivorId = survivor.id();
        const l2d::GameObjectId queuedId = queuedByDestructor.id();
        const l2d::GameObjectHandle firstHandle = scene.createHandle(first);
        const l2d::GameObjectHandle queuedHandle =
            scene.createHandle(queuedByDestructor);

        first.addComponent<DestroyTargetOnDestruction>(queuedByDestructor);
        first.destroy();

        scene.destroyQueuedGameObjects();

        L2D_REQUIRE(scene.gameObjectCount() == 1);
        L2D_REQUIRE(scene.findGameObjectById(firstId) == nullptr);
        L2D_REQUIRE(scene.findGameObjectById(queuedId) == nullptr);
        L2D_REQUIRE(scene.findGameObjectById(survivorId) == &survivor);
        L2D_REQUIRE(!firstHandle.isValid());
        L2D_REQUIRE(!queuedHandle.isValid());

        constexpr std::size_t replacementCount = 128;

        for (std::size_t index = 0; index < replacementCount; ++index)
        {
            l2d::GameObject& replacement = scene.createGameObject(
                "ReplacementAfterSweep_" + std::to_string(index)
            );

            L2D_REQUIRE(replacement.id() != queuedId);
            L2D_REQUIRE(scene.findGameObjectById(queuedId) == nullptr);
            L2D_REQUIRE(!queuedHandle.isValid());
            L2D_REQUIRE(
                scene.findGameObjectById(replacement.id()) == &replacement
            );
        }

        L2D_REQUIRE(scene.gameObjectCount() == replacementCount + 1);
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
}

int main()
{
    int failures = 0;

    runTest("scenes snapshot transforms before updates",
        testSceneSnapshotsAllTransformsBeforeComponentUpdates, failures);
    runTest("component mutation is deferred", testComponentMutationIsDeferredUntilNextUpdate, failures);
    runTest("deactivation stops remaining components", testDeactivationStopsRemainingComponents, failures);
    runTest("scene mutation and deferred clear are safe",
        testSceneMutationAndDeferredClearAreSafe, failures);
    runTest("scene manager clear is dispatch-safe",
        testSceneManagerClearIsDeferredDuringDispatch, failures);
    runTest("deferred manager clear resets replacement active scene",
        testDeferredManagerClearResetsReplacementActiveScene, failures);
    runTest("manager clear is safe during direct scene dispatch",
        testManagerClearIsSafeDuringDirectSceneDispatch, failures);
    runTest("activation joins all fixed phases next tick",
        testActivationJoinsFixedPhasesOnTheNextTick, failures);
    runTest("fixed updates are non-reentrant", testFixedUpdateIsNonReentrant, failures);
    runTest("handles expire with their scene", testHandlesExpireWithTheirScene, failures);
    runTest("scene ID index tracks owned object lifetime",
        testSceneIdIndexTracksOwnedObjectLifetime, failures);
    runTest("scene index tracks objects queued during sweep",
        testSceneIndexTracksObjectsQueuedDuringSweep, failures);
    runTest("queued objects are not active", testQueuedObjectsAreNotReportedAsActive, failures);

    if (failures != 0)
    {
        std::cerr << failures << " ECS and scene test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D ECS and scene tests passed.\n";
    return 0;
}

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

    void testPhysicsPreservesTangentAndOutwardVelocity()
    {
        l2d::PhysicsWorld2D world;

        FloorContactFixture inward;
        inward.body.setVelocity({ 25.f, 10.f });
        world.step(inward.scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().x, 25.f));
        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().y, 0.f));
        L2D_REQUIRE(inward.body.isGrounded());
        L2D_REQUIRE(inward.circle.isColliding());
        L2D_REQUIRE(inward.box.isColliding());
        L2D_REQUIRE(inward.mover.transform.position().y < 0.f);

        FloorContactFixture outward;
        outward.body.setVelocity({ 25.f, -10.f });
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

        L2D_REQUIRE(approximatelyEqual(mover.transform.position().x, originalPosition.x));
        L2D_REQUIRE(approximatelyEqual(mover.transform.position().y, originalPosition.y));
        L2D_REQUIRE(approximatelyEqual(body.velocity().x, 3.f));
        L2D_REQUIRE(approximatelyEqual(body.velocity().y, 4.f));
        L2D_REQUIRE(!circle.isColliding());
        L2D_REQUIRE(!box.isColliding());
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
    runTest("component mutation is deferred", testComponentMutationIsDeferredUntilNextUpdate, failures);
    runTest("deactivation stops remaining components", testDeactivationStopsRemainingComponents, failures);
    runTest("scene mutation and deferred clear are safe", testSceneMutationAndDeferredClearAreSafe, failures);
    runTest("handles expire with their scene", testHandlesExpireWithTheirScene, failures);
    runTest("queued objects are not active", testQueuedObjectsAreNotReportedAsActive, failures);
    runTest("physics inputs are finite and valid", testPhysicsInputsAreFiniteAndValid, failures);
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

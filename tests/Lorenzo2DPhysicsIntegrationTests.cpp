#include <Lorenzo2D/Core/FixedStepScheduler.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <limits>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::approximatelyEqual;
    using l2d::test::runTest;

    class CounterComponent final : public l2d::Component
    {
      public:
        explicit CounterComponent(int& counter) : m_counter(&counter) {}

        void onUpdate(float) override
        {
            ++(*m_counter);
        }

      private:
        int* m_counter;
    };

    class SpawnRigidBodyOnce final : public l2d::Component
    {
      public:
        SpawnRigidBodyOnce(l2d::Scene& scene, l2d::GameObject*& spawnedObject, int& spawnedUpdates)
            : m_scene(&scene), m_spawnedObject(&spawnedObject), m_spawnedUpdates(&spawnedUpdates)
        {
        }

        void onUpdate(float) override
        {
            if (*m_spawnedObject != nullptr) return;

            l2d::GameObject& spawned = m_scene->createGameObject("SpawnedBody");

            spawned.addComponent<CounterComponent>(*m_spawnedUpdates);
            l2d::RigidBody2D& body = spawned.addComponent<l2d::RigidBody2D>();
            body.setVelocity({10.f, 0.f});

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
        explicit ApplyForceOnceComponent(sf::Vector2f force) : m_force(force) {}

        void onUpdate(float) override
        {
            if (m_applied || owner() == nullptr) return;

            l2d::RigidBody2D* rigidBody = owner()->getComponent<l2d::RigidBody2D>();

            if (rigidBody == nullptr) return;

            rigidBody->addForce(m_force);
            m_applied = true;
        }

      private:
        sf::Vector2f m_force;
        bool m_applied = false;
    };

    void testPhysicsInputsAreFiniteAndValid()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::BoxCollider2D box({-10.f, nan});
        L2D_REQUIRE(std::isfinite(box.size().x));
        L2D_REQUIRE(std::isfinite(box.size().y));
        L2D_REQUIRE(box.size().x > 0.f);
        L2D_REQUIRE(box.size().y > 0.f);

        box.setSize({infinity, 0.f});
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
            : mover(scene.createGameObject("Mover")), body(mover.addComponent<l2d::RigidBody2D>()),
              circle(mover.addComponent<l2d::CircleCollider2D>(10.f)),
              floor(scene.createGameObject("Floor")),
              box(floor.addComponent<l2d::BoxCollider2D>(sf::Vector2f{20.f, 20.f}))
        {
            mover.transform.setPosition({0.f, 0.f});
            circle.setOffset({10.f, 10.f});
            floor.transform.setPosition({0.f, 19.f});
            box.setOffset({10.f, 10.f});
        }
    };

    struct PhysicsSimulationResult
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        std::uint64_t tickCount = 0;
    };

    PhysicsSimulationResult simulatePhysicsFrames(const std::vector<double>& frameDeltas)
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
        body.setVelocity({1.f, -2.f});
        body.setAcceleration({4.f, 8.f});

        for (double frameDelta : frameDeltas)
        {
            const l2d::FixedStepFrame frame = scheduler.advance(frameDelta);

            for (std::uint32_t tick = 0; tick < frame.ticksToRun; ++tick)
            {
                scene.fixedUpdate(static_cast<float>(config.fixedDeltaTime));
                world.step(scene, static_cast<float>(config.fixedDeltaTime));
            }
        }

        return {object.transform.position(), body.velocity(), scheduler.tickCount()};
    }

    void testPhysicsWorldIntegratesOncePerFixedTick()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world;

        l2d::GameObject& bodyFirst = scene.createGameObject("BodyFirst");
        l2d::RigidBody2D& firstBody = bodyFirst.addComponent<l2d::RigidBody2D>();
        firstBody.setMass(2.f);
        bodyFirst.addComponent<ApplyForceOnceComponent>(sf::Vector2f{4.f, 8.f});

        l2d::GameObject& forceFirst = scene.createGameObject("ForceFirst");
        forceFirst.addComponent<ApplyForceOnceComponent>(sf::Vector2f{4.f, 8.f});
        l2d::RigidBody2D& secondBody = forceFirst.addComponent<l2d::RigidBody2D>();
        secondBody.setMass(2.f);

        scene.fixedUpdate(0.5f);

        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().x, 0.f));
        L2D_REQUIRE(approximatelyEqual(forceFirst.transform.position().x, 0.f));

        world.step(scene, 0.5f);

        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().x, 0.5f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().y, 1.f));

        L2D_REQUIRE(approximatelyEqual(secondBody.velocity().x, firstBody.velocity().x));
        L2D_REQUIRE(approximatelyEqual(secondBody.velocity().y, firstBody.velocity().y));
        L2D_REQUIRE(approximatelyEqual(forceFirst.transform.position().x,
                                       bodyFirst.transform.position().x));
        L2D_REQUIRE(approximatelyEqual(forceFirst.transform.position().y,
                                       bodyFirst.transform.position().y));

        scene.fixedUpdate(0.5f);
        world.step(scene, 0.5f);

        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(firstBody.velocity().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(bodyFirst.transform.position().y, 2.f));
        L2D_REQUIRE(approximatelyEqual(secondBody.velocity().x, firstBody.velocity().x));
        L2D_REQUIRE(approximatelyEqual(secondBody.velocity().y, firstBody.velocity().y));
    }

    void testSpawnedBodiesJoinPhysicsOnTheNextFixedTick()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world;
        l2d::GameObject* spawned = nullptr;
        int spawnedUpdates = 0;

        l2d::GameObject& spawner = scene.createGameObject("Spawner");
        spawner.addComponent<SpawnRigidBodyOnce>(scene, spawned, spawnedUpdates);

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
        L2D_REQUIRE(approximatelyEqual(spawned->transform.interpolated(0.f).position.x, 0.f));
    }

    void testFixedPhysicsIsIndependentOfRenderCadence()
    {
        const PhysicsSimulationResult steady = simulatePhysicsFrames(std::vector<double>(8, 0.125));
        const PhysicsSimulationResult chunky = simulatePhysicsFrames({0.25, 0.5, 0.25});
        const PhysicsSimulationResult irregular =
            simulatePhysicsFrames({0.0625, 0.1875, 0.375, 0.375});

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
        inward.body.setVelocity({25.f, 10.f});
        inward.scene.fixedUpdate(1.f / 60.f);
        world.step(inward.scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().x, 25.f));
        L2D_REQUIRE(approximatelyEqual(inward.body.velocity().y, 0.f));
        L2D_REQUIRE(inward.body.isGrounded());
        L2D_REQUIRE(inward.circle.isColliding());
        L2D_REQUIRE(inward.box.isColliding());
        L2D_REQUIRE(inward.mover.transform.position().y < 0.f);

        FloorContactFixture outward;
        outward.body.setVelocity({25.f, -10.f});
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
        l2d::CircleCollider2D& circle = mover.addComponent<l2d::CircleCollider2D>(10.f);

        l2d::GameObject& obstacle = scene.createGameObject("Obstacle");
        obstacle.transform.setPosition({0.f, 5.f});
        l2d::BoxCollider2D& box =
            obstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{20.f, 20.f});
        box.setActive(false);

        body.setVelocity({3.f, 4.f});
        const sf::Vector2f originalPosition = mover.transform.position();

        l2d::PhysicsWorld2D world;
        world.step(scene, 1.f / 60.f);

        L2D_REQUIRE(
            approximatelyEqual(mover.transform.position().x, originalPosition.x + 3.f / 60.f));
        L2D_REQUIRE(
            approximatelyEqual(mover.transform.position().y, originalPosition.y + 4.f / 60.f));
        L2D_REQUIRE(approximatelyEqual(body.velocity().x, 3.f));
        L2D_REQUIRE(approximatelyEqual(body.velocity().y, 4.f));
        L2D_REQUIRE(!circle.isColliding());
        L2D_REQUIRE(!box.isColliding());

        body.setActive(false);
        const sf::Vector2f positionBeforeInactiveStep = mover.transform.position();

        world.step(scene, 1.f / 60.f);

        L2D_REQUIRE(approximatelyEqual(mover.transform.position().x, positionBeforeInactiveStep.x));
        L2D_REQUIRE(approximatelyEqual(mover.transform.position().y, positionBeforeInactiveStep.y));
    }
}

int main()
{
    int failures = 0;

    runTest("physics inputs are finite and valid", testPhysicsInputsAreFiniteAndValid, failures);
    runTest("physics integrates once per fixed tick", testPhysicsWorldIntegratesOncePerFixedTick,
            failures);
    runTest("spawned bodies join physics next tick", testSpawnedBodiesJoinPhysicsOnTheNextFixedTick,
            failures);
    runTest("fixed physics ignores render cadence", testFixedPhysicsIsIndependentOfRenderCadence,
            failures);
    runTest("physics preserves tangent and outward velocity",
            testPhysicsPreservesTangentAndOutwardVelocity, failures);
    runTest("inactive physics components do not participate",
            testInactivePhysicsComponentsDoNotParticipate, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics integration test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics integration tests passed.\n";
    return 0;
}

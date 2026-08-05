#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

    constexpr float kPhysicsComparisonEpsilon = 0.001f;
    constexpr float kDeterminismComparisonEpsilon = 0.00001f;

    bool isFinite(sf::Vector2f value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    void fixedStep(l2d::Scene& scene, l2d::PhysicsWorld2D& world, float deltaTime = 1.f / 120.f)
    {
        scene.fixedUpdate(deltaTime);
        world.step(scene, deltaTime);
    }

    struct BoxBody
    {
        l2d::GameObject* object = nullptr;
        l2d::BoxCollider2D* collider = nullptr;
        l2d::RigidBody2D* body = nullptr;
    };

    BoxBody createBox(l2d::Scene& scene, const std::string& name, sf::Vector2f position,
                      sf::Vector2f size, bool withBody = true,
                      l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);

        return {&object, &collider, body};
    }

    struct CircleBody
    {
        l2d::GameObject* object = nullptr;
        l2d::CircleCollider2D* collider = nullptr;
        l2d::RigidBody2D* body = nullptr;
    };

    CircleBody createCircle(l2d::Scene& scene, const std::string& name, sf::Vector2f position,
                            float radius, bool withBody = true,
                            l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::CircleCollider2D& collider = object.addComponent<l2d::CircleCollider2D>(radius);
        collider.setOffset({radius, radius});

        return {&object, &collider, body};
    }

    void testLongDurationRestingContactRemainsStable()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 30.f};
        config.velocityIterations = 12;
        config.positionIterations = 6;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        createBox(scene, "Floor", {0.f, 5.f}, {5.f, 1.f}, false);

        BoxBody resting = createBox(scene, "Resting", {2.f, 4.f}, {1.f, 1.f});
        resting.body->setUseGravity(true);

        constexpr std::size_t warmupTicks = 600;
        constexpr std::size_t totalTicks = 3600;
        std::size_t contactTicks = 0;
        std::size_t groundedTicks = 0;

        for (std::size_t tick = 0; tick < totalTicks; ++tick)
        {
            fixedStep(scene, world);

            L2D_REQUIRE(isFinite(resting.object->transform.position()));
            L2D_REQUIRE(isFinite(resting.body->velocity()));

            if (tick >= warmupTicks)
            {
                if (!world.contacts().empty()) ++contactTicks;

                if (resting.body->isGrounded()) ++groundedTicks;
            }
        }

        constexpr std::size_t observedTicks = totalTicks - warmupTicks;
        L2D_REQUIRE(contactTicks >= observedTicks * 99u / 100u);
        L2D_REQUIRE(groundedTicks >= observedTicks * 99u / 100u);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Stay);
        L2D_REQUIRE(resting.body->isGrounded());
        L2D_REQUIRE(resting.object->transform.position().y > 3.9f);
        L2D_REQUIRE(resting.object->transform.position().y < 4.1f);
        L2D_REQUIRE(std::fabs(resting.body->velocity().y) < 5.f);
    }

    void testLongerDynamicBoxStackRemainsFiniteAndOrdered()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 30.f};
        config.velocityIterations = 16;
        config.positionIterations = 8;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        createBox(scene, "Floor", {0.f, 10.f}, {8.f, 1.f}, false);

        constexpr std::size_t boxCount = 8;
        std::vector<BoxBody> boxes;
        boxes.reserve(boxCount);

        for (std::size_t index = 0; index < boxCount; ++index)
        {
            BoxBody box = createBox(scene, "StackBox" + std::to_string(index),
                                    {3.f, 9.f - static_cast<float>(index)}, {1.f, 1.f});
            box.body->setUseGravity(true);
            boxes.push_back(box);
        }

        constexpr std::size_t totalTicks = 1800;

        for (std::size_t tick = 0; tick < totalTicks; ++tick)
        {
            fixedStep(scene, world);

            for (const BoxBody& box : boxes)
            {
                L2D_REQUIRE(isFinite(box.object->transform.position()));
                L2D_REQUIRE(isFinite(box.body->velocity()));
            }
        }

        for (std::size_t index = 0; index < boxes.size(); ++index)
        {
            const BoxBody& box = boxes[index];
            L2D_REQUIRE(box.object->transform.position().y < 10.f);
            L2D_REQUIRE(std::fabs(box.body->velocity().x) < 20.f);
            L2D_REQUIRE(std::fabs(box.body->velocity().y) < 20.f);

            if (index > 0)
            {
                L2D_REQUIRE(box.object->transform.position().y <
                            boxes[index - 1].object->transform.position().y);
            }
        }
    }

    struct ScenarioResult
    {
        std::vector<sf::Vector2f> positions;
        std::vector<sf::Vector2f> velocities;
        std::vector<bool> groundedStates;
        std::vector<std::size_t> contactCounts;
        std::array<std::size_t, 3> eventPhaseCounts{0u, 0u, 0u};
    };

    ScenarioResult runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D broadPhaseMode)
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 30.f};
        config.velocityIterations = 12;
        config.positionIterations = 6;
        config.broadPhaseMode = broadPhaseMode;
        config.broadPhaseCellSize = 2.f;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        createBox(scene, "Floor", {-8.f, 10.f}, {16.f, 1.f}, false);

        std::vector<BoxBody> boxes;
        boxes.reserve(5);

        for (std::size_t index = 0; index < 5; ++index)
        {
            BoxBody box = createBox(
                scene, "DeterministicBox" + std::to_string(index),
                {-2.f + static_cast<float>(index), 2.f - static_cast<float>(index) * 0.25f},
                {1.f, 1.f});
            box.body->setUseGravity(true);
            box.body->setVelocity({static_cast<float>(index) * 0.2f - 0.4f, 0.f});
            boxes.push_back(box);
        }

        ScenarioResult result;
        constexpr std::size_t totalTicks = 1200;
        constexpr std::size_t checkpointInterval = 120;

        for (std::size_t tick = 0; tick < totalTicks; ++tick)
        {
            fixedStep(scene, world);

            for (const l2d::PhysicsContactEvent2D& event : world.contactEvents())
            {
                if (event.phase == l2d::PhysicsContactPhase2D::Begin)
                    ++result.eventPhaseCounts[0];
                else if (event.phase == l2d::PhysicsContactPhase2D::Stay)
                    ++result.eventPhaseCounts[1];
                else
                    ++result.eventPhaseCounts[2];
            }

            if ((tick + 1u) % checkpointInterval == 0u)
            {
                result.contactCounts.push_back(world.contacts().size());

                for (const BoxBody& box : boxes)
                {
                    result.positions.push_back(box.object->transform.position());
                    result.velocities.push_back(box.body->velocity());
                    result.groundedStates.push_back(box.body->isGrounded());
                }
            }
        }

        return result;
    }

    void requireEquivalentScenarioResults(const ScenarioResult& left, const ScenarioResult& right)
    {
        L2D_REQUIRE_EQUAL(left.positions.size(), right.positions.size());
        L2D_REQUIRE_EQUAL(left.velocities.size(), right.velocities.size());
        L2D_REQUIRE_EQUAL(left.groundedStates.size(), right.groundedStates.size());
        L2D_REQUIRE_EQUAL(left.contactCounts.size(), right.contactCounts.size());

        for (std::size_t index = 0; index < left.positions.size(); ++index)
        {
            L2D_REQUIRE_APPROX(left.positions[index], right.positions[index],
                               kDeterminismComparisonEpsilon);
            L2D_REQUIRE_APPROX(left.velocities[index], right.velocities[index],
                               kDeterminismComparisonEpsilon);
            L2D_REQUIRE_EQUAL(left.groundedStates[index], right.groundedStates[index]);
        }

        for (std::size_t index = 0; index < left.contactCounts.size(); ++index)
        {
            L2D_REQUIRE_EQUAL(left.contactCounts[index], right.contactCounts[index]);
        }

        for (std::size_t index = 0; index < left.eventPhaseCounts.size(); ++index)
        {
            L2D_REQUIRE_EQUAL(left.eventPhaseCounts[index], right.eventPhaseCounts[index]);
        }
    }

    void testRepeatedSimulationsRemainDeterministicOverManyTicks()
    {
        const ScenarioResult first =
            runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D::UniformGrid);
        const ScenarioResult second =
            runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D::UniformGrid);
        const ScenarioResult third =
            runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D::UniformGrid);

        requireEquivalentScenarioResults(first, second);
        requireEquivalentScenarioResults(first, third);
    }

    void testUniformGridMatchesBruteForceOverManyTicks()
    {
        const ScenarioResult uniformGrid =
            runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D::UniformGrid);
        const ScenarioResult bruteForce =
            runDeterministicScenario(l2d::PhysicsBroadPhaseMode2D::BruteForce);

        requireEquivalentScenarioResults(uniformGrid, bruteForce);
    }

    void testContinuousCollisionStopsHighSpeedMotion()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 0.f};

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        CircleBody mover = createCircle(scene, "FastMover", {0.f, 0.f}, 0.5f);
        createBox(scene, "ThinWall", {5.f, -5.f}, {0.1f, 10.f}, false);

        mover.body->setVelocity({1000.f, 0.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(isFinite(mover.object->transform.position()));
        L2D_REQUIRE(isFinite(mover.body->velocity()));
        L2D_REQUIRE(mover.object->transform.position().x > 3.9f);
        L2D_REQUIRE(mover.object->transform.position().x < 5.f);
        L2D_REQUIRE_APPROX(mover.body->velocity().x, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
        L2D_REQUIRE(mover.collider->isColliding());
        L2D_REQUIRE(world.stepStats().ccdSubstepCount > 1);
    }

    void testDeactivatedRigidBodyEndsAndReactivationBeginsContact()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 0.f};

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        CircleBody mover = createCircle(scene, "Mover", {0.f, 0.f}, 1.f);
        createBox(scene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);

        mover.body->setVelocity({0.f, 4.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);

        const sf::Vector2f positionBeforeDeactivation = mover.object->transform.position();
        mover.body->setActive(false);
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::End);
        L2D_REQUIRE_APPROX(mover.object->transform.position(), positionBeforeDeactivation,
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE(!mover.body->isGrounded());
        L2D_REQUIRE(!mover.collider->isColliding());

        mover.body->setActive(true);
        mover.body->setVelocity({0.f, 0.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
    }
}

int main()
{
    int failures = 0;

    runTest("long-duration resting contact remains stable",
            testLongDurationRestingContactRemainsStable, failures);
    runTest("longer dynamic box stack remains finite and ordered",
            testLongerDynamicBoxStackRemainsFiniteAndOrdered, failures);
    runTest("repeated simulations remain deterministic over many ticks",
            testRepeatedSimulationsRemainDeterministicOverManyTicks, failures);
    runTest("uniform grid matches brute force over many ticks",
            testUniformGridMatchesBruteForceOverManyTicks, failures);
    runTest("continuous collision stops high-speed motion",
            testContinuousCollisionStopsHighSpeedMotion, failures);
    runTest("deactivated rigid body ends and reactivation begins contact",
            testDeactivatedRigidBodyEndsAndReactivationBeginsContact, failures);

    return failures == 0 ? 0 : 1;
}

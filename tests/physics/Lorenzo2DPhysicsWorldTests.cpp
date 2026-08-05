#include "PhysicsTestSupport.hpp"

namespace
{
    using namespace l2d::test::physics;

    void testSceneAddressReuseStartsFreshContactHistory()
    {
        std::optional<l2d::Scene> sceneSlot;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        sceneSlot.emplace("FirstScene");
        const std::uintptr_t firstSceneAddress =
            reinterpret_cast<std::uintptr_t>(&sceneSlot.value());

        {
            CircleBody firstMover = createCircle(sceneSlot.value(), "FirstMover", {0.f, 0.f}, 1.f);
            CircleBody firstSensor =
                createCircle(sceneSlot.value(), "FirstSensor", {1.5f, 0.f}, 1.f, false);
            firstSensor.collider.setSensor(true);

            fixedStep(sceneSlot.value(), world);

            L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
            L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
            L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
            L2D_REQUIRE(world.isTouching(firstMover.object.id(), firstSensor.object.id()));
        }

        sceneSlot.reset();
        sceneSlot.emplace("SecondScene");

        L2D_REQUIRE_EQUAL(reinterpret_cast<std::uintptr_t>(&sceneSlot.value()), firstSceneAddress);

        CircleBody secondMover = createCircle(sceneSlot.value(), "SecondMover", {0.f, 0.f}, 1.f);
        CircleBody secondSensor =
            createCircle(sceneSlot.value(), "SecondSensor", {1.5f, 0.f}, 1.f, false);
        secondSensor.collider.setSensor(true);

        fixedStep(sceneSlot.value(), world);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
        L2D_REQUIRE(world.isTouching(secondMover.object.id(), secondSensor.object.id()));
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].contact.firstObjectId, secondMover.object.id());
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].contact.secondObjectId,
                          secondSensor.object.id());
    }

    void testResetSceneClearsPhysicsFlags()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody mover = createCircle(scene, "Mover", {0.f, 0.f}, 1.f);
        BoxBody floor = createBox(scene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        mover.body->setVelocity({0.f, 4.f});

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(floor.collider.isColliding());
        L2D_REQUIRE(mover.body->isGrounded());

        world.reset(scene);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(broadPhaseStatsEqual(world.broadPhaseStats(), l2d::PhysicsBroadPhaseStats2D{}));
        L2D_REQUIRE(!world.isTouching(mover.object.id(), floor.object.id()));
        L2D_REQUIRE(!mover.collider.isColliding());
        L2D_REQUIRE(!floor.collider.isColliding());
        L2D_REQUIRE(!mover.body->isGrounded());
    }

    void testKinematicBodiesNeverBecomeGrounded()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody kinematic =
            createCircle(scene, "Kinematic", {0.f, 0.f}, 1.f, true, l2d::BodyType2D::Kinematic);
        BoxBody dynamicSupport = createBox(scene, "DynamicSupport", {-5.f, 1.8f}, {10.f, 1.f});
        kinematic.body->setVelocity({0.f, 4.f});

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(kinematic.body->bodyType(), l2d::BodyType2D::Kinematic);
        L2D_REQUIRE(!kinematic.body->isGrounded());
        L2D_REQUIRE(!dynamicSupport.body->isGrounded());

        l2d::Scene transitionScene;
        l2d::PhysicsWorld2D transitionWorld(zeroGravityConfig());
        CircleBody transitioningBody =
            createCircle(transitionScene, "TransitioningBody", {0.f, 0.f}, 1.f);
        createBox(transitionScene, "TransitionFloor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        transitioningBody.body->setVelocity({0.f, 4.f});

        fixedStep(transitionScene, transitionWorld, 0.01f);
        L2D_REQUIRE(transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Kinematic);
        L2D_REQUIRE(!transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Dynamic);
        transitioningBody.object.transform.setPosition({0.f, 0.f});
        transitioningBody.body->setVelocity({0.f, 4.f});
        fixedStep(transitionScene, transitionWorld, 0.01f);
        L2D_REQUIRE(transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Static);
        L2D_REQUIRE(!transitioningBody.body->isGrounded());
    }

    void testAllBaseCollidersParticipate()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        l2d::GameObject& mover = scene.createGameObject("Mover");
        l2d::RigidBody2D& body = mover.addComponent<l2d::RigidBody2D>();
        l2d::BoxCollider2D& box = mover.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        l2d::CircleCollider2D& circle = mover.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::GameObject& obstacle = scene.createGameObject("Obstacle");
        obstacle.transform.setPosition({1.5f, 0.f});
        l2d::BoxCollider2D& obstacleBox =
            obstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});

        body.setVelocity({1.f, 0.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 2);
        L2D_REQUIRE(box.isColliding());
        L2D_REQUIRE(circle.isColliding());
        L2D_REQUIRE(obstacleBox.isColliding());
        L2D_REQUIRE(box.id() != circle.id());
        L2D_REQUIRE(world.isColliderTouching(box.id(), obstacleBox.id()));
        L2D_REQUIRE(world.isColliderTouching(circle.id(), obstacleBox.id()));

        const bool firstContactUsesBox = world.contacts()[0].firstColliderId == box.id() ||
                                         world.contacts()[0].secondColliderId == box.id();
        const bool secondContactUsesCircle = world.contacts()[1].firstColliderId == circle.id() ||
                                             world.contacts()[1].secondColliderId == circle.id();
        L2D_REQUIRE(firstContactUsesBox);
        L2D_REQUIRE(secondContactUsesCircle);
    }

    void testShortDynamicBoxStackRemainsFinite()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 30.f};
        config.velocityIterations = 12;
        config.positionIterations = 6;
        l2d::PhysicsWorld2D world(config);
        l2d::Scene scene;

        createBox(scene, "Floor", {0.f, 5.f}, {5.f, 1.f}, false);
        BoxBody lower = createBox(scene, "Lower", {2.f, 4.f}, {1.f, 1.f});
        BoxBody upper = createBox(scene, "Upper", {2.f, 3.f}, {1.f, 1.f});
        lower.body->setUseGravity(true);
        upper.body->setUseGravity(true);

        for (std::size_t tick = 0; tick < 240; ++tick)
            fixedStep(scene, world, 1.f / 120.f);

        L2D_REQUIRE(isFinite(lower.object.transform.position()));
        L2D_REQUIRE(isFinite(upper.object.transform.position()));
        L2D_REQUIRE(isFinite(lower.body->velocity()));
        L2D_REQUIRE(isFinite(upper.body->velocity()));
        L2D_REQUIRE(lower.object.transform.position().y < 5.f);
        L2D_REQUIRE(upper.object.transform.position().y < lower.object.transform.position().y);
        L2D_REQUIRE(std::fabs(lower.body->velocity().y) < 10.f);
        L2D_REQUIRE(std::fabs(upper.body->velocity().y) < 10.f);
    }

    void testLegacyTangentAndOutwardVelocityRemain()
    {
        l2d::Scene inwardScene;
        l2d::PhysicsWorld2D inwardWorld(zeroGravityConfig());
        CircleBody inward = createCircle(inwardScene, "Inward", {0.f, 0.f}, 1.f);
        createBox(inwardScene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        inward.body->setVelocity({3.f, 4.f});
        fixedStep(inwardScene, inwardWorld, 0.01f);

        L2D_REQUIRE_APPROX(inward.body->velocity().x, 3.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(inward.body->velocity().y, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(inward.body->isGrounded());

        l2d::Scene outwardScene;
        l2d::PhysicsWorld2D outwardWorld(zeroGravityConfig());
        CircleBody outward = createCircle(outwardScene, "Outward", {0.f, 0.f}, 1.f);
        createBox(outwardScene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        outward.body->setVelocity({3.f, -4.f});
        fixedStep(outwardScene, outwardWorld, 0.01f);

        L2D_REQUIRE_APPROX(outward.body->velocity().x, 3.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(outward.body->velocity().y, -4.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(outward.collider.isColliding());
    }
}

int main()
{
    int failures = 0;

    runTest("scene address reuse starts fresh contact history",
            testSceneAddressReuseStartsFreshContactHistory, failures);
    runTest("reset scene clears physics flags", testResetSceneClearsPhysicsFlags, failures);
    runTest("kinematic bodies never become grounded", testKinematicBodiesNeverBecomeGrounded,
            failures);
    runTest("all base colliders participate", testAllBaseCollidersParticipate, failures);
    runTest("short dynamic box stack remains finite", testShortDynamicBoxStackRemainsFinite,
            failures);
    runTest("legacy tangent and outward velocity remain", testLegacyTangentAndOutwardVelocityRemain,
            failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics world test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics world tests passed.\n";
    return 0;
}

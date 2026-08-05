#include "PhysicsTestSupport.hpp"

#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>

#include <cmath>

namespace
{
    using namespace l2d::test::physics;

    void testColliderTransformsIncludeScaleRotationAndOffset()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Transformed collider");
        object.transform.setPosition({10.f, 10.f});
        object.transform.setScale({2.f, 3.f});
        object.transform.setRotation(90.f);

        l2d::BoxCollider2D& box = object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 4.f});
        box.setOffset({1.f, 0.f});

        L2D_REQUIRE_APPROX(box.center(), (sf::Vector2f{10.f, 12.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(box.worldHalfExtents(), (sf::Vector2f{2.f, 6.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(box.min(), (sf::Vector2f{4.f, 10.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(box.max(), (sf::Vector2f{16.f, 14.f}), kPhysicsComparisonEpsilon);

        l2d::CircleCollider2D& circle = object.addComponent<l2d::CircleCollider2D>(2.f);
        L2D_REQUIRE_APPROX(circle.worldRadius(), 6.f, kPhysicsComparisonEpsilon);
    }

    void testAngularMotionAndOffCenterImpulse()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        l2d::GameObject& object = scene.createGameObject("Spinner");
        l2d::RigidBody2D& body = object.addComponent<l2d::RigidBody2D>();
        body.setFixedRotation(false);
        body.setInertia(2.f);

        body.applyImpulseAtPoint({0.f, 2.f}, {1.f, 0.f});
        L2D_REQUIRE_APPROX(body.angularVelocity(), 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(body.velocity(), (sf::Vector2f{0.f, 2.f}), kPhysicsComparisonEpsilon);

        fixedStep(scene, world, 0.5f);

        L2D_REQUIRE_APPROX(object.transform.rotation(), 28.64789f, 0.001f);
        L2D_REQUIRE_APPROX(object.transform.position(), (sf::Vector2f{0.f, 1.f}),
                           kPhysicsComparisonEpsilon);
    }

    void testSleepingAndWakeUp()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.timeToSleep = 0.05f;
        config.sleepLinearVelocityThreshold = 0.1f;
        config.sleepAngularVelocityThreshold = 0.1f;
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        l2d::GameObject& object = scene.createGameObject("Sleeper");
        l2d::RigidBody2D& body = object.addComponent<l2d::RigidBody2D>();

        fixedStep(scene, world, 0.03f);
        L2D_REQUIRE(body.isAwake());
        fixedStep(scene, world, 0.03f);

        L2D_REQUIRE(!body.isAwake());
        L2D_REQUIRE_EQUAL(world.stepStats().sleepingBodyCount, 1);

        body.setVelocity({2.f, 0.f});
        L2D_REQUIRE(body.isAwake());
    }

    void testPersistentContactsWarmStart()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.positionCorrectionPercent = 0.f;
        config.sleeping = false;
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        CircleBody mover = createCircle(scene, "Warm mover", {0.f, 0.f}, 1.f);
        BoxBody floor = createBox(scene, "Warm floor", {-5.f, 0.5f}, {10.f, 1.f}, false);
        mover.body->setVelocity({0.f, 1.f});
        fixedStep(scene, world, 0.01f);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);

        mover.body->setVelocity({0.f, 1.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.stepStats().warmStartedContactCount > 0);
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(floor.collider.isColliding());
    }

    void testDistanceJointConstrainsAndFiltersConnectedBodies()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.timeToSleep = 0.01f;
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        l2d::GameObject& first = scene.createGameObject("Joint first");
        first.transform.setPosition({0.f, 0.f});
        l2d::RigidBody2D& firstBody = first.addComponent<l2d::RigidBody2D>();
        first.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::GameObject& second = scene.createGameObject("Joint second");
        second.transform.setPosition({1.f, 0.f});
        l2d::RigidBody2D& secondBody = second.addComponent<l2d::RigidBody2D>();
        second.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::DistanceJoint2D& joint = first.addComponent<l2d::DistanceJoint2D>(second.id(), 5.f);
        L2D_REQUIRE(joint.id() != l2d::InvalidJointId);
        L2D_REQUIRE(!joint.collideConnected());

        fixedStep(scene, world, 0.01f);

        const sf::Vector2f delta = second.transform.position() - first.transform.position();
        const float distance = std::hypot(delta.x, delta.y);
        L2D_REQUIRE_APPROX(distance, 5.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(world.stepStats().jointConstraintCount, 1);
        L2D_REQUIRE(world.contacts().empty());

        firstBody.setVelocity({0.f, 0.f});
        secondBody.setVelocity({0.f, 0.f});
        fixedStep(scene, world, 0.02f);
        L2D_REQUIRE(!firstBody.isAwake());
        L2D_REQUIRE(!secondBody.isAwake());

        joint.setRestLength(3.f);
        fixedStep(scene, world, 0.01f);
        L2D_REQUIRE(firstBody.isAwake());
        L2D_REQUIRE(secondBody.isAwake());

        const sf::Vector2f adjustedDelta = second.transform.position() - first.transform.position();
        L2D_REQUIRE_APPROX(std::hypot(adjustedDelta.x, adjustedDelta.y), 3.f,
                           kPhysicsComparisonEpsilon);
    }
}

int main()
{
    int failures = 0;

    runTest("collider transforms include scale rotation and offset",
            testColliderTransformsIncludeScaleRotationAndOffset, failures);
    runTest("angular motion and off-center impulse", testAngularMotionAndOffCenterImpulse,
            failures);
    runTest("sleeping and wake-up", testSleepingAndWakeUp, failures);
    runTest("persistent contacts warm start", testPersistentContactsWarmStart, failures);
    runTest("distance joint constrains and filters connected bodies",
            testDistanceJointConstrainsAndFiltersConnectedBodies, failures);

    return failures == 0 ? 0 : 1;
}

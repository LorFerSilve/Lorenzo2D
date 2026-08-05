#include "PhysicsTestSupport.hpp"

namespace
{
    using namespace l2d::test::physics;

    void testCircleCircleManifolds()
    {
        l2d::GameObject firstObject;
        l2d::CircleCollider2D& first = firstObject.addComponent<l2d::CircleCollider2D>(2.f);

        l2d::GameObject secondObject;
        l2d::CircleCollider2D& second = secondObject.addComponent<l2d::CircleCollider2D>(2.f);

        secondObject.transform.setPosition({3.f, 0.f});

        l2d::CollisionManifold2D manifold;
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(isFinite(manifold.point));

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE_APPROX(reversed.normal, (sf::Vector2f{-1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversed.penetration, 1.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({4.f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.penetration, 0.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({0.f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 4.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({5.f, 0.f});
        L2D_REQUIRE(!l2d::computeCollisionManifold(first, second, manifold));
    }

    void testBoxBoxManifolds()
    {
        l2d::GameObject firstObject;
        l2d::BoxCollider2D& first =
            firstObject.addComponent<l2d::BoxCollider2D>(sf::Vector2f{4.f, 4.f});

        l2d::GameObject secondObject;
        l2d::BoxCollider2D& second =
            secondObject.addComponent<l2d::BoxCollider2D>(sf::Vector2f{4.f, 4.f});

        l2d::CollisionManifold2D manifold;

        secondObject.transform.setPosition({3.f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 1.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({0.f, 3.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{0.f, 1.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 1.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({2.f, 2.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 2.f, kPhysicsComparisonEpsilon);

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE_APPROX(reversed.normal, (sf::Vector2f{-1.f, 0.f}), kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({4.f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.penetration, 0.f, kPhysicsComparisonEpsilon);

        secondObject.transform.setPosition({4.01f, 0.f});
        L2D_REQUIRE(!l2d::computeCollisionManifold(first, second, manifold));

        firstObject.transform.setPosition({4.f, 4.f});
        first.setSize({2.f, 2.f});
        secondObject.transform.setPosition({0.f, 0.f});
        second.setSize({10.f, 10.f});

        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{-1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 2.f, kPhysicsComparisonEpsilon);

        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE_APPROX(reversed.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversed.penetration, 2.f, kPhysicsComparisonEpsilon);
    }

    void testCircleBoxManifolds()
    {
        l2d::GameObject circleObject;
        l2d::CircleCollider2D& circle = circleObject.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::GameObject boxObject;
        l2d::BoxCollider2D& box =
            boxObject.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});

        l2d::CollisionManifold2D manifold;

        boxObject.transform.setPosition({1.5f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 0.5f, kPhysicsComparisonEpsilon);

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(box, circle, reversed));
        L2D_REQUIRE_APPROX(reversed.normal, (sf::Vector2f{-1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversed.penetration, 0.5f, kPhysicsComparisonEpsilon);

        boxObject.transform.setPosition({2.f, 0.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE_APPROX(manifold.penetration, 0.f, kPhysicsComparisonEpsilon);

        boxObject.transform.setPosition({1.5f, 1.5f});
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{0.70710677f, 0.70710677f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 1.f - std::sqrt(0.5f), kPhysicsComparisonEpsilon);

        boxObject.transform.setPosition({0.f, 0.f});
        circleObject.transform.setPosition({1.f, 1.f});
        box.setSize({4.f, 4.f});
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE_APPROX(manifold.normal, (sf::Vector2f{-1.f, 0.f}), kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(manifold.penetration, 2.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(isFinite(manifold.point));
    }

    void testCollisionFilters()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody first = createCircle(scene, "First", {0.f, 0.f}, 1.f);
        CircleBody second = createCircle(scene, "Second", {1.5f, 0.f}, 1.f, false);

        first.collider.setFilter({1u, 2u});
        second.collider.setFilter({4u, 1u});

        L2D_REQUIRE(!first.collider.canCollideWith(second.collider));
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(!first.collider.isColliding());
        L2D_REQUIRE(!second.collider.isColliding());

        second.collider.setFilter({2u, 1u});
        L2D_REQUIRE(first.collider.canCollideWith(second.collider));
        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);

        first.collider.setFilter({1u, 0u});
        fixedStep(scene, world);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::End);
    }

    void testStaticPairsAreNotReported()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        BoxBody first = createBox(scene, "FirstStatic", {0.f, 0.f}, {2.f, 2.f}, false);
        BoxBody second = createBox(scene, "SecondStatic", {1.f, 0.f}, {2.f, 2.f}, false);

        fixedStep(scene, world);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(!first.collider.isColliding());
        L2D_REQUIRE(!second.collider.isColliding());
    }

    void testSensorEventLifecycleHasNoResponse()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody mover = createCircle(scene, "Mover", {0.f, 0.f}, 1.f);
        CircleBody sensor = createCircle(scene, "Sensor", {1.5f, 0.f}, 1.f, false);
        sensor.collider.setSensor(true);
        mover.body->setVelocity({1.f, 0.f});

        const sf::Vector2f sensorPosition = sensor.object.transform.position();

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.contacts()[0].sensor);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
        L2D_REQUIRE(world.isTouching(mover.object.id(), sensor.object.id()));
        L2D_REQUIRE(world.isTouching(sensor.object.id(), mover.object.id()));
        L2D_REQUIRE_APPROX(mover.body->velocity().x, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(mover.object.transform.position(), (sf::Vector2f{0.01f, 0.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(sensor.object.transform.position(), sensorPosition,
                           kPhysicsComparisonEpsilon);

        fixedStep(scene, world, 0.01f);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Stay);

        mover.object.transform.setPosition({10.f, 0.f});
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::End);
        L2D_REQUIRE(!mover.collider.isColliding());
        L2D_REQUIRE(!sensor.collider.isColliding());
    }

    void testDeactivationAndDestructionEmitEnd()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody first = createCircle(scene, "First", {0.f, 0.f}, 1.f);
        CircleBody second = createCircle(scene, "Second", {1.5f, 0.f}, 1.f, false);
        second.collider.setSensor(true);

        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.isTouching(first.object.id(), second.object.id()));

        second.collider.setActive(false);
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::End);

        second.collider.setActive(true);
        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);

        second.object.destroy();
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::End);
    }

    void testInvalidDeltaTimeIsANoOp()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody mover = createCircle(scene, "Mover", {0.f, 0.f}, 1.f);
        CircleBody sensor = createCircle(scene, "Sensor", {1.5f, 0.f}, 1.f, false);
        sensor.collider.setSensor(true);

        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);

        const l2d::PhysicsBroadPhaseStats2D statsBeforeInvalidStep = world.broadPhaseStats();
        L2D_REQUIRE_EQUAL(statsBeforeInvalidStep.proxyCount, 2);
        L2D_REQUIRE_EQUAL(statsBeforeInvalidStep.fallbackProxyCount, 0);
        L2D_REQUIRE_EQUAL(statsBeforeInvalidStep.bruteForcePairCount, 1);
        L2D_REQUIRE_EQUAL(statsBeforeInvalidStep.candidatePairCount, 1);
        L2D_REQUIRE_EQUAL(statsBeforeInvalidStep.narrowPhaseTestCount, 1);

        mover.body->addForce({60.f, 0.f});
        const sf::Vector2f positionBeforeInvalidStep = mover.object.transform.position();

        world.step(scene, 0.f);
        world.step(scene, std::numeric_limits<float>::quiet_NaN());

        L2D_REQUIRE_APPROX(mover.object.transform.position(), positionBeforeInvalidStep,
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
        L2D_REQUIRE(broadPhaseStatsEqual(world.broadPhaseStats(), statsBeforeInvalidStep));

        mover.object.transform.setPosition({10.f, 0.f});
        fixedStep(scene, world, 1.f / 60.f);
        L2D_REQUIRE_APPROX(mover.body->velocity().x, 1.f, kPhysicsComparisonEpsilon);

        world.reset();
        L2D_REQUIRE(broadPhaseStatsEqual(world.broadPhaseStats(), l2d::PhysicsBroadPhaseStats2D{}));
    }

    void testGroundedUsesOnlySolidSupportContacts()
    {
        l2d::Scene floorScene;
        l2d::PhysicsWorld2D floorWorld(zeroGravityConfig());
        CircleBody mover = createCircle(floorScene, "Mover", {0.f, 0.f}, 1.f);
        createBox(floorScene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        mover.body->setVelocity({0.f, 4.f});

        fixedStep(floorScene, floorWorld, 0.01f);
        L2D_REQUIRE(mover.body->isGrounded());

        l2d::Scene staticFirstScene;
        l2d::PhysicsWorld2D staticFirstWorld(zeroGravityConfig());
        BoxBody staticFirstFloor =
            createBox(staticFirstScene, "StaticFirstFloor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        CircleBody staticFirstMover =
            createCircle(staticFirstScene, "StaticFirstMover", {0.f, 0.f}, 1.f);
        staticFirstMover.body->setVelocity({0.f, 4.f});

        fixedStep(staticFirstScene, staticFirstWorld, 0.01f);

        L2D_REQUIRE(staticFirstMover.body->isGrounded());
        L2D_REQUIRE_APPROX(staticFirstMover.body->velocity().y, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(staticFirstWorld.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(staticFirstWorld.contacts()[0].firstObjectId,
                          staticFirstFloor.object.id());
        L2D_REQUIRE_EQUAL(staticFirstWorld.contacts()[0].secondObjectId,
                          staticFirstMover.object.id());
        L2D_REQUIRE(staticFirstWorld.contacts()[0].manifold.normal.y < 0.f);
        L2D_REQUIRE_APPROX(staticFirstMover.object.transform.position(),
                           mover.object.transform.position(), kPhysicsComparisonEpsilon);

        l2d::Scene wallScene;
        l2d::PhysicsWorld2D wallWorld(zeroGravityConfig());
        CircleBody wallMover = createCircle(wallScene, "WallMover", {0.f, 0.f}, 1.f);
        createBox(wallScene, "Wall", {1.8f, -5.f}, {1.f, 10.f}, false);
        wallMover.body->setVelocity({4.f, 0.f});

        fixedStep(wallScene, wallWorld, 0.01f);
        L2D_REQUIRE(!wallMover.body->isGrounded());

        l2d::Scene sensorScene;
        l2d::PhysicsWorld2D sensorWorld(zeroGravityConfig());
        CircleBody sensorMover = createCircle(sensorScene, "SensorMover", {0.f, 0.f}, 1.f);
        BoxBody sensorFloor =
            createBox(sensorScene, "SensorFloor", {-5.f, 1.8f}, {10.f, 1.f}, false);
        sensorFloor.collider.setSensor(true);
        sensorMover.body->setVelocity({0.f, 4.f});

        fixedStep(sensorScene, sensorWorld, 0.01f);
        L2D_REQUIRE(!sensorMover.body->isGrounded());
    }

    void testContactsHaveDeterministicOrdering()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody first = createCircle(scene, "First", {0.f, 0.f}, 2.f);
        CircleBody second = createCircle(scene, "Second", {1.f, 0.f}, 2.f);
        CircleBody third = createCircle(scene, "Third", {2.f, 0.f}, 2.f);

        first.collider.setSensor(true);
        second.collider.setSensor(true);
        third.collider.setSensor(true);

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 3);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 3);

        for (std::size_t index = 0; index < world.contacts().size(); ++index)
        {
            const l2d::PhysicsContact2D& contact = world.contacts()[index];

            L2D_REQUIRE(contact.firstObjectId < contact.secondObjectId);
            L2D_REQUIRE(isFinite(contact.manifold.normal));
            L2D_REQUIRE(isFinite(contact.manifold.point));
            L2D_REQUIRE(contact.manifold.penetration >= 0.f);

            if (index == 0) continue;

            const l2d::PhysicsContact2D& previous = world.contacts()[index - 1];

            L2D_REQUIRE(std::tie(previous.firstObjectId, previous.secondObjectId) <
                        std::tie(contact.firstObjectId, contact.secondObjectId));
        }

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 3);

        for (const l2d::PhysicsContactEvent2D& event : world.contactEvents())
        {
            L2D_REQUIRE_EQUAL(event.phase, l2d::PhysicsContactPhase2D::Stay);
        }

        l2d::Scene otherScene;
        fixedStep(otherScene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());

        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 3);

        for (const l2d::PhysicsContactEvent2D& event : world.contactEvents())
        {
            L2D_REQUIRE_EQUAL(event.phase, l2d::PhysicsContactPhase2D::Begin);
        }

        world.reset();
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(!world.isTouching(first.object.id(), second.object.id()));
    }

    void testMaximumMassCollisionRemainsFinite()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        BoxBody mover = createBox(scene, "MaximumMassMover", {0.f, 0.f}, {2.f, 2.f});
        BoxBody obstacle = createBox(scene, "StaticObstacle", {0.5f, 0.f}, {2.f, 2.f}, false);

        mover.body->setMass(std::numeric_limits<float>::max());
        mover.body->setVelocity({1.f, 0.f});

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(obstacle.collider.isColliding());
        L2D_REQUIRE(isFinite(mover.object.transform.position()));
        L2D_REQUIRE(isFinite(mover.body->velocity()));
        L2D_REQUIRE(mover.object.transform.position().x < -1.f);
        L2D_REQUIRE_APPROX(mover.body->velocity().x, 0.f, 0.01f);
        L2D_REQUIRE_APPROX(obstacle.object.transform.position(), (sf::Vector2f{0.5f, 0.f}),
                           kPhysicsComparisonEpsilon);
    }

    void testExtremeRestitutionSwapsFiniteVelocities()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.restitutionVelocityThreshold = 0.f;
        config.positionCorrectionPercent = 0.f;
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        CircleBody first = createCircle(scene, "FirstExtremeBody", {0.f, 0.f}, 10.f);
        CircleBody second = createCircle(scene, "SecondExtremeBody", {10.f, 0.f}, 10.f);

        const float maximum = std::numeric_limits<float>::max();
        first.body->setVelocity({maximum, 0.f});
        second.body->setVelocity({-maximum, 0.f});
        first.collider.setMaterial(material(1.f));
        second.collider.setMaterial(material(1.f));

        fixedStep(scene, world, std::numeric_limits<float>::min());

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(isFinite(first.body->velocity()));
        L2D_REQUIRE(isFinite(second.body->velocity()));
        L2D_REQUIRE_APPROX(first.body->velocity().x / maximum, -1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(second.body->velocity().x / maximum, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(first.body->velocity().y, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(second.body->velocity().y, 0.f, kPhysicsComparisonEpsilon);
    }
}

int main()
{
    int failures = 0;

    runTest("circle-circle manifolds", testCircleCircleManifolds, failures);
    runTest("box-box manifolds", testBoxBoxManifolds, failures);
    runTest("circle-box manifolds", testCircleBoxManifolds, failures);
    runTest("collision filters", testCollisionFilters, failures);
    runTest("static pairs are not reported", testStaticPairsAreNotReported, failures);
    runTest("sensor event lifecycle has no response", testSensorEventLifecycleHasNoResponse,
            failures);
    runTest("deactivation and destruction emit End", testDeactivationAndDestructionEmitEnd,
            failures);
    runTest("invalid delta time is a no-op", testInvalidDeltaTimeIsANoOp, failures);
    runTest("grounded uses only solid support contacts", testGroundedUsesOnlySolidSupportContacts,
            failures);
    runTest("contacts have deterministic ordering", testContactsHaveDeterministicOrdering,
            failures);
    runTest("maximum-mass collision remains finite", testMaximumMassCollisionRemainsFinite,
            failures);
    runTest("extreme restitution swaps finite velocities",
            testExtremeRestitutionSwapsFiniteVelocities, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics collision test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics collision tests passed.\n";
    return 0;
}

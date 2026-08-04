#include "PhysicsTestSupport.hpp"

namespace
{
    using namespace l2d::test::physics;

    void testLegacyDefaultsAndInputSanitization()
    {
        l2d::RigidBody2D body;

        L2D_REQUIRE_EQUAL(body.bodyType(), l2d::BodyType2D::Dynamic);
        L2D_REQUIRE_APPROX(body.mass(), 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(body.inverseMass(), 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(!body.useGravity());
        L2D_REQUIRE_APPROX(body.gravityScale(), 1.f, kPhysicsComparisonEpsilon);

        body.setMass(2.f);
        L2D_REQUIRE_APPROX(body.inverseMass(), 0.5f, kPhysicsComparisonEpsilon);

        body.setVelocity({std::numeric_limits<float>::quiet_NaN(), 1.f});
        L2D_REQUIRE_APPROX(body.velocity(), (sf::Vector2f{0.f, 0.f}), kPhysicsComparisonEpsilon);

        body.setAcceleration({1.f, std::numeric_limits<float>::infinity()});
        L2D_REQUIRE_APPROX(body.acceleration(), (sf::Vector2f{0.f, 0.f}),
                           kPhysicsComparisonEpsilon);

        body.applyImpulse({2.f, 4.f});
        L2D_REQUIRE_APPROX(body.velocity(), (sf::Vector2f{1.f, 2.f}), kPhysicsComparisonEpsilon);

        const float maximum = std::numeric_limits<float>::max();
        l2d::RigidBody2D cancellationBody;
        cancellationBody.setMass(0.5f);
        cancellationBody.setVelocity({maximum, -maximum});
        cancellationBody.applyImpulse({-maximum, maximum});
        L2D_REQUIRE_APPROX((sf::Vector2f{cancellationBody.velocity().x / maximum,
                                         cancellationBody.velocity().y / maximum}),
                           (sf::Vector2f{-1.f, 1.f}), kPhysicsComparisonEpsilon);

        body.setBodyType(l2d::BodyType2D::Static);
        L2D_REQUIRE_APPROX(body.inverseMass(), 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(body.velocity(), (sf::Vector2f{0.f, 0.f}), kPhysicsComparisonEpsilon);
        body.setVelocity({4.f, 5.f});
        L2D_REQUIRE_APPROX(body.velocity(), (sf::Vector2f{0.f, 0.f}), kPhysicsComparisonEpsilon);

        body.setBodyType(static_cast<l2d::BodyType2D>(999));
        L2D_REQUIRE_EQUAL(body.bodyType(), l2d::BodyType2D::Dynamic);
        L2D_REQUIRE_APPROX(body.inverseMass(), 0.5f, kPhysicsComparisonEpsilon);

        l2d::GameObject object;
        l2d::CircleCollider2D& collider = object.addComponent<l2d::CircleCollider2D>(2.f);

        L2D_REQUIRE_APPROX(collider.material().restitution, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().staticFriction, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().dynamicFriction, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(!collider.isSensor());
        L2D_REQUIRE_EQUAL(collider.filter().categoryBits, 1u);
        L2D_REQUIRE_EQUAL(collider.filter().maskBits, std::numeric_limits<std::uint32_t>::max());

        collider.setMaterial({2.f, std::numeric_limits<float>::quiet_NaN(), 0.75f});

        L2D_REQUIRE_APPROX(collider.material().restitution, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().staticFriction, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().dynamicFriction, 0.f, kPhysicsComparisonEpsilon);

        collider.setMaterial({0.5f, 0.8f, 1.f});
        L2D_REQUIRE_APPROX(collider.material().restitution, 0.5f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().staticFriction, 0.8f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(collider.material().dynamicFriction, 0.8f, kPhysicsComparisonEpsilon);

        collider.setOffset({std::numeric_limits<float>::infinity(), -3.f});
        L2D_REQUIRE_APPROX(collider.offset(), (sf::Vector2f{0.f, -3.f}), kPhysicsComparisonEpsilon);
    }

    void testWorldConfigAndRestitutionThreshold()
    {
        l2d::PhysicsWorld2D world;

        L2D_REQUIRE_EQUAL(world.config().broadPhaseMode, l2d::PhysicsBroadPhaseMode2D::UniformGrid);
        L2D_REQUIRE_APPROX(world.config().broadPhaseCellSize, 128.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(world.config().broadPhaseMaxCellsPerProxy, 256u);
        L2D_REQUIRE(broadPhaseStatsEqual(world.broadPhaseStats(), l2d::PhysicsBroadPhaseStats2D{}));

        l2d::PhysicsWorld2DConfig invalidConfig;
        invalidConfig.gravity = {std::numeric_limits<float>::quiet_NaN(), 10.f};
        invalidConfig.velocityIterations = 0;
        invalidConfig.positionIterations = 0;
        invalidConfig.positionCorrectionPercent = 2.f;
        invalidConfig.penetrationSlop = -1.f;
        invalidConfig.restitutionVelocityThreshold = std::numeric_limits<float>::infinity();
        invalidConfig.groundedNormalThreshold = -1.f;
        invalidConfig.broadPhaseMode = static_cast<l2d::PhysicsBroadPhaseMode2D>(999);
        invalidConfig.broadPhaseCellSize = std::numeric_limits<float>::quiet_NaN();
        invalidConfig.broadPhaseMaxCellsPerProxy = 0;

        world.setConfig(invalidConfig);

        const l2d::PhysicsWorld2DConfig& sanitized = world.config();
        L2D_REQUIRE_APPROX(sanitized.gravity, (sf::Vector2f{0.f, 980.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(sanitized.velocityIterations, 8u);
        L2D_REQUIRE_EQUAL(sanitized.positionIterations, 3u);
        L2D_REQUIRE_APPROX(sanitized.positionCorrectionPercent, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(sanitized.penetrationSlop, 0.01f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(sanitized.restitutionVelocityThreshold, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(sanitized.groundedNormalThreshold, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(sanitized.broadPhaseMode, l2d::PhysicsBroadPhaseMode2D::UniformGrid);
        L2D_REQUIRE_APPROX(sanitized.broadPhaseCellSize, 128.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(sanitized.broadPhaseMaxCellsPerProxy, 256u);

        l2d::PhysicsWorld2DConfig extremeIterations = zeroGravityConfig();
        extremeIterations.velocityIterations = std::numeric_limits<std::uint32_t>::max();
        extremeIterations.positionIterations = std::numeric_limits<std::uint32_t>::max();
        extremeIterations.positionCorrectionPercent = 0.f;
        world.setConfig(extremeIterations);

        L2D_REQUIRE_EQUAL(world.config().velocityIterations, 64u);
        L2D_REQUIRE_EQUAL(world.config().positionIterations, 64u);

        l2d::Scene noCorrectionScene;
        BoxBody noCorrectionMover =
            createBox(noCorrectionScene, "NoCorrectionMover", {0.f, 0.f}, {1.f, 1.f}, true);
        createBox(noCorrectionScene, "NoCorrectionObstacle", {0.5f, 0.f}, {1.f, 1.f}, false);
        const sf::Vector2f positionBeforeStep = noCorrectionMover.object.transform.position();

        world.step(noCorrectionScene, 1.f / 60.f);

        L2D_REQUIRE_EQUAL(noCorrectionMover.object.transform.position(), positionBeforeStep);

        l2d::PhysicsWorld2DConfig thresholdConfig = zeroGravityConfig();
        thresholdConfig.positionCorrectionPercent = 0.f;
        thresholdConfig.restitutionVelocityThreshold = 2.f;
        l2d::PhysicsWorld2D thresholdWorld(thresholdConfig);
        l2d::Scene suppressedScene;
        CircleBody suppressedMover =
            createCircle(suppressedScene, "SuppressedMover", {0.f, 0.f}, 1.f);
        CircleBody suppressedObstacle =
            createCircle(suppressedScene, "SuppressedObstacle", {1.5f, 0.f}, 1.f, false);
        suppressedMover.collider.setMaterial(material(1.f));
        suppressedObstacle.collider.setMaterial(material(1.f));
        suppressedMover.body->setVelocity({1.f, 0.f});

        fixedStep(suppressedScene, thresholdWorld, 0.01f);
        L2D_REQUIRE_APPROX(suppressedMover.body->velocity().x, 0.f, kPhysicsComparisonEpsilon);

        thresholdConfig.restitutionVelocityThreshold = 0.f;
        thresholdWorld.setConfig(thresholdConfig);
        l2d::Scene bounceScene;
        CircleBody bounceMover = createCircle(bounceScene, "BounceMover", {0.f, 0.f}, 1.f);
        CircleBody bounceObstacle =
            createCircle(bounceScene, "BounceObstacle", {1.5f, 0.f}, 1.f, false);
        bounceMover.collider.setMaterial(material(1.f));
        bounceObstacle.collider.setMaterial(material(1.f));
        bounceMover.body->setVelocity({1.f, 0.f});

        fixedStep(bounceScene, thresholdWorld, 0.01f);
        L2D_REQUIRE_APPROX(bounceMover.body->velocity().x, -1.f, kPhysicsComparisonEpsilon);
    }

    void testBodyModesAndFreeIntegration()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.gravity = {0.f, 10.f};
        l2d::PhysicsWorld2D world(config);

        l2d::GameObject& dynamicObject = scene.createGameObject("Dynamic");
        l2d::RigidBody2D& dynamicBody = dynamicObject.addComponent<l2d::RigidBody2D>();
        dynamicBody.setVelocity({1.f, 0.f});
        dynamicBody.setAcceleration({2.f, 0.f});
        dynamicBody.setUseGravity(true);
        dynamicBody.setGravityScale(0.5f);

        l2d::GameObject& kinematicObject = scene.createGameObject("Kinematic");
        kinematicObject.transform.setPosition({10.f, 0.f});
        l2d::RigidBody2D& kinematicBody = kinematicObject.addComponent<l2d::RigidBody2D>();
        kinematicBody.setBodyType(l2d::BodyType2D::Kinematic);
        kinematicBody.setVelocity({3.f, 4.f});
        kinematicBody.setAcceleration({100.f, 100.f});
        kinematicBody.setUseGravity(true);
        kinematicBody.addForce({100.f, 100.f});

        l2d::GameObject& staticObject = scene.createGameObject("Static");
        staticObject.transform.setPosition({20.f, 0.f});
        l2d::RigidBody2D& staticBody = staticObject.addComponent<l2d::RigidBody2D>();
        staticBody.setBodyType(l2d::BodyType2D::Static);
        staticBody.setVelocity({20.f, 20.f});

        fixedStep(scene, world, 0.5f);

        L2D_REQUIRE_APPROX(dynamicBody.velocity(), (sf::Vector2f{2.f, 2.5f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(dynamicObject.transform.position(), (sf::Vector2f{1.f, 1.25f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(kinematicBody.velocity(), (sf::Vector2f{3.f, 4.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(kinematicObject.transform.position(), (sf::Vector2f{11.5f, 2.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(staticObject.transform.position(), (sf::Vector2f{20.f, 0.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(staticBody.velocity(), (sf::Vector2f{0.f, 0.f}),
                           kPhysicsComparisonEpsilon);

        l2d::Scene forceScene;
        l2d::PhysicsWorld2D forceWorld(zeroGravityConfig());
        l2d::GameObject& forceObject = forceScene.createGameObject("Force");
        l2d::RigidBody2D& forceBody = forceObject.addComponent<l2d::RigidBody2D>();
        forceBody.setMass(2.f);
        forceBody.addForce({4.f, 0.f});

        fixedStep(forceScene, forceWorld, 0.5f);
        L2D_REQUIRE_APPROX(forceBody.velocity().x, 1.f, kPhysicsComparisonEpsilon);
        fixedStep(forceScene, forceWorld, 0.5f);
        L2D_REQUIRE_APPROX(forceBody.velocity().x, 1.f, kPhysicsComparisonEpsilon);
    }

    void testExtremeIntegrationCancellationStaysFinite()
    {
        const float maximum = std::numeric_limits<float>::max();

        l2d::Scene dynamicScene;
        l2d::PhysicsWorld2D dynamicWorld(zeroGravityConfig());
        l2d::GameObject& dynamicObject = dynamicScene.createGameObject("ExtremeDynamic");
        l2d::RigidBody2D& dynamicBody = dynamicObject.addComponent<l2d::RigidBody2D>();
        dynamicBody.setVelocity({-maximum, 0.f});
        dynamicBody.setAcceleration({maximum, 0.f});

        fixedStep(dynamicScene, dynamicWorld, 2.f);

        L2D_REQUIRE(isFinite(dynamicBody.velocity()));
        L2D_REQUIRE_APPROX(dynamicBody.velocity().x / maximum, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(isFinite(dynamicObject.transform.position()));

        l2d::Scene kinematicScene;
        l2d::PhysicsWorld2D kinematicWorld(zeroGravityConfig());
        l2d::GameObject& kinematicObject = kinematicScene.createGameObject("ExtremeKinematic");
        kinematicObject.transform.setPosition({-maximum, 0.f});
        l2d::RigidBody2D& kinematicBody = kinematicObject.addComponent<l2d::RigidBody2D>();
        kinematicBody.setBodyType(l2d::BodyType2D::Kinematic);
        kinematicBody.setVelocity({maximum, 0.f});

        fixedStep(kinematicScene, kinematicWorld, 2.f);

        L2D_REQUIRE(isFinite(kinematicObject.transform.position()));
        L2D_REQUIRE_APPROX(kinematicObject.transform.position().x / maximum, 1.f,
                           kPhysicsComparisonEpsilon);
    }

    void testDynamicStaticAndDynamicBoxResponse()
    {
        l2d::Scene circleScene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody mover = createCircle(circleScene, "Mover", {0.f, 0.f}, 1.f);
        CircleBody obstacle = createCircle(circleScene, "Obstacle", {1.5f, 0.f}, 1.f, false);

        mover.body->setVelocity({1.f, 0.f});
        fixedStep(circleScene, world, 0.01f);

        L2D_REQUIRE_APPROX(mover.body->velocity().x, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(obstacle.object.transform.position(), (sf::Vector2f{1.5f, 0.f}),
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE(world.isTouching(mover.object.id(), obstacle.object.id()));
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(obstacle.collider.isColliding());

        l2d::Scene boxScene;
        l2d::PhysicsWorld2D boxWorld(zeroGravityConfig());
        BoxBody boxMover = createBox(boxScene, "BoxMover", {0.f, 0.f}, {2.f, 2.f});
        BoxBody boxObstacle = createBox(boxScene, "BoxObstacle", {1.8f, 0.f}, {2.f, 2.f}, false);

        boxMover.body->setVelocity({1.f, 0.f});
        fixedStep(boxScene, boxWorld, 0.01f);

        L2D_REQUIRE_APPROX(boxMover.body->velocity().x, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_EQUAL(boxWorld.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(boxWorld.contacts()[0].firstColliderType, l2d::ColliderType::Box);
        L2D_REQUIRE_EQUAL(boxWorld.contacts()[0].secondColliderType, l2d::ColliderType::Box);
        L2D_REQUIRE_APPROX(boxObstacle.object.transform.position(), (sf::Vector2f{1.8f, 0.f}),
                           kPhysicsComparisonEpsilon);
    }

    void testUnequalMassRestitutionAndKinematicPush()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.restitutionVelocityThreshold = 0.f;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        CircleBody first = createCircle(scene, "First", {0.f, 0.f}, 1.f);
        CircleBody second = createCircle(scene, "Second", {1.8f, 0.f}, 1.f);

        first.body->setMass(1.f);
        second.body->setMass(3.f);
        first.body->setVelocity({2.f, 0.f});
        second.body->setVelocity({0.f, 0.f});
        first.collider.setMaterial(material(0.25f));
        second.collider.setMaterial(material(0.75f));

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE_APPROX(first.body->velocity().x, -0.625f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(second.body->velocity().x, 0.875f, kPhysicsComparisonEpsilon);

        l2d::Scene reversedRestitutionScene;
        l2d::PhysicsWorld2D reversedRestitutionWorld(config);
        CircleBody reversedFirst =
            createCircle(reversedRestitutionScene, "ReversedFirst", {0.f, 0.f}, 1.f);
        CircleBody reversedSecond =
            createCircle(reversedRestitutionScene, "ReversedSecond", {1.8f, 0.f}, 1.f);

        reversedFirst.body->setMass(1.f);
        reversedSecond.body->setMass(3.f);
        reversedFirst.body->setVelocity({2.f, 0.f});
        reversedFirst.collider.setMaterial(material(0.75f));
        reversedSecond.collider.setMaterial(material(0.25f));

        fixedStep(reversedRestitutionScene, reversedRestitutionWorld, 0.01f);

        L2D_REQUIRE_APPROX(reversedFirst.body->velocity().x, -0.625f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversedSecond.body->velocity().x, 0.875f, kPhysicsComparisonEpsilon);

        l2d::Scene kinematicScene;
        l2d::PhysicsWorld2D kinematicWorld(zeroGravityConfig());
        CircleBody kinematic = createCircle(kinematicScene, "Kinematic", {0.f, 0.f}, 1.f, true,
                                            l2d::BodyType2D::Kinematic);
        CircleBody dynamic = createCircle(kinematicScene, "Dynamic", {1.8f, 0.f}, 1.f);

        kinematic.body->setVelocity({1.f, 0.f});
        fixedStep(kinematicScene, kinematicWorld, 0.01f);

        L2D_REQUIRE_APPROX(kinematic.body->velocity().x, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(dynamic.body->velocity().x, 1.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE(kinematic.object.transform.position().x > 0.f);
    }

    float runFrictionFixture(l2d::PhysicsMaterial2D moverMaterial,
                             l2d::PhysicsMaterial2D floorMaterial, float tangentVelocity)
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody mover = createCircle(scene, "Mover", {0.f, 0.f}, 1.f);
        BoxBody floor = createBox(scene, "Floor", {-5.f, 1.8f}, {10.f, 1.f}, false);

        mover.collider.setMaterial(moverMaterial);
        floor.collider.setMaterial(floorMaterial);
        mover.body->setVelocity({tangentVelocity, 4.f});

        fixedStep(scene, world, 0.01f);

        return mover.body->velocity().x;
    }

    void testStaticAndDynamicFriction()
    {
        const float frictionless = runFrictionFixture(material(0.f), material(0.f), 4.f);
        const float staticFriction =
            runFrictionFixture(material(0.f, 1.f, 0.5f), material(0.f, 0.25f, 0.125f), 1.5f);
        const float reversedStaticFriction =
            runFrictionFixture(material(0.f, 0.25f, 0.125f), material(0.f, 1.f, 0.5f), 1.5f);
        const float dynamicFriction =
            runFrictionFixture(material(0.f, 1.f, 1.f), material(0.f, 0.25f, 0.25f), 4.f);
        const float reversedDynamicFriction =
            runFrictionFixture(material(0.f, 0.25f, 0.25f), material(0.f, 1.f, 1.f), 4.f);

        L2D_REQUIRE_APPROX(frictionless, 4.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(staticFriction, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversedStaticFriction, 0.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(dynamicFriction, 2.f, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(reversedDynamicFriction, 2.f, kPhysicsComparisonEpsilon);
    }
}

int main()
{
    int failures = 0;

    runTest("legacy defaults and input sanitization", testLegacyDefaultsAndInputSanitization,
            failures);
    runTest("world config and restitution threshold", testWorldConfigAndRestitutionThreshold,
            failures);
    runTest("body modes and free integration", testBodyModesAndFreeIntegration, failures);
    runTest("extreme integration cancellation stays finite",
            testExtremeIntegrationCancellationStaysFinite, failures);
    runTest("dynamic/static and dynamic box response", testDynamicStaticAndDynamicBoxResponse,
            failures);
    runTest("unequal mass restitution and kinematic push",
            testUnequalMassRestitutionAndKinematicPush, failures);
    runTest("static and dynamic friction", testStaticAndDynamicFriction, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics body test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics body tests passed.\n";
    return 0;
}

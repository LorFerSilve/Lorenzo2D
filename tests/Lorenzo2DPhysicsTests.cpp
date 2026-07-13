#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>
#include <Lorenzo2D/Physics/PhysicsContact2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
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

    bool approximatelyEqual(
        float left,
        float right,
        float epsilon = 0.001f
    )
    {
        return std::fabs(left - right) <= epsilon;
    }

    bool approximatelyEqual(
        sf::Vector2f left,
        sf::Vector2f right,
        float epsilon = 0.001f
    )
    {
        return approximatelyEqual(left.x, right.x, epsilon) &&
            approximatelyEqual(left.y, right.y, epsilon);
    }

    bool isFinite(sf::Vector2f value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    l2d::PhysicsWorld2DConfig zeroGravityConfig()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = { 0.f, 0.f };
        return config;
    }

    void fixedStep(
        l2d::Scene& scene,
        l2d::PhysicsWorld2D& world,
        float deltaTime = 1.f / 60.f
    )
    {
        scene.fixedUpdate(deltaTime);
        world.step(scene, deltaTime);
    }

    l2d::PhysicsMaterial2D material(
        float restitution,
        float staticFriction = 0.f,
        float dynamicFriction = 0.f
    )
    {
        return { restitution, staticFriction, dynamicFriction };
    }

    struct CircleBody
    {
        l2d::GameObject& object;
        l2d::CircleCollider2D& collider;
        l2d::RigidBody2D* body;
    };

    CircleBody createCircle(
        l2d::Scene& scene,
        const std::string& name,
        sf::Vector2f position,
        float radius,
        bool withBody = true,
        l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic
    )
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::CircleCollider2D& collider =
            object.addComponent<l2d::CircleCollider2D>(radius);

        return { object, collider, body };
    }

    struct BoxBody
    {
        l2d::GameObject& object;
        l2d::BoxCollider2D& collider;
        l2d::RigidBody2D* body;
    };

    BoxBody createBox(
        l2d::Scene& scene,
        const std::string& name,
        sf::Vector2f position,
        sf::Vector2f size,
        bool withBody = true,
        l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic
    )
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::BoxCollider2D& collider =
            object.addComponent<l2d::BoxCollider2D>(size);

        return { object, collider, body };
    }

    void testLegacyDefaultsAndInputSanitization()
    {
        l2d::RigidBody2D body;

        L2D_REQUIRE(body.bodyType() == l2d::BodyType2D::Dynamic);
        L2D_REQUIRE(approximatelyEqual(body.mass(), 1.f));
        L2D_REQUIRE(approximatelyEqual(body.inverseMass(), 1.f));
        L2D_REQUIRE(!body.useGravity());
        L2D_REQUIRE(approximatelyEqual(body.gravityScale(), 1.f));

        body.setMass(2.f);
        L2D_REQUIRE(approximatelyEqual(body.inverseMass(), 0.5f));

        body.setVelocity({
            std::numeric_limits<float>::quiet_NaN(),
            1.f
        });
        L2D_REQUIRE(approximatelyEqual(body.velocity(), { 0.f, 0.f }));

        body.setAcceleration({
            1.f,
            std::numeric_limits<float>::infinity()
        });
        L2D_REQUIRE(approximatelyEqual(body.acceleration(), { 0.f, 0.f }));

        body.applyImpulse({ 2.f, 4.f });
        L2D_REQUIRE(approximatelyEqual(body.velocity(), { 1.f, 2.f }));

        const float maximum = std::numeric_limits<float>::max();
        l2d::RigidBody2D cancellationBody;
        cancellationBody.setMass(0.5f);
        cancellationBody.setVelocity({ maximum, -maximum });
        cancellationBody.applyImpulse({ -maximum, maximum });
        L2D_REQUIRE(approximatelyEqual(
            {
                cancellationBody.velocity().x / maximum,
                cancellationBody.velocity().y / maximum
            },
            { -1.f, 1.f }
        ));

        body.setBodyType(l2d::BodyType2D::Static);
        L2D_REQUIRE(approximatelyEqual(body.inverseMass(), 0.f));
        L2D_REQUIRE(approximatelyEqual(body.velocity(), { 0.f, 0.f }));
        body.setVelocity({ 4.f, 5.f });
        L2D_REQUIRE(approximatelyEqual(body.velocity(), { 0.f, 0.f }));

        body.setBodyType(static_cast<l2d::BodyType2D>(999));
        L2D_REQUIRE(body.bodyType() == l2d::BodyType2D::Dynamic);
        L2D_REQUIRE(approximatelyEqual(body.inverseMass(), 0.5f));

        l2d::GameObject object;
        l2d::CircleCollider2D& collider =
            object.addComponent<l2d::CircleCollider2D>(2.f);

        L2D_REQUIRE(approximatelyEqual(collider.material().restitution, 0.f));
        L2D_REQUIRE(approximatelyEqual(collider.material().staticFriction, 0.f));
        L2D_REQUIRE(approximatelyEqual(collider.material().dynamicFriction, 0.f));
        L2D_REQUIRE(!collider.isSensor());
        L2D_REQUIRE(collider.filter().categoryBits == 1u);
        L2D_REQUIRE(collider.filter().maskBits ==
            std::numeric_limits<std::uint32_t>::max());

        collider.setMaterial({
            2.f,
            std::numeric_limits<float>::quiet_NaN(),
            0.75f
        });

        L2D_REQUIRE(approximatelyEqual(collider.material().restitution, 1.f));
        L2D_REQUIRE(approximatelyEqual(collider.material().staticFriction, 0.f));
        L2D_REQUIRE(approximatelyEqual(collider.material().dynamicFriction, 0.f));

        collider.setMaterial({ 0.5f, 0.8f, 1.f });
        L2D_REQUIRE(approximatelyEqual(collider.material().restitution, 0.5f));
        L2D_REQUIRE(approximatelyEqual(collider.material().staticFriction, 0.8f));
        L2D_REQUIRE(approximatelyEqual(collider.material().dynamicFriction, 0.8f));

        collider.setOffset({
            std::numeric_limits<float>::infinity(),
            -3.f
        });
        L2D_REQUIRE(approximatelyEqual(collider.offset(), { 0.f, -3.f }));
    }

    void testWorldConfigAndRestitutionThreshold()
    {
        l2d::PhysicsWorld2D world;
        l2d::PhysicsWorld2DConfig invalidConfig;
        invalidConfig.gravity = {
            std::numeric_limits<float>::quiet_NaN(),
            10.f
        };
        invalidConfig.velocityIterations = 0;
        invalidConfig.positionIterations = 0;
        invalidConfig.positionCorrectionPercent = 2.f;
        invalidConfig.penetrationSlop = -1.f;
        invalidConfig.restitutionVelocityThreshold =
            std::numeric_limits<float>::infinity();
        invalidConfig.groundedNormalThreshold = -1.f;

        world.setConfig(invalidConfig);

        const l2d::PhysicsWorld2DConfig& sanitized = world.config();
        L2D_REQUIRE(approximatelyEqual(sanitized.gravity, { 0.f, 980.f }));
        L2D_REQUIRE(sanitized.velocityIterations == 8u);
        L2D_REQUIRE(sanitized.positionIterations == 3u);
        L2D_REQUIRE(approximatelyEqual(
            sanitized.positionCorrectionPercent,
            1.f
        ));
        L2D_REQUIRE(approximatelyEqual(sanitized.penetrationSlop, 0.01f));
        L2D_REQUIRE(approximatelyEqual(
            sanitized.restitutionVelocityThreshold,
            1.f
        ));
        L2D_REQUIRE(approximatelyEqual(
            sanitized.groundedNormalThreshold,
            0.f
        ));

        l2d::PhysicsWorld2DConfig thresholdConfig = zeroGravityConfig();
        thresholdConfig.positionCorrectionPercent = 0.f;
        thresholdConfig.restitutionVelocityThreshold = 2.f;
        l2d::PhysicsWorld2D thresholdWorld(thresholdConfig);
        l2d::Scene suppressedScene;
        CircleBody suppressedMover = createCircle(
            suppressedScene,
            "SuppressedMover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody suppressedObstacle = createCircle(
            suppressedScene,
            "SuppressedObstacle",
            { 1.5f, 0.f },
            1.f,
            false
        );
        suppressedMover.collider.setMaterial(material(1.f));
        suppressedObstacle.collider.setMaterial(material(1.f));
        suppressedMover.body->setVelocity({ 1.f, 0.f });

        fixedStep(suppressedScene, thresholdWorld, 0.01f);
        L2D_REQUIRE(approximatelyEqual(
            suppressedMover.body->velocity().x,
            0.f
        ));

        thresholdConfig.restitutionVelocityThreshold = 0.f;
        thresholdWorld.setConfig(thresholdConfig);
        l2d::Scene bounceScene;
        CircleBody bounceMover = createCircle(
            bounceScene,
            "BounceMover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody bounceObstacle = createCircle(
            bounceScene,
            "BounceObstacle",
            { 1.5f, 0.f },
            1.f,
            false
        );
        bounceMover.collider.setMaterial(material(1.f));
        bounceObstacle.collider.setMaterial(material(1.f));
        bounceMover.body->setVelocity({ 1.f, 0.f });

        fixedStep(bounceScene, thresholdWorld, 0.01f);
        L2D_REQUIRE(approximatelyEqual(
            bounceMover.body->velocity().x,
            -1.f
        ));
    }

    void testBodyModesAndFreeIntegration()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.gravity = { 0.f, 10.f };
        l2d::PhysicsWorld2D world(config);

        l2d::GameObject& dynamicObject =
            scene.createGameObject("Dynamic");
        l2d::RigidBody2D& dynamicBody =
            dynamicObject.addComponent<l2d::RigidBody2D>();
        dynamicBody.setVelocity({ 1.f, 0.f });
        dynamicBody.setAcceleration({ 2.f, 0.f });
        dynamicBody.setUseGravity(true);
        dynamicBody.setGravityScale(0.5f);

        l2d::GameObject& kinematicObject =
            scene.createGameObject("Kinematic");
        kinematicObject.transform.setPosition({ 10.f, 0.f });
        l2d::RigidBody2D& kinematicBody =
            kinematicObject.addComponent<l2d::RigidBody2D>();
        kinematicBody.setBodyType(l2d::BodyType2D::Kinematic);
        kinematicBody.setVelocity({ 3.f, 4.f });
        kinematicBody.setAcceleration({ 100.f, 100.f });
        kinematicBody.setUseGravity(true);
        kinematicBody.addForce({ 100.f, 100.f });

        l2d::GameObject& staticObject = scene.createGameObject("Static");
        staticObject.transform.setPosition({ 20.f, 0.f });
        l2d::RigidBody2D& staticBody =
            staticObject.addComponent<l2d::RigidBody2D>();
        staticBody.setBodyType(l2d::BodyType2D::Static);
        staticBody.setVelocity({ 20.f, 20.f });

        fixedStep(scene, world, 0.5f);

        L2D_REQUIRE(approximatelyEqual(dynamicBody.velocity(), { 2.f, 2.5f }));
        L2D_REQUIRE(approximatelyEqual(
            dynamicObject.transform.position(),
            { 1.f, 1.25f }
        ));
        L2D_REQUIRE(approximatelyEqual(kinematicBody.velocity(), { 3.f, 4.f }));
        L2D_REQUIRE(approximatelyEqual(
            kinematicObject.transform.position(),
            { 11.5f, 2.f }
        ));
        L2D_REQUIRE(approximatelyEqual(
            staticObject.transform.position(),
            { 20.f, 0.f }
        ));
        L2D_REQUIRE(approximatelyEqual(staticBody.velocity(), { 0.f, 0.f }));

        l2d::Scene forceScene;
        l2d::PhysicsWorld2D forceWorld(zeroGravityConfig());
        l2d::GameObject& forceObject = forceScene.createGameObject("Force");
        l2d::RigidBody2D& forceBody =
            forceObject.addComponent<l2d::RigidBody2D>();
        forceBody.setMass(2.f);
        forceBody.addForce({ 4.f, 0.f });

        fixedStep(forceScene, forceWorld, 0.5f);
        L2D_REQUIRE(approximatelyEqual(forceBody.velocity().x, 1.f));
        fixedStep(forceScene, forceWorld, 0.5f);
        L2D_REQUIRE(approximatelyEqual(forceBody.velocity().x, 1.f));
    }

    void testExtremeIntegrationCancellationStaysFinite()
    {
        const float maximum = std::numeric_limits<float>::max();

        l2d::Scene dynamicScene;
        l2d::PhysicsWorld2D dynamicWorld(zeroGravityConfig());
        l2d::GameObject& dynamicObject =
            dynamicScene.createGameObject("ExtremeDynamic");
        l2d::RigidBody2D& dynamicBody =
            dynamicObject.addComponent<l2d::RigidBody2D>();
        dynamicBody.setVelocity({ -maximum, 0.f });
        dynamicBody.setAcceleration({ maximum, 0.f });

        fixedStep(dynamicScene, dynamicWorld, 2.f);

        L2D_REQUIRE(isFinite(dynamicBody.velocity()));
        L2D_REQUIRE(approximatelyEqual(
            dynamicBody.velocity().x / maximum,
            1.f
        ));
        L2D_REQUIRE(isFinite(dynamicObject.transform.position()));

        l2d::Scene kinematicScene;
        l2d::PhysicsWorld2D kinematicWorld(zeroGravityConfig());
        l2d::GameObject& kinematicObject =
            kinematicScene.createGameObject("ExtremeKinematic");
        kinematicObject.transform.setPosition({ -maximum, 0.f });
        l2d::RigidBody2D& kinematicBody =
            kinematicObject.addComponent<l2d::RigidBody2D>();
        kinematicBody.setBodyType(l2d::BodyType2D::Kinematic);
        kinematicBody.setVelocity({ maximum, 0.f });

        fixedStep(kinematicScene, kinematicWorld, 2.f);

        L2D_REQUIRE(isFinite(kinematicObject.transform.position()));
        L2D_REQUIRE(approximatelyEqual(
            kinematicObject.transform.position().x / maximum,
            1.f
        ));
    }

    void testDynamicStaticAndDynamicBoxResponse()
    {
        l2d::Scene circleScene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody mover = createCircle(
            circleScene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody obstacle = createCircle(
            circleScene,
            "Obstacle",
            { 1.5f, 0.f },
            1.f,
            false
        );

        mover.body->setVelocity({ 1.f, 0.f });
        fixedStep(circleScene, world, 0.01f);

        L2D_REQUIRE(approximatelyEqual(mover.body->velocity().x, 0.f));
        L2D_REQUIRE(approximatelyEqual(
            obstacle.object.transform.position(),
            { 1.5f, 0.f }
        ));
        L2D_REQUIRE(world.isTouching(mover.object.id(), obstacle.object.id()));
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(obstacle.collider.isColliding());

        l2d::Scene boxScene;
        l2d::PhysicsWorld2D boxWorld(zeroGravityConfig());
        BoxBody boxMover = createBox(
            boxScene,
            "BoxMover",
            { 0.f, 0.f },
            { 2.f, 2.f }
        );
        BoxBody boxObstacle = createBox(
            boxScene,
            "BoxObstacle",
            { 1.8f, 0.f },
            { 2.f, 2.f },
            false
        );

        boxMover.body->setVelocity({ 1.f, 0.f });
        fixedStep(boxScene, boxWorld, 0.01f);

        L2D_REQUIRE(approximatelyEqual(boxMover.body->velocity().x, 0.f));
        L2D_REQUIRE(boxWorld.contacts().size() == 1);
        L2D_REQUIRE(
            boxWorld.contacts()[0].firstColliderType ==
            l2d::ColliderType::Box
        );
        L2D_REQUIRE(
            boxWorld.contacts()[0].secondColliderType ==
            l2d::ColliderType::Box
        );
        L2D_REQUIRE(approximatelyEqual(
            boxObstacle.object.transform.position(),
            { 1.8f, 0.f }
        ));
    }

    void testUnequalMassRestitutionAndKinematicPush()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.restitutionVelocityThreshold = 0.f;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        CircleBody first = createCircle(
            scene,
            "First",
            { 0.f, 0.f },
            1.f
        );
        CircleBody second = createCircle(
            scene,
            "Second",
            { 1.8f, 0.f },
            1.f
        );

        first.body->setMass(1.f);
        second.body->setMass(3.f);
        first.body->setVelocity({ 2.f, 0.f });
        second.body->setVelocity({ 0.f, 0.f });
        first.collider.setMaterial(material(0.25f));
        second.collider.setMaterial(material(0.75f));

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(approximatelyEqual(first.body->velocity().x, -0.625f));
        L2D_REQUIRE(approximatelyEqual(second.body->velocity().x, 0.875f));

        l2d::Scene reversedRestitutionScene;
        l2d::PhysicsWorld2D reversedRestitutionWorld(config);
        CircleBody reversedFirst = createCircle(
            reversedRestitutionScene,
            "ReversedFirst",
            { 0.f, 0.f },
            1.f
        );
        CircleBody reversedSecond = createCircle(
            reversedRestitutionScene,
            "ReversedSecond",
            { 1.8f, 0.f },
            1.f
        );

        reversedFirst.body->setMass(1.f);
        reversedSecond.body->setMass(3.f);
        reversedFirst.body->setVelocity({ 2.f, 0.f });
        reversedFirst.collider.setMaterial(material(0.75f));
        reversedSecond.collider.setMaterial(material(0.25f));

        fixedStep(
            reversedRestitutionScene,
            reversedRestitutionWorld,
            0.01f
        );

        L2D_REQUIRE(approximatelyEqual(
            reversedFirst.body->velocity().x,
            -0.625f
        ));
        L2D_REQUIRE(approximatelyEqual(
            reversedSecond.body->velocity().x,
            0.875f
        ));

        l2d::Scene kinematicScene;
        l2d::PhysicsWorld2D kinematicWorld(zeroGravityConfig());
        CircleBody kinematic = createCircle(
            kinematicScene,
            "Kinematic",
            { 0.f, 0.f },
            1.f,
            true,
            l2d::BodyType2D::Kinematic
        );
        CircleBody dynamic = createCircle(
            kinematicScene,
            "Dynamic",
            { 1.8f, 0.f },
            1.f
        );

        kinematic.body->setVelocity({ 1.f, 0.f });
        fixedStep(kinematicScene, kinematicWorld, 0.01f);

        L2D_REQUIRE(approximatelyEqual(kinematic.body->velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(dynamic.body->velocity().x, 1.f));
        L2D_REQUIRE(kinematic.object.transform.position().x > 0.f);
    }

    float runFrictionFixture(
        l2d::PhysicsMaterial2D moverMaterial,
        l2d::PhysicsMaterial2D floorMaterial,
        float tangentVelocity
    )
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody mover = createCircle(
            scene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        BoxBody floor = createBox(
            scene,
            "Floor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );

        mover.collider.setMaterial(moverMaterial);
        floor.collider.setMaterial(floorMaterial);
        mover.body->setVelocity({ tangentVelocity, 4.f });

        fixedStep(scene, world, 0.01f);

        return mover.body->velocity().x;
    }

    void testStaticAndDynamicFriction()
    {
        const float frictionless = runFrictionFixture(
            material(0.f),
            material(0.f),
            4.f
        );
        const float staticFriction = runFrictionFixture(
            material(0.f, 1.f, 0.5f),
            material(0.f, 0.25f, 0.125f),
            1.5f
        );
        const float reversedStaticFriction = runFrictionFixture(
            material(0.f, 0.25f, 0.125f),
            material(0.f, 1.f, 0.5f),
            1.5f
        );
        const float dynamicFriction = runFrictionFixture(
            material(0.f, 1.f, 1.f),
            material(0.f, 0.25f, 0.25f),
            4.f
        );
        const float reversedDynamicFriction = runFrictionFixture(
            material(0.f, 0.25f, 0.25f),
            material(0.f, 1.f, 1.f),
            4.f
        );

        L2D_REQUIRE(approximatelyEqual(frictionless, 4.f));
        L2D_REQUIRE(approximatelyEqual(staticFriction, 0.f));
        L2D_REQUIRE(approximatelyEqual(reversedStaticFriction, 0.f));
        L2D_REQUIRE(approximatelyEqual(dynamicFriction, 2.f));
        L2D_REQUIRE(approximatelyEqual(reversedDynamicFriction, 2.f));
    }

    void testCircleCircleManifolds()
    {
        l2d::GameObject firstObject;
        l2d::CircleCollider2D& first =
            firstObject.addComponent<l2d::CircleCollider2D>(2.f);

        l2d::GameObject secondObject;
        l2d::CircleCollider2D& second =
            secondObject.addComponent<l2d::CircleCollider2D>(2.f);

        secondObject.transform.setPosition({ 3.f, 0.f });

        l2d::CollisionManifold2D manifold;
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 1.f));
        L2D_REQUIRE(isFinite(manifold.point));

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE(approximatelyEqual(reversed.normal, { -1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(reversed.penetration, 1.f));

        secondObject.transform.setPosition({ 4.f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 0.f));

        secondObject.transform.setPosition({ 0.f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 4.f));

        secondObject.transform.setPosition({ 5.f, 0.f });
        L2D_REQUIRE(!l2d::computeCollisionManifold(first, second, manifold));
    }

    void testBoxBoxManifolds()
    {
        l2d::GameObject firstObject;
        l2d::BoxCollider2D& first =
            firstObject.addComponent<l2d::BoxCollider2D>(
                sf::Vector2f{ 4.f, 4.f }
            );

        l2d::GameObject secondObject;
        l2d::BoxCollider2D& second =
            secondObject.addComponent<l2d::BoxCollider2D>(
                sf::Vector2f{ 4.f, 4.f }
            );

        l2d::CollisionManifold2D manifold;

        secondObject.transform.setPosition({ 3.f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 1.f));

        secondObject.transform.setPosition({ 0.f, 3.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 0.f, 1.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 1.f));

        secondObject.transform.setPosition({ 2.f, 2.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 2.f));

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE(approximatelyEqual(reversed.normal, { -1.f, 0.f }));

        secondObject.transform.setPosition({ 4.f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 0.f));

        secondObject.transform.setPosition({ 4.01f, 0.f });
        L2D_REQUIRE(!l2d::computeCollisionManifold(first, second, manifold));

        firstObject.transform.setPosition({ 4.f, 4.f });
        first.setSize({ 2.f, 2.f });
        secondObject.transform.setPosition({ 0.f, 0.f });
        second.setSize({ 10.f, 10.f });

        L2D_REQUIRE(l2d::computeCollisionManifold(first, second, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 6.f));

        L2D_REQUIRE(l2d::computeCollisionManifold(second, first, reversed));
        L2D_REQUIRE(approximatelyEqual(reversed.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(reversed.penetration, 6.f));
    }

    void testCircleBoxManifolds()
    {
        l2d::GameObject circleObject;
        l2d::CircleCollider2D& circle =
            circleObject.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::GameObject boxObject;
        l2d::BoxCollider2D& box =
            boxObject.addComponent<l2d::BoxCollider2D>(
                sf::Vector2f{ 2.f, 2.f }
            );

        l2d::CollisionManifold2D manifold;

        boxObject.transform.setPosition({ 1.5f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 0.5f));

        l2d::CollisionManifold2D reversed;
        L2D_REQUIRE(l2d::computeCollisionManifold(box, circle, reversed));
        L2D_REQUIRE(approximatelyEqual(reversed.normal, { -1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(reversed.penetration, 0.5f));

        boxObject.transform.setPosition({ 2.f, 0.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 0.f));

        boxObject.transform.setPosition({ 1.5f, 1.5f });
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE(approximatelyEqual(
            manifold.normal,
            { 0.70710677f, 0.70710677f }
        ));
        L2D_REQUIRE(approximatelyEqual(
            manifold.penetration,
            1.f - std::sqrt(0.5f)
        ));

        boxObject.transform.setPosition({ 0.f, 0.f });
        circleObject.transform.setPosition({ 1.f, 1.f });
        box.setSize({ 4.f, 4.f });
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, box, manifold));
        L2D_REQUIRE(approximatelyEqual(manifold.normal, { 1.f, 0.f }));
        L2D_REQUIRE(approximatelyEqual(manifold.penetration, 3.f));
        L2D_REQUIRE(isFinite(manifold.point));
    }

    void testCollisionFilters()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody first = createCircle(
            scene,
            "First",
            { 0.f, 0.f },
            1.f
        );
        CircleBody second = createCircle(
            scene,
            "Second",
            { 1.5f, 0.f },
            1.f,
            false
        );

        first.collider.setFilter({ 1u, 2u });
        second.collider.setFilter({ 4u, 1u });

        L2D_REQUIRE(!first.collider.canCollideWith(second.collider));
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(!first.collider.isColliding());
        L2D_REQUIRE(!second.collider.isColliding());

        second.collider.setFilter({ 2u, 1u });
        L2D_REQUIRE(first.collider.canCollideWith(second.collider));
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Begin
        );

        first.collider.setFilter({ 1u, 0u });
        fixedStep(scene, world);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::End
        );
    }

    void testStaticPairsAreNotReported()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        BoxBody first = createBox(
            scene,
            "FirstStatic",
            { 0.f, 0.f },
            { 2.f, 2.f },
            false
        );
        BoxBody second = createBox(
            scene,
            "SecondStatic",
            { 1.f, 0.f },
            { 2.f, 2.f },
            false
        );

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
        CircleBody mover = createCircle(
            scene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody sensor = createCircle(
            scene,
            "Sensor",
            { 1.5f, 0.f },
            1.f,
            false
        );
        sensor.collider.setSensor(true);
        mover.body->setVelocity({ 1.f, 0.f });

        const sf::Vector2f sensorPosition =
            sensor.object.transform.position();

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.contacts()[0].sensor);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Begin
        );
        L2D_REQUIRE(world.isTouching(mover.object.id(), sensor.object.id()));
        L2D_REQUIRE(world.isTouching(sensor.object.id(), mover.object.id()));
        L2D_REQUIRE(approximatelyEqual(mover.body->velocity().x, 1.f));
        L2D_REQUIRE(approximatelyEqual(
            mover.object.transform.position(),
            { 0.01f, 0.f }
        ));
        L2D_REQUIRE(approximatelyEqual(
            sensor.object.transform.position(),
            sensorPosition
        ));

        fixedStep(scene, world, 0.01f);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Stay
        );

        mover.object.transform.setPosition({ 10.f, 0.f });
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::End
        );
        L2D_REQUIRE(!mover.collider.isColliding());
        L2D_REQUIRE(!sensor.collider.isColliding());
    }

    void testDeactivationAndDestructionEmitEnd()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody first = createCircle(
            scene,
            "First",
            { 0.f, 0.f },
            1.f
        );
        CircleBody second = createCircle(
            scene,
            "Second",
            { 1.5f, 0.f },
            1.f,
            false
        );
        second.collider.setSensor(true);

        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.isTouching(first.object.id(), second.object.id()));

        second.collider.setActive(false);
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::End
        );

        second.collider.setActive(true);
        fixedStep(scene, world);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Begin
        );

        second.object.destroy();
        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::End
        );
    }

    void testInvalidDeltaTimeIsANoOp()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody mover = createCircle(
            scene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody sensor = createCircle(
            scene,
            "Sensor",
            { 1.5f, 0.f },
            1.f,
            false
        );
        sensor.collider.setSensor(true);

        fixedStep(scene, world);
        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.contactEvents().size() == 1);

        mover.body->addForce({ 60.f, 0.f });
        const sf::Vector2f positionBeforeInvalidStep =
            mover.object.transform.position();

        world.step(scene, 0.f);
        world.step(scene, std::numeric_limits<float>::quiet_NaN());

        L2D_REQUIRE(approximatelyEqual(
            mover.object.transform.position(),
            positionBeforeInvalidStep
        ));
        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Begin
        );

        mover.object.transform.setPosition({ 10.f, 0.f });
        fixedStep(scene, world, 1.f / 60.f);
        L2D_REQUIRE(approximatelyEqual(mover.body->velocity().x, 1.f));
    }

    void testGroundedUsesOnlySolidSupportContacts()
    {
        l2d::Scene floorScene;
        l2d::PhysicsWorld2D floorWorld(zeroGravityConfig());
        CircleBody mover = createCircle(
            floorScene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        createBox(
            floorScene,
            "Floor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        mover.body->setVelocity({ 0.f, 4.f });

        fixedStep(floorScene, floorWorld, 0.01f);
        L2D_REQUIRE(mover.body->isGrounded());

        l2d::Scene staticFirstScene;
        l2d::PhysicsWorld2D staticFirstWorld(zeroGravityConfig());
        BoxBody staticFirstFloor = createBox(
            staticFirstScene,
            "StaticFirstFloor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        CircleBody staticFirstMover = createCircle(
            staticFirstScene,
            "StaticFirstMover",
            { 0.f, 0.f },
            1.f
        );
        staticFirstMover.body->setVelocity({ 0.f, 4.f });

        fixedStep(staticFirstScene, staticFirstWorld, 0.01f);

        L2D_REQUIRE(staticFirstMover.body->isGrounded());
        L2D_REQUIRE(approximatelyEqual(
            staticFirstMover.body->velocity().y,
            0.f
        ));
        L2D_REQUIRE(staticFirstWorld.contacts().size() == 1);
        L2D_REQUIRE(
            staticFirstWorld.contacts()[0].firstObjectId ==
            staticFirstFloor.object.id()
        );
        L2D_REQUIRE(
            staticFirstWorld.contacts()[0].secondObjectId ==
            staticFirstMover.object.id()
        );
        L2D_REQUIRE(staticFirstWorld.contacts()[0].manifold.normal.y < 0.f);
        L2D_REQUIRE(approximatelyEqual(
            staticFirstMover.object.transform.position(),
            mover.object.transform.position()
        ));

        l2d::Scene wallScene;
        l2d::PhysicsWorld2D wallWorld(zeroGravityConfig());
        CircleBody wallMover = createCircle(
            wallScene,
            "WallMover",
            { 0.f, 0.f },
            1.f
        );
        createBox(
            wallScene,
            "Wall",
            { 1.8f, -5.f },
            { 1.f, 10.f },
            false
        );
        wallMover.body->setVelocity({ 4.f, 0.f });

        fixedStep(wallScene, wallWorld, 0.01f);
        L2D_REQUIRE(!wallMover.body->isGrounded());

        l2d::Scene sensorScene;
        l2d::PhysicsWorld2D sensorWorld(zeroGravityConfig());
        CircleBody sensorMover = createCircle(
            sensorScene,
            "SensorMover",
            { 0.f, 0.f },
            1.f
        );
        BoxBody sensorFloor = createBox(
            sensorScene,
            "SensorFloor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        sensorFloor.collider.setSensor(true);
        sensorMover.body->setVelocity({ 0.f, 4.f });

        fixedStep(sensorScene, sensorWorld, 0.01f);
        L2D_REQUIRE(!sensorMover.body->isGrounded());
    }

    void testContactsHaveDeterministicOrdering()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        CircleBody first = createCircle(
            scene,
            "First",
            { 0.f, 0.f },
            2.f
        );
        CircleBody second = createCircle(
            scene,
            "Second",
            { 1.f, 0.f },
            2.f
        );
        CircleBody third = createCircle(
            scene,
            "Third",
            { 2.f, 0.f },
            2.f
        );

        first.collider.setSensor(true);
        second.collider.setSensor(true);
        third.collider.setSensor(true);

        fixedStep(scene, world);

        L2D_REQUIRE(world.contacts().size() == 3);
        L2D_REQUIRE(world.contactEvents().size() == 3);

        for (std::size_t index = 0; index < world.contacts().size(); ++index)
        {
            const l2d::PhysicsContact2D& contact = world.contacts()[index];

            L2D_REQUIRE(contact.firstObjectId < contact.secondObjectId);
            L2D_REQUIRE(isFinite(contact.manifold.normal));
            L2D_REQUIRE(isFinite(contact.manifold.point));
            L2D_REQUIRE(contact.manifold.penetration >= 0.f);

            if (index == 0)
                continue;

            const l2d::PhysicsContact2D& previous =
                world.contacts()[index - 1];

            L2D_REQUIRE(
                std::tie(previous.firstObjectId, previous.secondObjectId) <
                std::tie(contact.firstObjectId, contact.secondObjectId)
            );
        }

        fixedStep(scene, world);

        L2D_REQUIRE(world.contactEvents().size() == 3);

        for (const l2d::PhysicsContactEvent2D& event : world.contactEvents())
        {
            L2D_REQUIRE(event.phase == l2d::PhysicsContactPhase2D::Stay);
        }

        l2d::Scene otherScene;
        fixedStep(otherScene, world);
        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());

        fixedStep(scene, world);
        L2D_REQUIRE(world.contactEvents().size() == 3);

        for (const l2d::PhysicsContactEvent2D& event : world.contactEvents())
        {
            L2D_REQUIRE(event.phase == l2d::PhysicsContactPhase2D::Begin);
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
        BoxBody mover = createBox(
            scene,
            "MaximumMassMover",
            { 0.f, 0.f },
            { 2.f, 2.f }
        );
        BoxBody obstacle = createBox(
            scene,
            "StaticObstacle",
            { 0.5f, 0.f },
            { 2.f, 2.f },
            false
        );

        mover.body->setMass(std::numeric_limits<float>::max());
        mover.body->setVelocity({ 1.f, 0.f });

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(obstacle.collider.isColliding());
        L2D_REQUIRE(isFinite(mover.object.transform.position()));
        L2D_REQUIRE(isFinite(mover.body->velocity()));
        L2D_REQUIRE(mover.object.transform.position().x < -1.f);
        L2D_REQUIRE(approximatelyEqual(
            mover.body->velocity().x,
            0.f,
            0.01f
        ));
        L2D_REQUIRE(approximatelyEqual(
            obstacle.object.transform.position(),
            { 0.5f, 0.f }
        ));
    }

    void testExtremeRestitutionSwapsFiniteVelocities()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.restitutionVelocityThreshold = 0.f;
        config.positionCorrectionPercent = 0.f;
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);

        CircleBody first = createCircle(
            scene,
            "FirstExtremeBody",
            { 0.f, 0.f },
            10.f
        );
        CircleBody second = createCircle(
            scene,
            "SecondExtremeBody",
            { 10.f, 0.f },
            10.f
        );

        const float maximum = std::numeric_limits<float>::max();
        first.body->setVelocity({ maximum, 0.f });
        second.body->setVelocity({ -maximum, 0.f });
        first.collider.setMaterial(material(1.f));
        second.collider.setMaterial(material(1.f));

        fixedStep(
            scene,
            world,
            std::numeric_limits<float>::min()
        );

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(isFinite(first.body->velocity()));
        L2D_REQUIRE(isFinite(second.body->velocity()));
        L2D_REQUIRE(approximatelyEqual(
            first.body->velocity().x / maximum,
            -1.f
        ));
        L2D_REQUIRE(approximatelyEqual(
            second.body->velocity().x / maximum,
            1.f
        ));
        L2D_REQUIRE(approximatelyEqual(first.body->velocity().y, 0.f));
        L2D_REQUIRE(approximatelyEqual(second.body->velocity().y, 0.f));
    }

    void testSceneAddressReuseStartsFreshContactHistory()
    {
        std::optional<l2d::Scene> sceneSlot;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        sceneSlot.emplace("FirstScene");
        const std::uintptr_t firstSceneAddress =
            reinterpret_cast<std::uintptr_t>(&sceneSlot.value());

        {
            CircleBody firstMover = createCircle(
                sceneSlot.value(),
                "FirstMover",
                { 0.f, 0.f },
                1.f
            );
            CircleBody firstSensor = createCircle(
                sceneSlot.value(),
                "FirstSensor",
                { 1.5f, 0.f },
                1.f,
                false
            );
            firstSensor.collider.setSensor(true);

            fixedStep(sceneSlot.value(), world);

            L2D_REQUIRE(world.contacts().size() == 1);
            L2D_REQUIRE(world.contactEvents().size() == 1);
            L2D_REQUIRE(
                world.contactEvents()[0].phase ==
                l2d::PhysicsContactPhase2D::Begin
            );
            L2D_REQUIRE(world.isTouching(
                firstMover.object.id(),
                firstSensor.object.id()
            ));
        }

        sceneSlot.reset();
        sceneSlot.emplace("SecondScene");

        L2D_REQUIRE(
            reinterpret_cast<std::uintptr_t>(&sceneSlot.value()) ==
            firstSceneAddress
        );

        CircleBody secondMover = createCircle(
            sceneSlot.value(),
            "SecondMover",
            { 0.f, 0.f },
            1.f
        );
        CircleBody secondSensor = createCircle(
            sceneSlot.value(),
            "SecondSensor",
            { 1.5f, 0.f },
            1.f,
            false
        );
        secondSensor.collider.setSensor(true);

        fixedStep(sceneSlot.value(), world);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(world.contactEvents().size() == 1);
        L2D_REQUIRE(
            world.contactEvents()[0].phase ==
            l2d::PhysicsContactPhase2D::Begin
        );
        L2D_REQUIRE(world.isTouching(
            secondMover.object.id(),
            secondSensor.object.id()
        ));
        L2D_REQUIRE(
            world.contactEvents()[0].contact.firstObjectId ==
            secondMover.object.id()
        );
        L2D_REQUIRE(
            world.contactEvents()[0].contact.secondObjectId ==
            secondSensor.object.id()
        );
    }

    void testResetSceneClearsPhysicsFlags()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody mover = createCircle(
            scene,
            "Mover",
            { 0.f, 0.f },
            1.f
        );
        BoxBody floor = createBox(
            scene,
            "Floor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        mover.body->setVelocity({ 0.f, 4.f });

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(mover.collider.isColliding());
        L2D_REQUIRE(floor.collider.isColliding());
        L2D_REQUIRE(mover.body->isGrounded());

        world.reset(scene);

        L2D_REQUIRE(world.contacts().empty());
        L2D_REQUIRE(world.contactEvents().empty());
        L2D_REQUIRE(!world.isTouching(mover.object.id(), floor.object.id()));
        L2D_REQUIRE(!mover.collider.isColliding());
        L2D_REQUIRE(!floor.collider.isColliding());
        L2D_REQUIRE(!mover.body->isGrounded());
    }

    void testKinematicBodiesNeverBecomeGrounded()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        CircleBody kinematic = createCircle(
            scene,
            "Kinematic",
            { 0.f, 0.f },
            1.f,
            true,
            l2d::BodyType2D::Kinematic
        );
        BoxBody dynamicSupport = createBox(
            scene,
            "DynamicSupport",
            { -5.f, 1.8f },
            { 10.f, 1.f }
        );
        kinematic.body->setVelocity({ 0.f, 4.f });

        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(
            kinematic.body->bodyType() == l2d::BodyType2D::Kinematic
        );
        L2D_REQUIRE(!kinematic.body->isGrounded());
        L2D_REQUIRE(!dynamicSupport.body->isGrounded());

        l2d::Scene transitionScene;
        l2d::PhysicsWorld2D transitionWorld(zeroGravityConfig());
        CircleBody transitioningBody = createCircle(
            transitionScene,
            "TransitioningBody",
            { 0.f, 0.f },
            1.f
        );
        createBox(
            transitionScene,
            "TransitionFloor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        transitioningBody.body->setVelocity({ 0.f, 4.f });

        fixedStep(transitionScene, transitionWorld, 0.01f);
        L2D_REQUIRE(transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Kinematic);
        L2D_REQUIRE(!transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Dynamic);
        transitioningBody.object.transform.setPosition({ 0.f, 0.f });
        transitioningBody.body->setVelocity({ 0.f, 4.f });
        fixedStep(transitionScene, transitionWorld, 0.01f);
        L2D_REQUIRE(transitioningBody.body->isGrounded());

        transitioningBody.body->setBodyType(l2d::BodyType2D::Static);
        L2D_REQUIRE(!transitioningBody.body->isGrounded());
    }

    void testOnlyFirstBaseColliderParticipates()
    {
        l2d::Scene scene;
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        l2d::GameObject& mover = scene.createGameObject("Mover");
        l2d::RigidBody2D& body =
            mover.addComponent<l2d::RigidBody2D>();
        l2d::BoxCollider2D& participatingBox =
            mover.addComponent<l2d::BoxCollider2D>(
                sf::Vector2f{ 2.f, 2.f }
            );
        l2d::CircleCollider2D& ignoredCircle =
            mover.addComponent<l2d::CircleCollider2D>(1.f);

        l2d::GameObject& obstacle = scene.createGameObject("Obstacle");
        obstacle.transform.setPosition({ 1.5f, 0.f });
        l2d::BoxCollider2D& obstacleBox =
            obstacle.addComponent<l2d::BoxCollider2D>(
                sf::Vector2f{ 2.f, 2.f }
            );

        body.setVelocity({ 1.f, 0.f });
        fixedStep(scene, world, 0.01f);

        L2D_REQUIRE(world.contacts().size() == 1);
        L2D_REQUIRE(participatingBox.isColliding());
        L2D_REQUIRE(!ignoredCircle.isColliding());
        L2D_REQUIRE(obstacleBox.isColliding());
        L2D_REQUIRE(
            world.contacts()[0].firstColliderType ==
            l2d::ColliderType::Box
        );
        L2D_REQUIRE(
            world.contacts()[0].secondColliderType ==
            l2d::ColliderType::Box
        );
    }

    void testShortDynamicBoxStackRemainsFinite()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = { 0.f, 30.f };
        config.velocityIterations = 12;
        config.positionIterations = 6;
        l2d::PhysicsWorld2D world(config);
        l2d::Scene scene;

        createBox(
            scene,
            "Floor",
            { 0.f, 5.f },
            { 5.f, 1.f },
            false
        );
        BoxBody lower = createBox(
            scene,
            "Lower",
            { 2.f, 4.f },
            { 1.f, 1.f }
        );
        BoxBody upper = createBox(
            scene,
            "Upper",
            { 2.f, 3.f },
            { 1.f, 1.f }
        );
        lower.body->setUseGravity(true);
        upper.body->setUseGravity(true);

        for (std::size_t tick = 0; tick < 240; ++tick)
            fixedStep(scene, world, 1.f / 120.f);

        L2D_REQUIRE(isFinite(lower.object.transform.position()));
        L2D_REQUIRE(isFinite(upper.object.transform.position()));
        L2D_REQUIRE(isFinite(lower.body->velocity()));
        L2D_REQUIRE(isFinite(upper.body->velocity()));
        L2D_REQUIRE(lower.object.transform.position().y < 5.f);
        L2D_REQUIRE(upper.object.transform.position().y <
            lower.object.transform.position().y);
        L2D_REQUIRE(std::fabs(lower.body->velocity().y) < 10.f);
        L2D_REQUIRE(std::fabs(upper.body->velocity().y) < 10.f);
    }

    void testLegacyTangentAndOutwardVelocityRemain()
    {
        l2d::Scene inwardScene;
        l2d::PhysicsWorld2D inwardWorld(zeroGravityConfig());
        CircleBody inward = createCircle(
            inwardScene,
            "Inward",
            { 0.f, 0.f },
            1.f
        );
        createBox(
            inwardScene,
            "Floor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        inward.body->setVelocity({ 3.f, 4.f });
        fixedStep(inwardScene, inwardWorld, 0.01f);

        L2D_REQUIRE(approximatelyEqual(inward.body->velocity().x, 3.f));
        L2D_REQUIRE(approximatelyEqual(inward.body->velocity().y, 0.f));
        L2D_REQUIRE(inward.body->isGrounded());

        l2d::Scene outwardScene;
        l2d::PhysicsWorld2D outwardWorld(zeroGravityConfig());
        CircleBody outward = createCircle(
            outwardScene,
            "Outward",
            { 0.f, 0.f },
            1.f
        );
        createBox(
            outwardScene,
            "Floor",
            { -5.f, 1.8f },
            { 10.f, 1.f },
            false
        );
        outward.body->setVelocity({ 3.f, -4.f });
        fixedStep(outwardScene, outwardWorld, 0.01f);

        L2D_REQUIRE(approximatelyEqual(outward.body->velocity().x, 3.f));
        L2D_REQUIRE(approximatelyEqual(outward.body->velocity().y, -4.f));
        L2D_REQUIRE(outward.collider.isColliding());
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

    runTest("legacy defaults and input sanitization",
        testLegacyDefaultsAndInputSanitization, failures);
    runTest("world config and restitution threshold",
        testWorldConfigAndRestitutionThreshold, failures);
    runTest("body modes and free integration",
        testBodyModesAndFreeIntegration, failures);
    runTest("extreme integration cancellation stays finite",
        testExtremeIntegrationCancellationStaysFinite, failures);
    runTest("dynamic/static and dynamic box response",
        testDynamicStaticAndDynamicBoxResponse, failures);
    runTest("unequal mass restitution and kinematic push",
        testUnequalMassRestitutionAndKinematicPush, failures);
    runTest("static and dynamic friction",
        testStaticAndDynamicFriction, failures);
    runTest("circle-circle manifolds", testCircleCircleManifolds, failures);
    runTest("box-box manifolds", testBoxBoxManifolds, failures);
    runTest("circle-box manifolds", testCircleBoxManifolds, failures);
    runTest("collision filters", testCollisionFilters, failures);
    runTest("static pairs are not reported",
        testStaticPairsAreNotReported, failures);
    runTest("sensor event lifecycle has no response",
        testSensorEventLifecycleHasNoResponse, failures);
    runTest("deactivation and destruction emit End",
        testDeactivationAndDestructionEmitEnd, failures);
    runTest("invalid delta time is a no-op",
        testInvalidDeltaTimeIsANoOp, failures);
    runTest("grounded uses only solid support contacts",
        testGroundedUsesOnlySolidSupportContacts, failures);
    runTest("contacts have deterministic ordering",
        testContactsHaveDeterministicOrdering, failures);
    runTest("maximum-mass collision remains finite",
        testMaximumMassCollisionRemainsFinite, failures);
    runTest("extreme restitution swaps finite velocities",
        testExtremeRestitutionSwapsFiniteVelocities, failures);
    runTest("scene address reuse starts fresh contact history",
        testSceneAddressReuseStartsFreshContactHistory, failures);
    runTest("reset scene clears physics flags",
        testResetSceneClearsPhysicsFlags, failures);
    runTest("kinematic bodies never become grounded",
        testKinematicBodiesNeverBecomeGrounded, failures);
    runTest("only first base collider participates",
        testOnlyFirstBaseColliderParticipates, failures);
    runTest("short dynamic box stack remains finite",
        testShortDynamicBoxStackRemainsFinite, failures);
    runTest("legacy tangent and outward velocity remain",
        testLegacyTangentAndOutwardVelocityRemain, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics tests passed.\n";
    return 0;
}

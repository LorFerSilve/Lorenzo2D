#include "TestSupport.hpp"

#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <limits>

namespace
{
    l2d::GameObject& addBox(l2d::Scene& scene, const char* name, sf::Vector2f center,
                            sf::Vector2f size)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(center);
        object.addComponent<l2d::BoxCollider2D>(size);
        return object;
    }

    struct BoxCharacter
    {
        l2d::GameObject& object;
        l2d::BoxCollider2D& collider;
        l2d::CharacterMotor2D& motor;
    };

    BoxCharacter addBoxCharacter(l2d::Scene& scene, sf::Vector2f center = {0.f, 0.f})
    {
        l2d::GameObject& object = scene.createGameObject("Character");
        object.transform.setPosition(center);
        l2d::BoxCollider2D& collider =
            object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        l2d::CharacterMotor2D& motor = object.addComponent<l2d::CharacterMotor2D>();
        return {object, collider, motor};
    }

    void testConfigurationAndFailureModes()
    {
        l2d::CharacterMotorConfig2D config;
        L2D_REQUIRE(l2d::CharacterMotor2D::isValidConfig(config));

        config.maximumSlopeAngleDegrees = 90.f;
        L2D_REQUIRE(!l2d::CharacterMotor2D::isValidConfig(config));
        config = {};
        config.upDirection = {0.f, 0.f};
        L2D_REQUIRE(!l2d::CharacterMotor2D::isValidConfig(config));
        config = {};
        config.maximumMoveDistance = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!l2d::CharacterMotor2D::isValidConfig(config));
        config = {};
        config.queryFilter.ignoredObject = 42u;
        L2D_REQUIRE(!l2d::CharacterMotor2D::isValidConfig(config));

        l2d::Scene scene;
        addBox(scene, "Obstacle", {5.f, 0.f}, {2.f, 20.f});
        l2d::PhysicsQueryContext2D queries(scene);
        l2d::CharacterMotor2D detached;
        L2D_REQUIRE(!detached.move(queries, {1.f, 0.f}).succeeded);

        l2d::GameObject& noCollider = scene.createGameObject("No collider");
        l2d::CharacterMotor2D& noColliderMotor = noCollider.addComponent<l2d::CharacterMotor2D>();
        L2D_REQUIRE(!noColliderMotor.move(queries, {1.f, 0.f}).succeeded);

        BoxCharacter dynamic = addBoxCharacter(scene, {0.f, 30.f});
        l2d::RigidBody2D& body = dynamic.object.addComponent<l2d::RigidBody2D>();
        body.setBodyType(l2d::BodyType2D::Dynamic);
        l2d::PhysicsQueryContext2D dynamicQueries(scene);
        L2D_REQUIRE(!dynamic.motor.move(dynamicQueries, {1.f, 0.f}).succeeded);
        L2D_REQUIRE_APPROX(dynamic.object.transform.position(), sf::Vector2f(0.f, 30.f), 0.0001f);

        BoxCharacter immovable = addBoxCharacter(scene, {0.f, 60.f});
        l2d::RigidBody2D& staticBody = immovable.object.addComponent<l2d::RigidBody2D>();
        staticBody.setBodyType(l2d::BodyType2D::Static);
        l2d::PhysicsQueryContext2D staticQueries(scene);
        L2D_REQUIRE(!immovable.motor.move(staticQueries, {1.f, 0.f}).succeeded);
    }

    void testSweepStopsAndSlidesAlongWalls()
    {
        l2d::Scene scene;
        l2d::GameObject& wall = addBox(scene, "Wall", {5.f, 0.f}, {2.f, 30.f});
        BoxCharacter character = addBoxCharacter(scene);

        const l2d::CharacterMoveResult2D result =
            character.motor.move(l2d::PhysicsQueryContext2D(scene), {10.f, 4.f});
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE(result.state.touchingWall);
        L2D_REQUIRE(!result.state.grounded);
        L2D_REQUIRE_APPROX(character.object.transform.position().x, 2.99f, 0.02f);
        L2D_REQUIRE_APPROX(character.object.transform.position().y, 4.f, 0.02f);
        L2D_REQUIRE(result.movementDisplacement.x < result.requestedDisplacement.x);
        L2D_REQUIRE(!result.contacts.empty());
        L2D_REQUIRE_EQUAL(result.contacts.front().object.id(), wall.id());
        L2D_REQUIRE_EQUAL(result.contacts.front().kind, l2d::CharacterContactKind2D::Wall);
    }

    void testGroundCeilingAndGroundSnap()
    {
        l2d::Scene scene;
        l2d::GameObject& floor = addBox(scene, "Floor", {0.f, 5.f}, {40.f, 2.f});
        addBox(scene, "Ceiling", {0.f, -5.f}, {40.f, 2.f});
        BoxCharacter character = addBoxCharacter(scene);

        l2d::CharacterMoveResult2D down =
            character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(down.succeeded);
        L2D_REQUIRE(down.state.grounded);
        L2D_REQUIRE_EQUAL(down.state.support.id(), floor.id());
        L2D_REQUIRE(down.state.supportColliderId != l2d::InvalidColliderId);
        L2D_REQUIRE_APPROX(down.state.groundNormal, sf::Vector2f(0.f, -1.f), 0.001f);
        L2D_REQUIRE_APPROX(character.object.transform.position().y, 2.99f, 0.02f);

        character.object.transform.setPosition({0.f, 0.f});
        character.motor.clearState();
        l2d::CharacterMoveResult2D up =
            character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, -10.f});
        L2D_REQUIRE(up.succeeded);
        L2D_REQUIRE(up.state.touchingCeiling);
        L2D_REQUIRE(!up.state.grounded);
        L2D_REQUIRE_APPROX(character.object.transform.position().y, -2.99f, 0.02f);

        character.object.transform.setPosition({0.f, 2.9f});
        character.motor.clearState();
        l2d::CharacterMoveResult2D snap =
            character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 0.f});
        L2D_REQUIRE(snap.succeeded);
        L2D_REQUIRE(snap.state.grounded);
        L2D_REQUIRE(snap.snapDisplacement.y > 0.f);
        L2D_REQUIRE_APPROX(character.object.transform.position().y, 2.99f, 0.02f);
    }

    void testSlopeClassificationRespectsConfiguredLimit()
    {
        l2d::Scene scene;
        l2d::GameObject& slope = addBox(scene, "Slope", {0.f, 5.f}, {40.f, 2.f});
        slope.transform.setRotation(30.f);

        BoxCharacter permissive = addBoxCharacter(scene, {-5.f, -2.f});
        l2d::CharacterMotorConfig2D config = permissive.motor.config();
        config.maximumSlopeAngleDegrees = 35.f;
        L2D_REQUIRE(permissive.motor.setConfig(config));
        const auto walkable = permissive.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(walkable.succeeded);
        L2D_REQUIRE(walkable.state.grounded);

        BoxCharacter strict = addBoxCharacter(scene, {5.f, 3.f});
        config = strict.motor.config();
        config.maximumSlopeAngleDegrees = 20.f;
        config.groundProbeDistance = 0.f;
        L2D_REQUIRE(strict.motor.setConfig(config));
        const auto tooSteep = strict.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 4.f});
        L2D_REQUIRE(tooSteep.succeeded);
        L2D_REQUIRE(!tooSteep.state.grounded);
        L2D_REQUIRE(tooSteep.state.touchingWall);
    }

    void testOverlapRecoveryAndColliderSelection()
    {
        l2d::Scene scene;
        addBox(scene, "Embedded wall", {1.5f, 0.f}, {2.f, 8.f});
        BoxCharacter character = addBoxCharacter(scene);

        const auto recovered = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 0.f});
        L2D_REQUIRE(recovered.succeeded);
        L2D_REQUIRE(recovered.recoveryDisplacement.x < -0.49f);
        L2D_REQUIRE(!recovered.contacts.empty());
        L2D_REQUIRE(recovered.contacts.front().recoveredOverlap);

        character.motor.setColliderId(character.collider.id() + 1000000u);
        const sf::Vector2f before = character.object.transform.position();
        const auto missing = character.motor.move(l2d::PhysicsQueryContext2D(scene), {1.f, 0.f});
        L2D_REQUIRE(!missing.succeeded);
        L2D_REQUIRE_APPROX(character.object.transform.position(), before, 0.0001f);
    }

    void testCapsuleMotorAndMovingPlatformTranslation()
    {
        l2d::Scene scene;
        l2d::GameObject& platform = addBox(scene, "Platform", {0.f, 5.f}, {40.f, 2.f});
        l2d::GameObject& character = scene.createGameObject("Capsule character");
        character.addComponent<l2d::CapsuleCollider2D>(1.f, 4.f);
        l2d::CharacterMotor2D& motor = character.addComponent<l2d::CharacterMotor2D>();

        const auto landing = motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(landing.succeeded);
        L2D_REQUIRE(landing.state.grounded);
        L2D_REQUIRE_EQUAL(landing.state.support.id(), platform.id());

        platform.transform.move({2.f, 0.f});
        const sf::Vector2f before = character.transform.position();
        const auto carried = motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 0.f});
        L2D_REQUIRE(carried.succeeded);
        L2D_REQUIRE_APPROX(carried.inheritedDisplacement, sf::Vector2f(2.f, 0.f), 0.001f);
        L2D_REQUIRE_APPROX(character.transform.position(), before + sf::Vector2f(2.f, 0.f), 0.02f);
        L2D_REQUIRE(carried.state.grounded);
    }

    void testMovingPlatformCannotCarryCharacterThroughWall()
    {
        l2d::Scene scene;
        l2d::GameObject& platform = addBox(scene, "Platform", {0.f, 5.f}, {40.f, 2.f});
        addBox(scene, "Wall", {5.f, 0.f}, {2.f, 20.f});
        BoxCharacter character = addBoxCharacter(scene);

        const auto landing = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(landing.state.grounded);
        L2D_REQUIRE_EQUAL(landing.state.support.id(), platform.id());

        platform.transform.move({10.f, 0.f});
        const auto carried = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 0.f});
        L2D_REQUIRE(carried.succeeded);
        L2D_REQUIRE(carried.state.touchingWall);
        L2D_REQUIRE(carried.inheritedDisplacement.x < 3.01f);
        L2D_REQUIRE_APPROX(character.object.transform.position().x, 2.99f, 0.02f);
    }

    void testMoveProbeRestoresTransformAndState()
    {
        l2d::Scene scene;
        l2d::GameObject& platform = addBox(scene, "Platform", {0.f, 5.f}, {40.f, 2.f});
        BoxCharacter character = addBoxCharacter(scene);
        const auto landing = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(landing.state.grounded);
        const sf::Vector2f position = character.object.transform.position();
        const l2d::CharacterMotorState2D state = character.motor.state();

        const auto probe =
            character.motor.testMove(l2d::PhysicsQueryContext2D(scene), {100.f, -25.f});
        L2D_REQUIRE(probe.succeeded);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), position, 0.0001f);
        L2D_REQUIRE_EQUAL(character.motor.state().grounded, state.grounded);
        L2D_REQUIRE_EQUAL(character.motor.state().support.id(), platform.id());
        L2D_REQUIRE_EQUAL(character.motor.state().supportColliderId, state.supportColliderId);

        platform.transform.move({2.f, 0.f});
        const auto carried = character.motor.move(l2d::PhysicsQueryContext2D(scene), {});
        L2D_REQUIRE_APPROX_2D(carried.inheritedDisplacement, sf::Vector2f(2.f, 0.f), 0.001f);
    }

    sf::Vector2f replayMovement()
    {
        l2d::Scene scene;
        addBox(scene, "Wall", {5.f, 0.f}, {2.f, 30.f});
        addBox(scene, "Floor", {0.f, 8.f}, {40.f, 2.f});
        BoxCharacter character = addBoxCharacter(scene);
        const sf::Vector2f commands[] = {
            {2.f, 1.f}, {2.f, 1.f}, {2.f, 1.f}, {-1.f, 2.f}, {3.f, 2.f}};
        for (const sf::Vector2f command : commands)
        {
            const auto result = character.motor.move(l2d::PhysicsQueryContext2D(scene), command);
            L2D_REQUIRE(result.succeeded);
        }
        return character.object.transform.position();
    }

    void testMotorReplayIsDeterministic()
    {
        L2D_REQUIRE_APPROX(replayMovement(), replayMovement(), 0.000001f);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("character motor configuration and failure modes",
                       testConfigurationAndFailureModes, failures);
    l2d::test::runTest("character sweep stops and slides along walls",
                       testSweepStopsAndSlidesAlongWalls, failures);
    l2d::test::runTest("character ground, ceiling, and ground snap", testGroundCeilingAndGroundSnap,
                       failures);
    l2d::test::runTest("character slope classification",
                       testSlopeClassificationRespectsConfiguredLimit, failures);
    l2d::test::runTest("character overlap recovery and collider selection",
                       testOverlapRecoveryAndColliderSelection, failures);
    l2d::test::runTest("capsule character and moving platform translation",
                       testCapsuleMotorAndMovingPlatformTranslation, failures);
    l2d::test::runTest("moving platform cannot carry a character through a wall",
                       testMovingPlatformCannotCarryCharacterThroughWall, failures);
    l2d::test::runTest("character move probe restores transform and state",
                       testMoveProbeRestoresTransformAndState, failures);
    l2d::test::runTest("character motor deterministic replay", testMotorReplayIsDeterministic,
                       failures);

    if (failures != 0)
    {
        std::cerr << failures << " character motor test(s) failed.\n";
        return 1;
    }
    std::cout << "All Lorenzo2D character motor tests passed.\n";
    return 0;
}

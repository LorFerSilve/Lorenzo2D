#include "TestSupport.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <cmath>
#include <limits>
#include <tuple>

namespace
{
    struct Character
    {
        l2d::GameObject& object;
        l2d::CharacterMotor2D& motor;
        l2d::TopDownController2D& controller;
    };

    Character addCharacter(l2d::Scene& scene, sf::Vector2f position = {})
    {
        l2d::GameObject& object = scene.createGameObject("Top-down character");
        object.transform.setPosition(position);
        object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        l2d::CharacterMotor2D& motor =
            object.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        l2d::TopDownController2D& controller = object.addComponent<l2d::TopDownController2D>();
        return {object, motor, controller};
    }

    void testConfigurationAndFailureModes()
    {
        l2d::TopDownControllerConfig2D config;
        L2D_REQUIRE(l2d::TopDownController2D::isValidConfig(config));
        config.maximumSpeed = 0.f;
        L2D_REQUIRE(!l2d::TopDownController2D::isValidConfig(config));
        config = {};
        config.acceleration = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!l2d::TopDownController2D::isValidConfig(config));
        config = {};
        config.inputDeadzone = 1.f;
        L2D_REQUIRE(!l2d::TopDownController2D::isValidConfig(config));

        const l2d::CharacterMotorConfig2D motorConfig = l2d::topDownCharacterMotorConfig2D();
        L2D_REQUIRE(motorConfig.groundProbeDistance == 0.f);
        L2D_REQUIRE(!motorConfig.snapToGround);
        L2D_REQUIRE(!motorConfig.inheritPlatformTranslation);

        l2d::Scene scene;
        l2d::GameObject& missingMotor = scene.createGameObject("Missing motor");
        l2d::TopDownController2D& controller =
            missingMotor.addComponent<l2d::TopDownController2D>();
        const l2d::PhysicsQueryContext2D queries(scene);
        L2D_REQUIRE(!controller.move(queries, {1.f, 0.f}, 1.f / 60.f).succeeded);

        Character character = addCharacter(scene, {10.f, 0.f});
        L2D_REQUIRE(!character.controller.move(queries, {1.f, 0.f}, 0.f).succeeded);
        L2D_REQUIRE(!character.controller
                         .move(queries, {std::numeric_limits<float>::quiet_NaN(), 0.f}, 1.f / 60.f)
                         .succeeded);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(10.f, 0.f),
                              0.0001f);
    }

    void testAccelerationAnalogInputAndStopping()
    {
        l2d::Scene scene;
        Character character = addCharacter(scene);
        l2d::TopDownControllerConfig2D config;
        config.maximumSpeed = 10.f;
        config.acceleration = 20.f;
        config.deceleration = 10.f;
        config.inputDeadzone = 0.f;
        config.maximumDeltaTime = 0.5f;
        L2D_REQUIRE(character.controller.setConfig(config));

        l2d::TopDownMoveResult2D first =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {0.5f, 0.f}, 0.25f);
        L2D_REQUIRE(first.succeeded);
        L2D_REQUIRE_APPROX_2D(first.state.velocity, sf::Vector2f(5.f, 0.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(1.25f, 0.f),
                              0.0001f);
        L2D_REQUIRE_APPROX_2D(first.state.facingDirection, sf::Vector2f(1.f, 0.f), 0.0001f);

        l2d::TopDownMoveResult2D second =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {0.f, 0.f}, 0.25f);
        L2D_REQUIRE_APPROX_2D(second.state.velocity, sf::Vector2f(2.5f, 0.f), 0.0001f);
        character.controller.stop();
        L2D_REQUIRE(!character.controller.state().moving);
        L2D_REQUIRE_APPROX_2D(character.controller.state().velocity, sf::Vector2f{}, 0.0001f);
    }

    void testDiagonalNormalizationAndWallSlide()
    {
        l2d::Scene scene;
        l2d::GameObject& wall = scene.createGameObject("Wall");
        wall.transform.setPosition({5.f, 0.f});
        wall.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 30.f});
        Character character = addCharacter(scene);

        l2d::TopDownControllerConfig2D config;
        config.maximumSpeed = 10.f;
        config.acceleration = 100.f;
        config.deceleration = 100.f;
        config.inputDeadzone = 0.f;
        config.maximumDeltaTime = 1.f;
        L2D_REQUIRE(character.controller.setConfig(config));

        const l2d::TopDownMoveResult2D result =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {1.f, 1.f}, 1.f);
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE_APPROX(std::hypot(result.desiredVelocity.x, result.desiredVelocity.y), 10.f,
                           0.0001f);
        L2D_REQUIRE(character.object.transform.position().x < 3.01f);
        L2D_REQUIRE(character.object.transform.position().y > 7.f);
        L2D_REQUIRE_APPROX(result.state.velocity.x, 0.f, 0.0001f);
        L2D_REQUIRE(result.state.velocity.y > 7.f);
        L2D_REQUIRE(result.motorResult.state.touchingWall);
    }

    auto replayTopDown()
    {
        l2d::Scene scene;
        Character character = addCharacter(scene);
        l2d::TopDownControllerConfig2D config;
        config.inputDeadzone = 0.f;
        character.controller.setConfig(config);
        for (int tick = 0; tick < 180; ++tick)
        {
            sf::Vector2f input;
            if (tick < 60)
                input = {1.f, 0.f};
            else if (tick < 120)
                input = {0.f, -1.f};
            character.controller.move(l2d::PhysicsQueryContext2D(scene), input, 1.f / 60.f);
        }
        return std::make_tuple(character.object.transform.position(),
                               character.controller.state().velocity,
                               character.controller.state().facingDirection);
    }

    void testDeterministicReplay()
    {
        L2D_REQUIRE_DETERMINISTIC_REPLAY(replayTopDown);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("top-down configuration and failures", testConfigurationAndFailureModes,
                       failures);
    l2d::test::runTest("top-down acceleration and stopping", testAccelerationAnalogInputAndStopping,
                       failures);
    l2d::test::runTest("top-down diagonal wall slide", testDiagonalNormalizationAndWallSlide,
                       failures);
    l2d::test::runTest("top-down deterministic replay", testDeterministicReplay, failures);
    return failures == 0 ? 0 : 1;
}

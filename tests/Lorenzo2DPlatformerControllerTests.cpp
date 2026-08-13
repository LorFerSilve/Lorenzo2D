#include "TestSupport.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <limits>
#include <tuple>

namespace
{
    constexpr std::uint32_t OneWayCategory = 1u << 1u;

    l2d::GameObject& addBox(l2d::Scene& scene, const char* name, sf::Vector2f center,
                            sf::Vector2f size, std::uint32_t category = 1u)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(center);
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        l2d::CollisionFilter2D filter = collider.filter();
        filter.categoryBits = category;
        collider.setFilter(filter);
        return object;
    }

    struct Character
    {
        l2d::GameObject& object;
        l2d::CharacterMotor2D& motor;
        l2d::PlatformerController2D& controller;
    };

    Character addCharacter(l2d::Scene& scene, sf::Vector2f position = {})
    {
        l2d::GameObject& object = scene.createGameObject("Platform character");
        object.transform.setPosition(position);
        object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        l2d::CharacterMotor2D& motor = object.addComponent<l2d::CharacterMotor2D>(
            l2d::platformerCharacterMotorConfig2D(OneWayCategory));
        l2d::PlatformerController2D& controller =
            object.addComponent<l2d::PlatformerController2D>();
        return {object, motor, controller};
    }

    void settle(Character& character, l2d::Scene& scene)
    {
        const auto landing = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 100.f});
        L2D_REQUIRE(landing.succeeded);
        L2D_REQUIRE(landing.state.grounded);
        const auto synchronized =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 1.f / 60.f);
        L2D_REQUIRE(synchronized.succeeded);
        L2D_REQUIRE(synchronized.state.grounded);
    }

    void testConfigurationAndFailureModes()
    {
        l2d::PlatformerControllerConfig2D config;
        L2D_REQUIRE(l2d::PlatformerController2D::isValidConfig(config));
        config.jumpSpeed = 0.f;
        L2D_REQUIRE(!l2d::PlatformerController2D::isValidConfig(config));
        config = {};
        config.jumpCutMultiplier = 1.1f;
        L2D_REQUIRE(!l2d::PlatformerController2D::isValidConfig(config));
        config = {};
        config.gravity = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!l2d::PlatformerController2D::isValidConfig(config));

        const l2d::CharacterMotorConfig2D motor =
            l2d::platformerCharacterMotorConfig2D(OneWayCategory);
        L2D_REQUIRE(motor.snapToGround);
        L2D_REQUIRE(motor.inheritPlatformTranslation);
        L2D_REQUIRE_EQUAL(motor.oneWayPlatformCategoryMask, OneWayCategory);

        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Missing motor");
        auto& controller = object.addComponent<l2d::PlatformerController2D>();
        const l2d::PhysicsQueryContext2D queries(scene);
        L2D_REQUIRE(!controller.move(queries, {}, 1.f / 60.f).succeeded);
        L2D_REQUIRE(!controller.move(queries, {}, 0.f).succeeded);
    }

    void testRunJumpAndVariableHeight()
    {
        l2d::Scene scene;
        addBox(scene, "Floor", {0.f, 5.f}, {80.f, 2.f});
        Character character = addCharacter(scene);
        settle(character, scene);

        l2d::PlatformerControllerConfig2D config;
        config.maximumRunSpeed = 10.f;
        config.groundAcceleration = 100.f;
        config.groundDeceleration = 100.f;
        config.airAcceleration = 50.f;
        config.airDeceleration = 20.f;
        config.gravity = 100.f;
        config.maximumFallSpeed = 50.f;
        config.jumpSpeed = 20.f;
        config.jumpCutMultiplier = 0.5f;
        config.inputDeadzone = 0.f;
        config.stepHeight = 0.f;
        config.maximumDeltaTime = 0.5f;
        L2D_REQUIRE(character.controller.setConfig(config));

        const auto grounded =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 0.1f);
        L2D_REQUIRE(grounded.state.grounded);
        const auto jump = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                    {1.f, true, false, false}, 0.1f);
        L2D_REQUIRE(jump.succeeded);
        L2D_REQUIRE(jump.events.jumped);
        L2D_REQUIRE(jump.state.rising);
        L2D_REQUIRE_APPROX(jump.state.velocity.x, 10.f, 0.001f);
        L2D_REQUIRE_APPROX(jump.state.velocity.y, -20.f, 0.001f);

        const auto cut = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                   {1.f, false, true, false}, 0.1f);
        L2D_REQUIRE(cut.state.rising);
        L2D_REQUIRE_APPROX(cut.state.velocity.y, -5.f, 0.001f);
        L2D_REQUIRE(character.controller.state().facingRight);
    }

    void testZeroLengthAssistWindowsStillAllowGroundJump()
    {
        l2d::Scene scene;
        addBox(scene, "Floor", {0.f, 5.f}, {40.f, 2.f});
        Character character = addCharacter(scene);
        settle(character, scene);

        l2d::PlatformerControllerConfig2D config;
        config.coyoteTime = 0.f;
        config.jumpBufferTime = 0.f;
        config.stepHeight = 0.f;
        L2D_REQUIRE(character.controller.setConfig(config));
        const auto result = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                      {0.f, true, false, false}, 1.f / 60.f);
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE(result.events.jumped);
        L2D_REQUIRE(result.state.rising);
    }

    void testCoyoteTimeAndJumpBuffer()
    {
        {
            l2d::Scene scene;
            addBox(scene, "Short floor", {0.f, 5.f}, {4.f, 2.f});
            Character character = addCharacter(scene);
            settle(character, scene);
            l2d::PlatformerControllerConfig2D config;
            config.maximumRunSpeed = 50.f;
            config.groundAcceleration = 1000.f;
            config.inputDeadzone = 0.f;
            config.stepHeight = 0.f;
            L2D_REQUIRE(character.controller.setConfig(config));
            L2D_REQUIRE(
                character.controller
                    .move(l2d::PhysicsQueryContext2D(scene), {1.f, false, false, false}, 0.1f)
                    .succeeded);
            const auto airborne =
                character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 0.01f);
            L2D_REQUIRE(!airborne.state.grounded);
            const auto coyote = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                          {0.f, true, false, false}, 0.05f);
            L2D_REQUIRE(coyote.events.jumped);
            L2D_REQUIRE(coyote.state.rising);
        }

        {
            l2d::Scene scene;
            addBox(scene, "Floor", {0.f, 5.f}, {40.f, 2.f});
            Character character = addCharacter(scene, {0.f, 0.f});
            l2d::PlatformerControllerConfig2D config;
            config.gravity = 100.f;
            config.maximumFallSpeed = 100.f;
            config.jumpSpeed = 20.f;
            config.jumpBufferTime = 0.3f;
            config.stepHeight = 0.f;
            config.maximumDeltaTime = 0.1f;
            L2D_REQUIRE(character.controller.setConfig(config));
            L2D_REQUIRE(character.controller.setVelocity({0.f, 20.f}));
            const auto buffered = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                            {0.f, true, false, false}, 0.05f);
            L2D_REQUIRE(!buffered.events.jumped);
            for (int tick = 0; tick < 4 && !character.controller.state().grounded; ++tick)
                L2D_REQUIRE(character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 0.05f)
                                .succeeded);
            L2D_REQUIRE(character.controller.state().grounded);
            const auto jump =
                character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 0.05f);
            L2D_REQUIRE(jump.events.jumped);
            L2D_REQUIRE(jump.state.rising);
        }
    }

    void testOneWayLandingAndDropThrough()
    {
        l2d::Scene scene;
        addBox(scene, "One-way", {0.f, 5.f}, {20.f, 1.f}, OneWayCategory);
        Character character = addCharacter(scene, {0.f, 8.f});

        const auto upward = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, -10.f});
        L2D_REQUIRE(upward.succeeded);
        L2D_REQUIRE(!upward.state.touchingCeiling);
        L2D_REQUIRE(character.object.transform.position().y < 0.f);

        const auto landing = character.motor.move(l2d::PhysicsQueryContext2D(scene), {0.f, 10.f});
        L2D_REQUIRE(landing.state.grounded);
        L2D_REQUIRE(landing.state.onOneWayPlatform);
        L2D_REQUIRE(landing.contacts.back().oneWayPlatform);

        const auto synchronized =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 1.f / 60.f);
        L2D_REQUIRE(synchronized.state.onOneWayPlatform);
        const float beforeDrop = character.object.transform.position().y;
        const auto dropping = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                        {0.f, false, false, true}, 0.1f);
        L2D_REQUIRE(dropping.events.droppedThrough);
        L2D_REQUIRE(!dropping.state.grounded);
        L2D_REQUIRE(character.object.transform.position().y > beforeDrop);
    }

    void testStepUpAndTransactionalFailure()
    {
        l2d::Scene scene;
        addBox(scene, "Floor", {0.f, 5.f}, {40.f, 2.f});
        addBox(scene, "Step", {4.f, 3.5f}, {2.f, 1.f});
        Character character = addCharacter(scene);
        settle(character, scene);

        const auto blocked = character.motor.move(l2d::PhysicsQueryContext2D(scene), {5.f, 0.f});
        L2D_REQUIRE(blocked.state.touchingWall);
        const auto step =
            character.motor.tryStep(l2d::PhysicsQueryContext2D(scene), {3.f, 0.f}, 1.5f, 0.25f);
        L2D_REQUIRE(step.succeeded);
        L2D_REQUIRE(step.state.grounded);
        L2D_REQUIRE(character.object.transform.position().x > 4.9f);
        L2D_REQUIRE(character.object.transform.position().y < 2.1f);

        const sf::Vector2f beforeFailure = character.object.transform.position();
        addBox(scene, "High wall", {8.f, 0.f}, {2.f, 10.f});
        const auto failure =
            character.motor.tryStep(l2d::PhysicsQueryContext2D(scene), {5.f, 0.f}, 1.f, 0.f);
        L2D_REQUIRE(!failure.succeeded);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), beforeFailure, 0.0001f);
    }

    void testControllerAutomaticallyStepsOverLowObstacle()
    {
        l2d::Scene scene;
        addBox(scene, "Floor", {0.f, 5.f}, {40.f, 2.f});
        addBox(scene, "Low step", {4.f, 3.5f}, {2.f, 1.f});
        Character character = addCharacter(scene);
        settle(character, scene);

        l2d::PlatformerControllerConfig2D config;
        config.maximumRunSpeed = 50.f;
        config.groundAcceleration = 1000.f;
        config.inputDeadzone = 0.f;
        config.stepHeight = 1.5f;
        config.stepDownDistance = 0.25f;
        config.maximumDeltaTime = 0.1f;
        L2D_REQUIRE(character.controller.setConfig(config));
        const auto result = character.controller.move(l2d::PhysicsQueryContext2D(scene),
                                                      {1.f, false, false, false}, 0.1f);
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE(result.events.steppedUp);
        L2D_REQUIRE(result.stepResult.succeeded);
        L2D_REQUIRE(character.object.transform.position().x > 4.9f);
        L2D_REQUIRE(character.object.transform.position().y < 2.1f);
    }

    void testMovingPlatformTranslation()
    {
        l2d::Scene scene;
        l2d::GameObject& platform = addBox(scene, "Moving platform", {0.f, 5.f}, {20.f, 2.f});
        Character character = addCharacter(scene);
        settle(character, scene);
        const sf::Vector2f before = character.object.transform.position();
        platform.transform.move({3.f, -1.f});
        const auto result =
            character.controller.move(l2d::PhysicsQueryContext2D(scene), {}, 1.f / 60.f);
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE_APPROX_2D(result.motorResult.inheritedDisplacement, sf::Vector2f(3.f, -1.f),
                              0.001f);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(),
                              before + sf::Vector2f(3.f, -1.f), 0.03f);
    }

    auto replayPlatformer()
    {
        l2d::Scene scene;
        addBox(scene, "Floor", {0.f, 20.f}, {200.f, 2.f});
        Character character = addCharacter(scene);
        settle(character, scene);
        for (int tick = 0; tick < 180; ++tick)
        {
            l2d::PlatformerInput2D input;
            input.horizontal = tick < 120 ? 1.f : -1.f;
            input.jumpPressed = tick == 15 || tick == 100;
            input.jumpReleased = tick == 25 || tick == 110;
            const auto result =
                character.controller.move(l2d::PhysicsQueryContext2D(scene), input, 1.f / 60.f);
            L2D_REQUIRE(result.succeeded);
        }
        return std::make_tuple(
            character.object.transform.position(), character.controller.state().velocity,
            character.controller.state().grounded, character.controller.state().facingRight);
    }

    void testDeterministicReplay()
    {
        L2D_REQUIRE_DETERMINISTIC_REPLAY(replayPlatformer);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("platformer configuration and failures", testConfigurationAndFailureModes,
                       failures);
    l2d::test::runTest("platformer run, jump, and variable height", testRunJumpAndVariableHeight,
                       failures);
    l2d::test::runTest("platformer zero-length assist windows",
                       testZeroLengthAssistWindowsStillAllowGroundJump, failures);
    l2d::test::runTest("platformer coyote time and jump buffer", testCoyoteTimeAndJumpBuffer,
                       failures);
    l2d::test::runTest("platformer one-way landing and drop-through",
                       testOneWayLandingAndDropThrough, failures);
    l2d::test::runTest("platformer step-up and rollback", testStepUpAndTransactionalFailure,
                       failures);
    l2d::test::runTest("platformer controller automatic step-up",
                       testControllerAutomaticallyStepsOverLowObstacle, failures);
    l2d::test::runTest("platformer moving-platform translation", testMovingPlatformTranslation,
                       failures);
    l2d::test::runTest("platformer deterministic replay", testDeterministicReplay, failures);
    return failures == 0 ? 0 : 1;
}

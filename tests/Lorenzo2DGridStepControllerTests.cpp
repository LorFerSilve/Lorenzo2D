#include "TestSupport.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <limits>
#include <tuple>
#include <vector>

namespace
{
    struct Character
    {
        l2d::GameObject& object;
        l2d::CharacterMotor2D& motor;
        l2d::GridStepController2D& controller;
    };

    Character addCharacter(l2d::Scene& scene, sf::Vector2f position = {})
    {
        l2d::GameObject& object = scene.createGameObject("Grid character");
        object.transform.setPosition(position);
        object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        l2d::CharacterMotor2D& motor =
            object.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        l2d::GridStepControllerConfig2D config;
        config.cellSize = {10.f, 10.f};
        config.stepDuration = 0.2f;
        config.maximumDeltaTime = 0.1f;
        l2d::GridStepController2D& controller =
            object.addComponent<l2d::GridStepController2D>(config);
        return {object, motor, controller};
    }

    void testDirectionResolutionAndConfiguration()
    {
        L2D_REQUIRE(l2d::gridDirectionFromInput({0.2f, 0.2f}) == l2d::GridDirection2D::None);
        L2D_REQUIRE(l2d::gridDirectionFromInput({-1.f, 0.5f}) == l2d::GridDirection2D::Left);
        L2D_REQUIRE(l2d::gridDirectionFromInput({1.f, -1.f}) == l2d::GridDirection2D::Up);
        L2D_REQUIRE(
            l2d::gridDirectionFromInput({1.f, -1.f}, 0.5f, l2d::GridAxisPriority2D::Horizontal) ==
            l2d::GridDirection2D::Right);
        L2D_REQUIRE(l2d::gridDirectionFromInput({std::numeric_limits<float>::quiet_NaN(), 0.f}) ==
                    l2d::GridDirection2D::None);

        l2d::GridStepControllerConfig2D config;
        L2D_REQUIRE(l2d::GridStepController2D::isValidConfig(config));
        config.cellSize.x = 0.f;
        L2D_REQUIRE(!l2d::GridStepController2D::isValidConfig(config));
        config = {};
        config.stepDuration = -1.f;
        L2D_REQUIRE(!l2d::GridStepController2D::isValidConfig(config));
        config = {};
        config.axisPriority = static_cast<l2d::GridAxisPriority2D>(99);
        L2D_REQUIRE(!l2d::GridStepController2D::isValidConfig(config));
    }

    void testSynchronizationAndSmoothStep()
    {
        l2d::Scene scene;
        Character character = addCharacter(scene, {20.f, 10.f});
        L2D_REQUIRE(character.controller.synchronizeToGrid());
        L2D_REQUIRE_EQUAL(character.controller.state().cell, sf::Vector2i(2, 1));

        const l2d::GridStepResult2D first = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE(first.succeeded);
        L2D_REQUIRE(first.stepStarted);
        L2D_REQUIRE(!first.stepCompleted);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(25.f, 10.f),
                              0.0001f);

        const l2d::GridStepResult2D second = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE(second.stepCompleted);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(30.f, 10.f),
                              0.0001f);
        L2D_REQUIRE_EQUAL(second.state.cell, sf::Vector2i(3, 1));

        character.object.transform.setPosition({31.f, 10.f});
        const l2d::GridStepResult2D externallyMoved = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE(!externallyMoved.succeeded);
        L2D_REQUIRE(!externallyMoved.state.synchronized);
        L2D_REQUIRE(!character.controller.synchronizeToGrid());
        L2D_REQUIRE(character.controller.synchronizeToGrid(true));
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(30.f, 10.f),
                              0.0001f);
    }

    void testBlockedStepIsTransactional()
    {
        l2d::Scene scene;
        l2d::GameObject& obstacle = scene.createGameObject("Blocked cell");
        obstacle.transform.setPosition({10.f, 0.f});
        obstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        Character character = addCharacter(scene);

        const l2d::GridStepResult2D result = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE(result.succeeded);
        L2D_REQUIRE(result.blocked);
        L2D_REQUIRE(!result.stepStarted);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f{}, 0.0001f);
        L2D_REQUIRE_EQUAL(character.controller.state().cell, sf::Vector2i(0, 0));
    }

    void testTurnBufferingAndDynamicRollback()
    {
        l2d::Scene scene;
        Character character = addCharacter(scene);
        character.controller.move(l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right,
                                  0.1f);
        const l2d::GridStepResult2D completed = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Down, 0.1f);
        L2D_REQUIRE(completed.stepCompleted);
        L2D_REQUIRE(completed.state.bufferedDirection == l2d::GridDirection2D::Down);

        const l2d::GridStepResult2D turn = character.controller.move(
            l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::None, 0.1f);
        L2D_REQUIRE(turn.stepStarted);
        L2D_REQUIRE(turn.state.direction == l2d::GridDirection2D::Down);
        L2D_REQUIRE_APPROX_2D(character.object.transform.position(), sf::Vector2f(10.f, 5.f),
                              0.0001f);

        l2d::Scene rollbackScene;
        Character rollback = addCharacter(rollbackScene);
        l2d::GridStepControllerConfig2D slow = rollback.controller.config();
        slow.stepDuration = 0.4f;
        L2D_REQUIRE(rollback.controller.setConfig(slow));
        rollback.controller.move(l2d::PhysicsQueryContext2D(rollbackScene),
                                 l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE_APPROX_2D(rollback.object.transform.position(), sf::Vector2f(2.5f, 0.f),
                              0.0001f);

        l2d::GameObject& movingObstacle = rollbackScene.createGameObject("Moving obstacle");
        movingObstacle.transform.setPosition({5.f, 0.f});
        movingObstacle.addComponent<l2d::BoxCollider2D>(sf::Vector2f{2.f, 2.f});
        const l2d::GridStepResult2D blocked = rollback.controller.move(
            l2d::PhysicsQueryContext2D(rollbackScene), l2d::GridDirection2D::Right, 0.1f);
        L2D_REQUIRE(blocked.rolledBack);
        L2D_REQUIRE(blocked.blocked);
        L2D_REQUIRE_APPROX_2D(rollback.object.transform.position(), sf::Vector2f{}, 0.0001f);
        L2D_REQUIRE_EQUAL(blocked.state.cell, sf::Vector2i(0, 0));
    }

    auto replayGridSteps()
    {
        l2d::Scene scene;
        Character character = addCharacter(scene);
        const std::vector<l2d::GridDirection2D> commands = {
            l2d::GridDirection2D::Right, l2d::GridDirection2D::Down, l2d::GridDirection2D::Left,
            l2d::GridDirection2D::Up};
        for (l2d::GridDirection2D direction : commands)
        {
            character.controller.move(l2d::PhysicsQueryContext2D(scene), direction, 0.1f);
            character.controller.move(l2d::PhysicsQueryContext2D(scene), direction, 0.1f);
        }
        return std::make_tuple(character.object.transform.position(),
                               character.controller.state().cell,
                               character.controller.state().facingDirection);
    }

    void testFailureModesAndDeterminism()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("No motor");
        l2d::GridStepController2D& controller = object.addComponent<l2d::GridStepController2D>();
        L2D_REQUIRE(
            !controller
                 .move(l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Right, 1.f / 60.f)
                 .succeeded);

        Character character = addCharacter(scene, {20.f, 0.f});
        L2D_REQUIRE(!character.controller
                         .move(l2d::PhysicsQueryContext2D(scene),
                               static_cast<l2d::GridDirection2D>(99), 0.1f)
                         .succeeded);
        L2D_REQUIRE(!character.controller
                         .move(l2d::PhysicsQueryContext2D(scene), l2d::GridDirection2D::Left, 0.f)
                         .succeeded);
        L2D_REQUIRE_DETERMINISTIC_REPLAY(replayGridSteps);
    }
}

int main()
{
    int failures = 0;
    l2d::test::runTest("grid direction and configuration", testDirectionResolutionAndConfiguration,
                       failures);
    l2d::test::runTest("grid synchronization and smooth step", testSynchronizationAndSmoothStep,
                       failures);
    l2d::test::runTest("grid blocked step is transactional", testBlockedStepIsTransactional,
                       failures);
    l2d::test::runTest("grid turn buffering and rollback", testTurnBufferingAndDynamicRollback,
                       failures);
    l2d::test::runTest("grid failures and deterministic replay", testFailureModesAndDeterminism,
                       failures);
    return failures == 0 ? 0 : 1;
}

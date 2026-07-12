#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <cmath>
#include <cstddef>

namespace l2d
{
    namespace
    {
        constexpr std::size_t SOLVER_ITERATIONS = 4;
        constexpr float GROUND_NORMAL_Y_THRESHOLD = -0.7f;

        bool isPhysicsParticipant(const GameObject* gameObject)
        {
            return gameObject != nullptr &&
                gameObject->isActive() &&
                !gameObject->isDestroyQueued();
        }
    }

    void PhysicsWorld2D::step(Scene& scene, float deltaTime)
    {
        (void)deltaTime;

        resetPhysicsStates(scene);
        resolveCircleBoxCollisions(scene);
    }

    void PhysicsWorld2D::resetPhysicsStates(Scene& scene)
    {
        for (const auto& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();

            if (gameObject == nullptr)
                continue;

            if (auto* circleCollider = gameObject->getComponent<CircleCollider2D>())
                circleCollider->setColliding(false);

            if (auto* boxCollider = gameObject->getComponent<BoxCollider2D>())
                boxCollider->setColliding(false);

            if (auto* rigidBody = gameObject->getComponent<RigidBody2D>())
                rigidBody->setGrounded(false);
        }
    }

    void PhysicsWorld2D::resolveCircleBoxCollisions(Scene& scene)
    {
        for (std::size_t iteration = 0; iteration < SOLVER_ITERATIONS; ++iteration)
        {
            bool resolvedAnyCollision = false;

            for (const auto& circleObjectPtr : scene.gameObjects())
            {
                GameObject* circleObject = circleObjectPtr.get();

                if (!isPhysicsParticipant(circleObject))
                    continue;

                auto* circleCollider = circleObject->getComponent<CircleCollider2D>();
                auto* rigidBody = circleObject->getComponent<RigidBody2D>();

                if (
                    circleCollider == nullptr ||
                    rigidBody == nullptr ||
                    !circleCollider->isActive() ||
                    !rigidBody->isActive()
                )
                {
                    continue;
                }

                for (const auto& boxObjectPtr : scene.gameObjects())
                {
                    GameObject* boxObject = boxObjectPtr.get();

                    if (!isPhysicsParticipant(boxObject))
                        continue;

                    if (boxObject == circleObject)
                        continue;

                    auto* boxCollider = boxObject->getComponent<BoxCollider2D>();

                    if (boxCollider == nullptr || !boxCollider->isActive())
                        continue;

                    if (
                        resolveCircleAgainstBox(
                            *circleObject,
                            *circleCollider,
                            *rigidBody,
                            *boxCollider
                        )
                    )
                    {
                        resolvedAnyCollision = true;
                    }
                }
            }

            if (!resolvedAnyCollision)
                break;
        }
    }

    bool PhysicsWorld2D::resolveCircleAgainstBox(
        GameObject& circleObject,
        CircleCollider2D& circleCollider,
        RigidBody2D& rigidBody,
        BoxCollider2D& boxCollider
    )
    {
        if (!circleCollider.overlaps(boxCollider))
            return false;

        circleCollider.setColliding(true);
        boxCollider.setColliding(true);

        const sf::Vector2f resolution =
            circleCollider.collisionResolutionVector(boxCollider);

        if (!std::isfinite(resolution.x) || !std::isfinite(resolution.y))
            return false;

        const double resolutionLength = std::hypot(
            static_cast<double>(resolution.x),
            static_cast<double>(resolution.y)
        );

        if (!std::isfinite(resolutionLength) || resolutionLength <= 0.0)
            return false;

        const sf::Vector2f contactNormal{
            static_cast<float>(resolution.x / resolutionLength),
            static_cast<float>(resolution.y / resolutionLength)
        };

        circleObject.transform.move(resolution);

        sf::Vector2f velocity = rigidBody.velocity();

        const float inwardNormalVelocity =
            velocity.x * contactNormal.x +
            velocity.y * contactNormal.y;

        if (inwardNormalVelocity < 0.f)
        {
            velocity -= contactNormal * inwardNormalVelocity;
        }

        if (contactNormal.y <= GROUND_NORMAL_Y_THRESHOLD)
            rigidBody.setGrounded(true);

        rigidBody.setVelocity(velocity);

        return true;
    }
}

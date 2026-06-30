#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

namespace l2d
{
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

            if (gameObject == nullptr || !gameObject->isActive())
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
        for (const auto& circleObjectPtr : scene.gameObjects())
        {
            GameObject* circleObject = circleObjectPtr.get();

            if (circleObject == nullptr || !circleObject->isActive())
                continue;

            auto* circleCollider = circleObject->getComponent<CircleCollider2D>();
            auto* rigidBody = circleObject->getComponent<RigidBody2D>();

            if (circleCollider == nullptr || rigidBody == nullptr)
                continue;

            for (const auto& boxObjectPtr : scene.gameObjects())
            {
                GameObject* boxObject = boxObjectPtr.get();

                if (boxObject == nullptr || !boxObject->isActive())
                    continue;

                if (boxObject == circleObject)
                    continue;

                auto* boxCollider = boxObject->getComponent<BoxCollider2D>();

                if (boxCollider == nullptr)
                    continue;

                resolveCircleAgainstBox(
                    *circleObject,
                    *circleCollider,
                    *rigidBody,
                    *boxCollider
                );
            }
        }
    }

    void PhysicsWorld2D::resolveCircleAgainstBox(
        GameObject& circleObject,
        CircleCollider2D& circleCollider,
        RigidBody2D& rigidBody,
        BoxCollider2D& boxCollider
    )
    {
        if (!circleCollider.overlaps(boxCollider))
            return;

        circleCollider.setColliding(true);
        boxCollider.setColliding(true);

        const sf::Vector2f resolution =
            circleCollider.collisionResolutionVector(boxCollider);

        circleObject.transform.move(resolution);

        sf::Vector2f velocity = rigidBody.velocity();

        if (resolution.x != 0.f)
            velocity.x = 0.f;

        if (resolution.y != 0.f)
        {
            velocity.y = 0.f;

            // Als resolution.y negatief is, werd de cirkel omhoog geduwd.
            // Dat betekent: hij stond op iets.
            if (resolution.y < 0.f)
            {
                rigidBody.setGrounded(true);
            }
        }

        rigidBody.setVelocity(velocity);
    }
}
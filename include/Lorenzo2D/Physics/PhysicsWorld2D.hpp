#pragma once

namespace l2d
{
    class Scene;
    class GameObject;
    class CircleCollider2D;
    class BoxCollider2D;
    class RigidBody2D;

    class PhysicsWorld2D
    {
    public:
        void step(Scene& scene, float deltaTime);

    private:
        void resetPhysicsStates(Scene& scene);
        void resolveCircleBoxCollisions(Scene& scene);

        void resolveCircleAgainstBox(
            GameObject& circleObject,
            CircleCollider2D& circleCollider,
            RigidBody2D& rigidBody,
            BoxCollider2D& boxCollider
        );
    };
}
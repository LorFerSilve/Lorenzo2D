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
        bool isPhysicsParticipant(
            const Scene& scene,
            const GameObject* gameObject
        ) const;

        void resetPhysicsStates(Scene& scene);
        void integrateRigidBodies(Scene& scene, float deltaTime);
        void resolveCircleBoxCollisions(Scene& scene);

        bool resolveCircleAgainstBox(
            GameObject& circleObject,
            CircleCollider2D& circleCollider,
            RigidBody2D& rigidBody,
            BoxCollider2D& boxCollider
        );
    };
}

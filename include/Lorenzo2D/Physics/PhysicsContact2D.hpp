#pragma once

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <cstdint>

namespace l2d
{
    enum class PhysicsContactPhase2D
    {
        Begin,
        Stay,
        End
    };

    struct PhysicsContact2D
    {
        GameObjectId firstObjectId = InvalidGameObjectId;
        GameObjectId secondObjectId = InvalidGameObjectId;
        ColliderType firstColliderType = ColliderType::Box;
        ColliderType secondColliderType = ColliderType::Box;
        CollisionManifold2D manifold;
        bool sensor = false;
    };

    struct PhysicsContactEvent2D
    {
        PhysicsContactPhase2D phase = PhysicsContactPhase2D::Begin;
        PhysicsContact2D contact;
    };
}

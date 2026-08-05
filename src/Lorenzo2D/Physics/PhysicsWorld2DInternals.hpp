#pragma once

#include <Lorenzo2D/Physics/PhysicsContact2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>

#include "UniformGridBroadPhase2D.hpp"

#include <vector>

namespace l2d
{
    class Collider2D;
    class GameObject;
    class RigidBody2D;

    namespace detail
    {
        struct PhysicsProxy2D
        {
            GameObject* object = nullptr;
            Collider2D* collider = nullptr;
            RigidBody2D* body = nullptr;
        };

        bool physicsContactLess(const PhysicsContact2D& left, const PhysicsContact2D& right);

        void buildPhysicsContactEvents(const std::vector<PhysicsContact2D>& previousContacts,
                                       const std::vector<PhysicsContact2D>& currentContacts,
                                       std::vector<PhysicsContactEvent2D>& events);
    }

    struct PhysicsWorld2D::BroadPhaseStepData2D
    {
        std::vector<detail::PhysicsProxy2D> proxies;
        detail::BroadPhaseBuildResult2D broadPhaseResult;
        PhysicsBroadPhaseStats2D stats;
    };
}

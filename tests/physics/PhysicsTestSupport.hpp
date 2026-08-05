#pragma once

#include "../TestSupport.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>
#include <Lorenzo2D/Physics/PhysicsContact2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace l2d::test::physics
{
    using l2d::test::runTest;

    inline constexpr float kPhysicsComparisonEpsilon = 0.001f;

    inline bool isFinite(sf::Vector2f value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    inline bool broadPhaseStatsEqual(const l2d::PhysicsBroadPhaseStats2D& left,
                                     const l2d::PhysicsBroadPhaseStats2D& right)
    {
        return left.proxyCount == right.proxyCount &&
               left.occupiedCellCount == right.occupiedCellCount &&
               left.fallbackProxyCount == right.fallbackProxyCount &&
               left.bruteForcePairCount == right.bruteForcePairCount &&
               left.candidatePairCount == right.candidatePairCount &&
               left.narrowPhaseTestCount == right.narrowPhaseTestCount;
    }

    inline void requireEquivalentContact(const l2d::PhysicsContact2D& left,
                                         const l2d::PhysicsContact2D& right)
    {
        L2D_REQUIRE_EQUAL(left.firstObjectId, right.firstObjectId);
        L2D_REQUIRE_EQUAL(left.secondObjectId, right.secondObjectId);
        L2D_REQUIRE_EQUAL(left.firstColliderType, right.firstColliderType);
        L2D_REQUIRE_EQUAL(left.secondColliderType, right.secondColliderType);
        L2D_REQUIRE_EQUAL(left.sensor, right.sensor);
        L2D_REQUIRE_APPROX(left.manifold.normal, right.manifold.normal, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(left.manifold.point, right.manifold.point, kPhysicsComparisonEpsilon);
        L2D_REQUIRE_APPROX(left.manifold.penetration, right.manifold.penetration,
                           kPhysicsComparisonEpsilon);
    }

    inline void requireEquivalentContacts(const std::vector<l2d::PhysicsContact2D>& left,
                                          const std::vector<l2d::PhysicsContact2D>& right)
    {
        L2D_REQUIRE_EQUAL(left.size(), right.size());

        for (std::size_t index = 0; index < left.size(); ++index)
            requireEquivalentContact(left[index], right[index]);
    }

    inline void requireEquivalentContactEvents(const std::vector<l2d::PhysicsContactEvent2D>& left,
                                               const std::vector<l2d::PhysicsContactEvent2D>& right)
    {
        L2D_REQUIRE_EQUAL(left.size(), right.size());

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            L2D_REQUIRE_EQUAL(left[index].phase, right[index].phase);
            requireEquivalentContact(left[index].contact, right[index].contact);
        }
    }

    inline bool containsContactPhase(const std::vector<l2d::PhysicsContactEvent2D>& events,
                                     l2d::PhysicsContactPhase2D phase)
    {
        for (const l2d::PhysicsContactEvent2D& event : events)
        {
            if (event.phase == phase) return true;
        }

        return false;
    }

    inline l2d::PhysicsWorld2DConfig zeroGravityConfig()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 0.f};
        return config;
    }

    inline void fixedStep(l2d::Scene& scene, l2d::PhysicsWorld2D& world,
                          float deltaTime = 1.f / 60.f)
    {
        scene.fixedUpdate(deltaTime);
        world.step(scene, deltaTime);
    }

    inline l2d::PhysicsMaterial2D material(float restitution, float staticFriction = 0.f,
                                           float dynamicFriction = 0.f)
    {
        return {restitution, staticFriction, dynamicFriction};
    }

    struct CircleBody
    {
        l2d::GameObject& object;
        l2d::CircleCollider2D& collider;
        l2d::RigidBody2D* body;
    };

    inline CircleBody createCircle(l2d::Scene& scene, const std::string& name,
                                   sf::Vector2f position, float radius, bool withBody = true,
                                   l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::CircleCollider2D& collider = object.addComponent<l2d::CircleCollider2D>(radius);
        collider.setOffset({radius, radius});

        return {object, collider, body};
    }

    struct BoxBody
    {
        l2d::GameObject& object;
        l2d::BoxCollider2D& collider;
        l2d::RigidBody2D* body;
    };

    inline BoxBody createBox(l2d::Scene& scene, const std::string& name, sf::Vector2f position,
                             sf::Vector2f size, bool withBody = true,
                             l2d::BodyType2D bodyType = l2d::BodyType2D::Dynamic)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(position);

        l2d::RigidBody2D* body = nullptr;

        if (withBody)
        {
            body = &object.addComponent<l2d::RigidBody2D>();
            body->setBodyType(bodyType);
        }

        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);

        return {object, collider, body};
    }
}

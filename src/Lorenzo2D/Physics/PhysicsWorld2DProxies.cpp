#include "PhysicsWorld2DInternals.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <memory>
#include <vector>

#include "PhysicsGeometry2D.hpp"

namespace l2d
{
    namespace
    {
        bool colliderBounds(const Collider2D& collider, sf::Vector2f& minimum,
                            sf::Vector2f& maximum)
        {
            detail::ColliderGeometry2D geometry;
            if (!detail::buildColliderGeometry(collider, geometry)) return false;
            minimum = geometry.bounds.minimum;
            maximum = geometry.bounds.maximum;
            return true;
        }

        bool hasMotion(const detail::PhysicsProxy2D& proxy)
        {
            return proxy.body != nullptr && proxy.body->bodyType() != BodyType2D::Static;
        }

        bool physicsProxyLess(const detail::PhysicsProxy2D& left,
                              const detail::PhysicsProxy2D& right)
        {
            if (left.object == nullptr) return right.object != nullptr;

            if (right.object == nullptr) return false;

            if (left.object->id() != right.object->id())
            {
                return left.object->id() < right.object->id();
            }

            if (left.collider == nullptr) return right.collider != nullptr;
            if (right.collider == nullptr) return false;

            return left.collider->id() < right.collider->id();
        }
    }

    PhysicsWorld2D::BroadPhaseStepData2D PhysicsWorld2D::buildBroadPhaseStepData(Scene& scene) const
    {
        BroadPhaseStepData2D stepData;
        stepData.proxies.reserve(scene.gameObjects().size());

        for (const std::unique_ptr<GameObject>& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();

            if (!isPhysicsParticipant(scene, gameObject)) continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();

            if (body != nullptr && !body->isActive()) body = nullptr;

            for (Collider2D* collider : gameObject->getComponents<Collider2D>())
            {
                if (collider == nullptr || !collider->isActive()) continue;

                stepData.proxies.push_back({gameObject, collider, body});
            }
        }

        std::sort(stepData.proxies.begin(), stepData.proxies.end(), physicsProxyLess);

        std::vector<detail::BroadPhaseProxy2D> broadPhaseProxies;
        broadPhaseProxies.reserve(stepData.proxies.size());

        for (const detail::PhysicsProxy2D& proxy : stepData.proxies)
        {
            detail::BroadPhaseProxy2D broadPhaseProxy;
            broadPhaseProxy.moving = hasMotion(proxy);

            if (proxy.collider != nullptr)
            {
                broadPhaseProxy.boundsValid = colliderBounds(
                    *proxy.collider, broadPhaseProxy.minimum, broadPhaseProxy.maximum);
            }

            broadPhaseProxies.push_back(broadPhaseProxy);
        }

        stepData.stats.proxyCount = broadPhaseProxies.size();
        stepData.stats.bruteForcePairCount = detail::countBruteForcePairs(broadPhaseProxies);

        if (m_config.broadPhaseMode == PhysicsBroadPhaseMode2D::BruteForce)
        {
            stepData.broadPhaseResult = detail::buildBruteForcePairs(broadPhaseProxies);
        }
        else
        {
            stepData.broadPhaseResult =
                detail::buildUniformGridPairs(broadPhaseProxies, m_config.broadPhaseCellSize,
                                              m_config.broadPhaseMaxCellsPerProxy);
        }

        stepData.stats.occupiedCellCount = stepData.broadPhaseResult.occupiedCellCount;
        stepData.stats.fallbackProxyCount = stepData.broadPhaseResult.fallbackProxyCount;
        stepData.stats.candidatePairCount = stepData.broadPhaseResult.pairs.size();

        return stepData;
    }
}

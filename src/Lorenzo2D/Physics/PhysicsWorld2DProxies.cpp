#include "PhysicsWorld2DInternals.hpp"

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace l2d
{
    namespace
    {
        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool conservativeFloat(double value, bool lowerBound, float& result)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(value) || value < -maximum || value > maximum) return false;

            result = static_cast<float>(value);
            const double converted = static_cast<double>(result);
            const float infinity = std::numeric_limits<float>::infinity();

            if (lowerBound && converted > value)
                result = std::nextafter(result, -infinity);
            else if (!lowerBound && converted < value)
                result = std::nextafter(result, infinity);

            return std::isfinite(result);
        }

        bool colliderBounds(const Collider2D& collider, sf::Vector2f& minimum,
                            sf::Vector2f& maximum)
        {
            if (collider.type() == ColliderType::Box)
            {
                const BoxCollider2D* box = dynamic_cast<const BoxCollider2D*>(&collider);

                if (box == nullptr) return false;

                minimum = box->min();
                maximum = box->max();
            }
            else if (collider.type() == ColliderType::Circle)
            {
                const CircleCollider2D* circle = dynamic_cast<const CircleCollider2D*>(&collider);

                if (circle == nullptr) return false;

                const sf::Vector2f center = circle->center();
                const double radius = circle->radius();
                const double minimumX = static_cast<double>(center.x) - radius;
                const double minimumY = static_cast<double>(center.y) - radius;
                const double maximumX = static_cast<double>(center.x) + radius;
                const double maximumY = static_cast<double>(center.y) + radius;

                if (!isFinite(center) || !std::isfinite(radius) ||
                    !conservativeFloat(minimumX, true, minimum.x) ||
                    !conservativeFloat(minimumY, true, minimum.y) ||
                    !conservativeFloat(maximumX, false, maximum.x) ||
                    !conservativeFloat(maximumY, false, maximum.y))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            if (!isFinite(minimum) || !isFinite(maximum) || minimum.x > maximum.x ||
                minimum.y > maximum.y)
            {
                return false;
            }

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

            return left.object->id() < right.object->id();
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

            Collider2D* collider = gameObject->getComponent<Collider2D>();

            if (collider == nullptr || !collider->isActive()) continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();

            if (body != nullptr && !body->isActive()) body = nullptr;

            stepData.proxies.push_back({gameObject, collider, body});
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

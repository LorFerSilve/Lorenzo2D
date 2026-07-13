#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>
#include <Lorenzo2D/Physics/PhysicsMaterial2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "UniformGridBroadPhase2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace l2d
{
    namespace
    {
        constexpr sf::Vector2f DEFAULT_GRAVITY = { 0.f, 980.f };
        constexpr std::uint32_t DEFAULT_VELOCITY_ITERATIONS = 8;
        constexpr std::uint32_t DEFAULT_POSITION_ITERATIONS = 3;
        constexpr float DEFAULT_POSITION_CORRECTION_PERCENT = 0.8f;
        constexpr float DEFAULT_PENETRATION_SLOP = 0.01f;
        constexpr float DEFAULT_RESTITUTION_VELOCITY_THRESHOLD = 1.f;
        constexpr float DEFAULT_GROUNDED_NORMAL_THRESHOLD = 0.7f;
        constexpr float DEFAULT_BROAD_PHASE_CELL_SIZE = 128.f;
        constexpr std::uint32_t DEFAULT_BROAD_PHASE_MAX_CELLS_PER_PROXY = 256;

        struct PhysicsProxy2D
        {
            GameObject* object = nullptr;
            Collider2D* collider = nullptr;
            RigidBody2D* body = nullptr;
        };

        struct ContactConstraint2D
        {
            PhysicsProxy2D* first = nullptr;
            PhysicsProxy2D* second = nullptr;

            PhysicsContact2D contact;

            double restitutionBias = 0.0;
            double staticFriction = 0.0;
            double dynamicFriction = 0.0;

            double accumulatedNormalImpulse = 0.0;
            double accumulatedTangentImpulse = 0.0;
        };

        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool conservativeFloat(
            double value,
            bool lowerBound,
            float& result
        )
        {
            const double maximum = static_cast<double>(
                std::numeric_limits<float>::max()
            );

            if (!std::isfinite(value) || value < -maximum || value > maximum)
                return false;

            result = static_cast<float>(value);
            const double converted = static_cast<double>(result);
            const float infinity = std::numeric_limits<float>::infinity();

            if (lowerBound && converted > value)
                result = std::nextafter(result, -infinity);
            else if (!lowerBound && converted < value)
                result = std::nextafter(result, infinity);

            return std::isfinite(result);
        }

        bool colliderBounds(
            const Collider2D& collider,
            sf::Vector2f& minimum,
            sf::Vector2f& maximum
        )
        {
            if (collider.type() == ColliderType::Box)
            {
                const auto* box = dynamic_cast<const BoxCollider2D*>(
                    &collider
                );

                if (box == nullptr)
                    return false;

                minimum = box->min();
                maximum = box->max();
            }
            else if (collider.type() == ColliderType::Circle)
            {
                const auto* circle = dynamic_cast<const CircleCollider2D*>(
                    &collider
                );

                if (circle == nullptr)
                    return false;

                const sf::Vector2f center = circle->center();
                const double radius = circle->radius();
                const double minimumX =
                    static_cast<double>(center.x) - radius;
                const double minimumY =
                    static_cast<double>(center.y) - radius;
                const double maximumX =
                    static_cast<double>(center.x) + radius;
                const double maximumY =
                    static_cast<double>(center.y) + radius;

                if (
                    !isFinite(center) ||
                    !std::isfinite(radius) ||
                    !conservativeFloat(
                        minimumX,
                        true,
                        minimum.x
                    ) ||
                    !conservativeFloat(
                        minimumY,
                        true,
                        minimum.y
                    ) ||
                    !conservativeFloat(
                        maximumX,
                        false,
                        maximum.x
                    ) ||
                    !conservativeFloat(
                        maximumY,
                        false,
                        maximum.y
                    )
                )
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            if (
                !isFinite(minimum) ||
                !isFinite(maximum) ||
                minimum.x > maximum.x ||
                minimum.y > maximum.y
            )
            {
                return false;
            }

            return true;
        }

        sf::Vector2f effectiveVelocity(const PhysicsProxy2D& proxy)
        {
            if (proxy.body == nullptr)
                return { 0.f, 0.f };

            if (proxy.body->bodyType() == BodyType2D::Static)
                return { 0.f, 0.f };

            return proxy.body->velocity();
        }

        double inverseMass(const PhysicsProxy2D& proxy)
        {
            if (
                proxy.body == nullptr ||
                proxy.body->bodyType() != BodyType2D::Dynamic
            )
            {
                return 0.0;
            }

            return 1.0 / static_cast<double>(proxy.body->mass());
        }

        double relativeVelocityAlong(
            const PhysicsProxy2D& first,
            const PhysicsProxy2D& second,
            sf::Vector2f direction
        )
        {
            const sf::Vector2f firstVelocity = effectiveVelocity(first);
            const sf::Vector2f secondVelocity = effectiveVelocity(second);

            const double relativeX =
                static_cast<double>(secondVelocity.x) -
                static_cast<double>(firstVelocity.x);
            const double relativeY =
                static_cast<double>(secondVelocity.y) -
                static_cast<double>(firstVelocity.y);

            return
                relativeX * static_cast<double>(direction.x) +
                relativeY * static_cast<double>(direction.y);
        }

        bool scaledVector(
            sf::Vector2f direction,
            double scale,
            sf::Vector2f& result
        )
        {
            const double x = static_cast<double>(direction.x) * scale;
            const double y = static_cast<double>(direction.y) * scale;
            const double maximum =
                static_cast<double>(std::numeric_limits<float>::max());

            if (
                !std::isfinite(x) ||
                !std::isfinite(y) ||
                std::fabs(x) > maximum ||
                std::fabs(y) > maximum
            )
            {
                return false;
            }

            result = {
                static_cast<float>(x),
                static_cast<float>(y)
            };
            return true;
        }

        bool moveObject(
            GameObject* object,
            sf::Vector2f direction,
            double distance
        )
        {
            if (object == nullptr)
                return false;

            sf::Vector2f offset;

            if (!scaledVector(direction, distance, offset))
                return false;

            const sf::Vector2f position = object->transform.position();
            const double x =
                static_cast<double>(position.x) +
                static_cast<double>(offset.x);
            const double y =
                static_cast<double>(position.y) +
                static_cast<double>(offset.y);
            const double maximum =
                static_cast<double>(std::numeric_limits<float>::max());

            if (
                !std::isfinite(x) ||
                !std::isfinite(y) ||
                std::fabs(x) > maximum ||
                std::fabs(y) > maximum
            )
            {
                return false;
            }

            object->transform.setPosition({
                static_cast<float>(x),
                static_cast<float>(y)
            });
            return true;
        }

        bool addScaledVelocity(
            RigidBody2D* body,
            sf::Vector2f direction,
            double scale
        )
        {
            if (body == nullptr)
                return false;

            const sf::Vector2f velocity = body->velocity();
            const double x =
                static_cast<double>(velocity.x) +
                static_cast<double>(direction.x) * scale;
            const double y =
                static_cast<double>(velocity.y) +
                static_cast<double>(direction.y) * scale;
            const double maximum =
                static_cast<double>(std::numeric_limits<float>::max());

            if (
                !std::isfinite(x) ||
                !std::isfinite(y) ||
                std::fabs(x) > maximum ||
                std::fabs(y) > maximum
            )
            {
                return false;
            }

            body->setVelocity({
                static_cast<float>(x),
                static_cast<float>(y)
            });
            return true;
        }

        bool hasMotion(const PhysicsProxy2D& proxy)
        {
            return proxy.body != nullptr &&
                proxy.body->bodyType() != BodyType2D::Static;
        }

        bool proxyLess(
            const PhysicsProxy2D& left,
            const PhysicsProxy2D& right
        )
        {
            if (left.object == nullptr)
                return right.object != nullptr;

            if (right.object == nullptr)
                return false;

            return left.object->id() < right.object->id();
        }

        auto contactKey(const PhysicsContact2D& contact)
        {
            return std::make_tuple(
                contact.firstObjectId,
                contact.secondObjectId,
                static_cast<int>(contact.firstColliderType),
                static_cast<int>(contact.secondColliderType)
            );
        }

        bool contactLess(
            const PhysicsContact2D& left,
            const PhysicsContact2D& right
        )
        {
            return contactKey(left) < contactKey(right);
        }

        bool sameContactKey(
            const PhysicsContact2D& left,
            const PhysicsContact2D& right
        )
        {
            return contactKey(left) == contactKey(right);
        }

        bool constraintLess(
            const ContactConstraint2D& left,
            const ContactConstraint2D& right
        )
        {
            return contactLess(left.contact, right.contact);
        }

        float sanitizeUnitInterval(float value, float fallback)
        {
            if (!std::isfinite(value))
                return fallback;

            return std::clamp(value, 0.f, 1.f);
        }

        float sanitizeNonNegative(float value, float fallback)
        {
            if (!std::isfinite(value) || value < 0.f)
                return fallback;

            return value;
        }

        PhysicsWorld2DConfig sanitizeConfig(PhysicsWorld2DConfig config)
        {
            if (!isFinite(config.gravity))
                config.gravity = DEFAULT_GRAVITY;

            if (config.velocityIterations == 0)
            {
                config.velocityIterations =
                    DEFAULT_VELOCITY_ITERATIONS;
            }

            if (config.positionIterations == 0)
            {
                config.positionIterations =
                    DEFAULT_POSITION_ITERATIONS;
            }

            config.positionCorrectionPercent = sanitizeUnitInterval(
                config.positionCorrectionPercent,
                DEFAULT_POSITION_CORRECTION_PERCENT
            );

            config.penetrationSlop = sanitizeNonNegative(
                config.penetrationSlop,
                DEFAULT_PENETRATION_SLOP
            );

            config.restitutionVelocityThreshold = sanitizeNonNegative(
                config.restitutionVelocityThreshold,
                DEFAULT_RESTITUTION_VELOCITY_THRESHOLD
            );

            config.groundedNormalThreshold = sanitizeUnitInterval(
                config.groundedNormalThreshold,
                DEFAULT_GROUNDED_NORMAL_THRESHOLD
            );

            switch (config.broadPhaseMode)
            {
            case PhysicsBroadPhaseMode2D::UniformGrid:
            case PhysicsBroadPhaseMode2D::BruteForce:
                break;
            default:
                config.broadPhaseMode =
                    PhysicsBroadPhaseMode2D::UniformGrid;
                break;
            }

            if (
                !std::isfinite(config.broadPhaseCellSize) ||
                config.broadPhaseCellSize <= 0.f
            )
            {
                config.broadPhaseCellSize =
                    DEFAULT_BROAD_PHASE_CELL_SIZE;
            }

            if (config.broadPhaseMaxCellsPerProxy == 0)
            {
                config.broadPhaseMaxCellsPerProxy =
                    DEFAULT_BROAD_PHASE_MAX_CELLS_PER_PROXY;
            }

            return config;
        }

        void applyVelocityImpulse(
            PhysicsProxy2D& first,
            PhysicsProxy2D& second,
            sf::Vector2f direction,
            double magnitude
        )
        {
            const double firstInverseMass = inverseMass(first);
            const double secondInverseMass = inverseMass(second);

            if (
                first.body != nullptr &&
                firstInverseMass > 0.0
            )
            {
                addScaledVelocity(
                    first.body,
                    direction,
                    -magnitude * firstInverseMass
                );
            }

            if (
                second.body != nullptr &&
                secondInverseMass > 0.0
            )
            {
                addScaledVelocity(
                    second.body,
                    direction,
                    magnitude * secondInverseMass
                );
            }
        }

        void solveVelocity(ContactConstraint2D& constraint)
        {
            PhysicsProxy2D& first = *constraint.first;
            PhysicsProxy2D& second = *constraint.second;

            const double firstInverseMass = inverseMass(first);
            const double secondInverseMass = inverseMass(second);
            const double inverseMassSum =
                firstInverseMass + secondInverseMass;

            if (inverseMassSum <= 0.0 || !std::isfinite(inverseMassSum))
                return;

            const sf::Vector2f normal = constraint.contact.manifold.normal;
            const double normalVelocity = relativeVelocityAlong(
                first,
                second,
                normal
            );
            const double normalImpulseDelta =
                (constraint.restitutionBias - normalVelocity) /
                inverseMassSum;

            const double previousNormalImpulse =
                constraint.accumulatedNormalImpulse;

            if (!std::isfinite(normalImpulseDelta))
                return;

            constraint.accumulatedNormalImpulse = std::max(
                previousNormalImpulse + normalImpulseDelta,
                0.0
            );

            const double appliedNormalImpulse =
                constraint.accumulatedNormalImpulse -
                previousNormalImpulse;

            applyVelocityImpulse(
                first,
                second,
                normal,
                appliedNormalImpulse
            );

            const sf::Vector2f tangent{ -normal.y, normal.x };
            const double tangentVelocity = relativeVelocityAlong(
                first,
                second,
                tangent
            );
            const double tangentImpulseDelta =
                -tangentVelocity / inverseMassSum;

            const double previousTangentImpulse =
                constraint.accumulatedTangentImpulse;
            const double candidateTangentImpulse =
                previousTangentImpulse + tangentImpulseDelta;

            if (!std::isfinite(candidateTangentImpulse))
                return;

            const double staticLimit =
                constraint.staticFriction *
                constraint.accumulatedNormalImpulse;

            if (std::fabs(candidateTangentImpulse) <= staticLimit)
            {
                constraint.accumulatedTangentImpulse =
                    candidateTangentImpulse;
            }
            else
            {
                const double dynamicLimit =
                    constraint.dynamicFriction *
                    constraint.accumulatedNormalImpulse;

                constraint.accumulatedTangentImpulse = std::clamp(
                    candidateTangentImpulse,
                    -dynamicLimit,
                    dynamicLimit
                );
            }

            const double appliedTangentImpulse =
                constraint.accumulatedTangentImpulse -
                previousTangentImpulse;

            applyVelocityImpulse(
                first,
                second,
                tangent,
                appliedTangentImpulse
            );
        }

        bool solvePosition(
            ContactConstraint2D& constraint,
            const PhysicsWorld2DConfig& config
        )
        {
            PhysicsProxy2D& first = *constraint.first;
            PhysicsProxy2D& second = *constraint.second;

            const double firstInverseMass = inverseMass(first);
            const double secondInverseMass = inverseMass(second);
            const double inverseMassSum =
                firstInverseMass + secondInverseMass;

            if (inverseMassSum <= 0.0 || !std::isfinite(inverseMassSum))
                return false;

            CollisionManifold2D manifold;

            if (!computeCollisionManifold(
                *first.collider,
                *second.collider,
                manifold
            ))
            {
                return false;
            }

            const double correctablePenetration = std::max(
                static_cast<double>(manifold.penetration) -
                    static_cast<double>(config.penetrationSlop),
                0.0
            );

            if (correctablePenetration <= 0.0)
                return false;

            const double correctionDistance =
                static_cast<double>(config.positionCorrectionPercent) *
                correctablePenetration;
            bool moved = false;

            if (firstInverseMass > 0.0)
            {
                moved = moveObject(
                    first.object,
                    manifold.normal,
                    -correctionDistance *
                        (firstInverseMass / inverseMassSum)
                ) || moved;
            }

            if (secondInverseMass > 0.0)
            {
                moved = moveObject(
                    second.object,
                    manifold.normal,
                    correctionDistance *
                        (secondInverseMass / inverseMassSum)
                ) || moved;
            }

            return moved;
        }

        void buildContactEvents(
            const std::vector<PhysicsContact2D>& previousContacts,
            const std::vector<PhysicsContact2D>& currentContacts,
            std::vector<PhysicsContactEvent2D>& events
        )
        {
            events.clear();

            std::size_t previousIndex = 0;
            std::size_t currentIndex = 0;

            while (
                previousIndex < previousContacts.size() ||
                currentIndex < currentContacts.size()
            )
            {
                if (previousIndex >= previousContacts.size())
                {
                    events.push_back({
                        PhysicsContactPhase2D::Begin,
                        currentContacts[currentIndex]
                    });
                    ++currentIndex;
                    continue;
                }

                if (currentIndex >= currentContacts.size())
                {
                    events.push_back({
                        PhysicsContactPhase2D::End,
                        previousContacts[previousIndex]
                    });
                    ++previousIndex;
                    continue;
                }

                const PhysicsContact2D& previous =
                    previousContacts[previousIndex];
                const PhysicsContact2D& current =
                    currentContacts[currentIndex];

                if (sameContactKey(previous, current))
                {
                    events.push_back({
                        PhysicsContactPhase2D::Stay,
                        current
                    });
                    ++previousIndex;
                    ++currentIndex;
                }
                else if (contactLess(previous, current))
                {
                    events.push_back({
                        PhysicsContactPhase2D::End,
                        previous
                    });
                    ++previousIndex;
                }
                else
                {
                    events.push_back({
                        PhysicsContactPhase2D::Begin,
                        current
                    });
                    ++currentIndex;
                }
            }
        }
    }

    PhysicsWorld2D::PhysicsWorld2D()
        : PhysicsWorld2D(PhysicsWorld2DConfig{})
    {
    }

    PhysicsWorld2D::PhysicsWorld2D(const PhysicsWorld2DConfig& config)
        : m_config(sanitizeConfig(config))
    {
    }

    const PhysicsWorld2DConfig& PhysicsWorld2D::config() const
    {
        return m_config;
    }

    void PhysicsWorld2D::setConfig(const PhysicsWorld2DConfig& config)
    {
        m_config = sanitizeConfig(config);
    }

    const PhysicsBroadPhaseStats2D&
    PhysicsWorld2D::broadPhaseStats() const
    {
        return m_broadPhaseStats;
    }

    const std::vector<PhysicsContact2D>& PhysicsWorld2D::contacts() const
    {
        return m_contacts;
    }

    const std::vector<PhysicsContactEvent2D>&
    PhysicsWorld2D::contactEvents() const
    {
        return m_contactEvents;
    }

    bool PhysicsWorld2D::isTouching(
        GameObjectId firstObjectId,
        GameObjectId secondObjectId
    ) const
    {
        for (const PhysicsContact2D& contact : m_contacts)
        {
            const bool forward =
                contact.firstObjectId == firstObjectId &&
                contact.secondObjectId == secondObjectId;
            const bool reverse =
                contact.firstObjectId == secondObjectId &&
                contact.secondObjectId == firstObjectId;

            if (forward || reverse)
                return true;
        }

        return false;
    }

    void PhysicsWorld2D::reset()
    {
        m_broadPhaseStats = PhysicsBroadPhaseStats2D{};
        m_contacts.clear();
        m_contactEvents.clear();
        m_contactSceneToken.reset();
    }

    void PhysicsWorld2D::reset(Scene& scene)
    {
        resetPhysicsStates(scene);
        reset();
    }

    void PhysicsWorld2D::step(Scene& scene, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.f)
            return;

        const std::shared_ptr<void> sceneToken = scene.m_handleState;

        if (m_contactSceneToken.lock() != sceneToken)
        {
            reset();
            m_contactSceneToken = sceneToken;
        }

        const std::vector<PhysicsContact2D> previousContacts = m_contacts;

        m_contacts.clear();
        m_contactEvents.clear();

        resetPhysicsStates(scene);
        integrateRigidBodies(scene, deltaTime);

        m_broadPhaseStats = PhysicsBroadPhaseStats2D{};

        std::vector<PhysicsProxy2D> proxies;
        proxies.reserve(scene.gameObjects().size());

        for (const auto& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();

            if (!isPhysicsParticipant(scene, gameObject))
                continue;

            Collider2D* collider = gameObject->getComponent<Collider2D>();

            if (collider == nullptr || !collider->isActive())
                continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();

            if (body != nullptr && !body->isActive())
                body = nullptr;

            proxies.push_back({ gameObject, collider, body });
        }

        std::sort(proxies.begin(), proxies.end(), proxyLess);

        std::vector<detail::BroadPhaseProxy2D> broadPhaseProxies;
        broadPhaseProxies.reserve(proxies.size());

        for (const PhysicsProxy2D& proxy : proxies)
        {
            detail::BroadPhaseProxy2D broadPhaseProxy;
            broadPhaseProxy.moving = hasMotion(proxy);

            if (proxy.collider != nullptr)
            {
                broadPhaseProxy.boundsValid = colliderBounds(
                    *proxy.collider,
                    broadPhaseProxy.minimum,
                    broadPhaseProxy.maximum
                );
            }

            broadPhaseProxies.push_back(broadPhaseProxy);
        }

        m_broadPhaseStats.proxyCount = broadPhaseProxies.size();
        m_broadPhaseStats.bruteForcePairCount =
            detail::countBruteForcePairs(broadPhaseProxies);

        detail::BroadPhaseBuildResult2D broadPhaseResult;

        if (
            m_config.broadPhaseMode ==
            PhysicsBroadPhaseMode2D::BruteForce
        )
        {
            broadPhaseResult =
                detail::buildBruteForcePairs(broadPhaseProxies);
        }
        else
        {
            broadPhaseResult = detail::buildUniformGridPairs(
                broadPhaseProxies,
                m_config.broadPhaseCellSize,
                m_config.broadPhaseMaxCellsPerProxy
            );
        }

        m_broadPhaseStats.occupiedCellCount =
            broadPhaseResult.occupiedCellCount;
        m_broadPhaseStats.fallbackProxyCount =
            broadPhaseResult.fallbackProxyCount;
        m_broadPhaseStats.candidatePairCount =
            broadPhaseResult.pairs.size();

        std::vector<ContactConstraint2D> constraints;

        auto addPair = [this, &constraints](
            PhysicsProxy2D& left,
            PhysicsProxy2D& right
        )
        {
            PhysicsProxy2D* first = &left;
            PhysicsProxy2D* second = &right;

            if (
                first->object == nullptr ||
                second->object == nullptr ||
                first->collider == nullptr ||
                second->collider == nullptr
            )
            {
                return;
            }

            if (second->object->id() < first->object->id())
                std::swap(first, second);

            if (!first->collider->canCollideWith(*second->collider))
                return;

            ++m_broadPhaseStats.narrowPhaseTestCount;

            CollisionManifold2D manifold;

            if (!computeCollisionManifold(
                *first->collider,
                *second->collider,
                manifold
            ))
            {
                return;
            }

            if (
                !isFinite(manifold.normal) ||
                !isFinite(manifold.point) ||
                !std::isfinite(manifold.penetration) ||
                manifold.penetration < 0.f
            )
            {
                return;
            }

            PhysicsContact2D contact;
            contact.firstObjectId = first->object->id();
            contact.secondObjectId = second->object->id();
            contact.firstColliderType = first->collider->type();
            contact.secondColliderType = second->collider->type();
            contact.manifold = manifold;
            contact.sensor =
                first->collider->isSensor() ||
                second->collider->isSensor();

            first->collider->setColliding(true);
            second->collider->setColliding(true);

            m_contacts.push_back(contact);

            if (contact.sensor)
                return;

            const double inverseMassSum =
                inverseMass(*first) + inverseMass(*second);

            if (inverseMassSum <= 0.0)
                return;

            const PhysicsMaterial2D& firstMaterial =
                first->collider->material();
            const PhysicsMaterial2D& secondMaterial =
                second->collider->material();

            const float restitution = std::max(
                firstMaterial.restitution,
                secondMaterial.restitution
            );

            const double initialNormalVelocity = relativeVelocityAlong(
                *first,
                *second,
                manifold.normal
            );

            double restitutionBias = 0.0;

            if (
                initialNormalVelocity <
                -m_config.restitutionVelocityThreshold
            )
            {
                restitutionBias =
                    -static_cast<double>(restitution) *
                    initialNormalVelocity;
            }

            constraints.push_back({
                first,
                second,
                contact,
                restitutionBias,
                std::sqrt(
                    firstMaterial.staticFriction *
                    secondMaterial.staticFriction
                ),
                std::sqrt(
                    firstMaterial.dynamicFriction *
                    secondMaterial.dynamicFriction
                ),
                0.0,
                0.0
            });
        };

        for (const detail::BroadPhasePair2D& pair : broadPhaseResult.pairs)
        {
            addPair(proxies[pair.first], proxies[pair.second]);
        }

        std::sort(m_contacts.begin(), m_contacts.end(), contactLess);
        std::sort(constraints.begin(), constraints.end(), constraintLess);

        for (std::uint32_t iteration = 0;
            iteration < m_config.velocityIterations;
            ++iteration)
        {
            for (ContactConstraint2D& constraint : constraints)
                solveVelocity(constraint);
        }

        for (std::uint32_t iteration = 0;
            iteration < m_config.positionIterations;
            ++iteration)
        {
            bool correctedAny = false;

            for (ContactConstraint2D& constraint : constraints)
            {
                if (solvePosition(constraint, m_config))
                    correctedAny = true;
            }

            if (!correctedAny)
                break;
        }

        for (const ContactConstraint2D& constraint : constraints)
        {
            const sf::Vector2f normal =
                constraint.contact.manifold.normal;

            if (
                constraint.first->body != nullptr &&
                constraint.first->body->bodyType() ==
                    BodyType2D::Dynamic &&
                normal.y >= m_config.groundedNormalThreshold
            )
            {
                constraint.first->body->setGrounded(true);
            }

            if (
                constraint.second->body != nullptr &&
                constraint.second->body->bodyType() ==
                    BodyType2D::Dynamic &&
                normal.y <= -m_config.groundedNormalThreshold
            )
            {
                constraint.second->body->setGrounded(true);
            }
        }

        buildContactEvents(
            previousContacts,
            m_contacts,
            m_contactEvents
        );
    }

    bool PhysicsWorld2D::isPhysicsParticipant(
        const Scene& scene,
        const GameObject* gameObject
    ) const
    {
        return gameObject != nullptr &&
            gameObject->isActive() &&
            !gameObject->isDestroyQueued() &&
            scene.isFixedStepParticipant(*gameObject);
    }

    void PhysicsWorld2D::resetPhysicsStates(Scene& scene)
    {
        for (const auto& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();

            if (gameObject == nullptr)
                continue;

            if (auto* collider = gameObject->getComponent<Collider2D>())
                collider->setColliding(false);

            if (auto* body = gameObject->getComponent<RigidBody2D>())
                body->setGrounded(false);
        }
    }

    void PhysicsWorld2D::integrateRigidBodies(
        Scene& scene,
        float deltaTime
    )
    {
        for (const auto& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();

            if (!isPhysicsParticipant(scene, gameObject))
                continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();

            if (body == nullptr || !body->isActive())
                continue;

            body->integrate(deltaTime, m_config.gravity);
        }
    }
}

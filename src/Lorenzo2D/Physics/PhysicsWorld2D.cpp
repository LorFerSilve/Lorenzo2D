#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>
#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>
#include <Lorenzo2D/Physics/PhysicsMaterial2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "PhysicsWorld2DInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace l2d
{
    namespace
    {
        constexpr sf::Vector2f DEFAULT_GRAVITY = {0.f, 980.f};
        constexpr std::uint32_t DEFAULT_VELOCITY_ITERATIONS = 8;
        constexpr std::uint32_t DEFAULT_POSITION_ITERATIONS = 3;
        constexpr std::uint32_t MAXIMUM_SOLVER_ITERATIONS = 64;
        constexpr std::uint32_t DEFAULT_MAXIMUM_CCD_SUBSTEPS = 32;
        constexpr std::uint32_t MAXIMUM_CCD_SUBSTEPS = 64;
        constexpr float DEFAULT_POSITION_CORRECTION_PERCENT = 0.8f;
        constexpr float DEFAULT_PENETRATION_SLOP = 0.01f;
        constexpr float DEFAULT_RESTITUTION_VELOCITY_THRESHOLD = 1.f;
        constexpr float DEFAULT_GROUNDED_NORMAL_THRESHOLD = 0.7f;
        constexpr float DEFAULT_BROAD_PHASE_CELL_SIZE = 128.f;
        constexpr std::uint32_t DEFAULT_BROAD_PHASE_MAX_CELLS_PER_PROXY = 256;
        constexpr float DEFAULT_CCD_MOTION_THRESHOLD = 0.5f;
        constexpr float DEFAULT_SLEEP_LINEAR_VELOCITY_THRESHOLD = 1.f;
        constexpr float DEFAULT_SLEEP_ANGULAR_VELOCITY_THRESHOLD = 0.05f;
        constexpr float DEFAULT_TIME_TO_SLEEP = 0.5f;
        constexpr double MINIMUM_GEOMETRY_EXTENT = 0.0001;
        constexpr double PI = 3.14159265358979323846;

        using PhysicsProxy2D = detail::PhysicsProxy2D;

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
            bool warmStarted = false;
        };

        struct JointConstraint2D
        {
            DistanceJoint2D* joint = nullptr;
            GameObject* firstObject = nullptr;
            GameObject* secondObject = nullptr;
            RigidBody2D* firstBody = nullptr;
            RigidBody2D* secondBody = nullptr;
            sf::Vector2f firstAnchor = {0.f, 0.f};
            sf::Vector2f secondAnchor = {0.f, 0.f};
            sf::Vector2f normal = {1.f, 0.f};
            double error = 0.0;
        };

        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        double dot(sf::Vector2f left, sf::Vector2f right)
        {
            return static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y;
        }

        double cross(sf::Vector2f left, sf::Vector2f right)
        {
            return static_cast<double>(left.x) * right.y - static_cast<double>(left.y) * right.x;
        }

        bool checkedFloat(double value, float& result)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(value) || std::fabs(value) > maximum) return false;

            result = static_cast<float>(value);
            return true;
        }

        bool checkedSolverFloat(double value, float& result)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());
            const double roundingTolerance =
                maximum * static_cast<double>(std::numeric_limits<float>::epsilon()) * 8.0;

            if (!std::isfinite(value) || value < -maximum - roundingTolerance ||
                value > maximum + roundingTolerance)
            {
                return false;
            }

            result = static_cast<float>(std::clamp(value, -maximum, maximum));
            return true;
        }

        bool scaledVector(sf::Vector2f direction, double scale, sf::Vector2f& result)
        {
            return checkedFloat(static_cast<double>(direction.x) * scale, result.x) &&
                   checkedFloat(static_cast<double>(direction.y) * scale, result.y);
        }

        bool moveObject(GameObject* object, sf::Vector2f direction, double distance)
        {
            if (object == nullptr || !isFinite(direction) || !std::isfinite(distance)) return false;

            sf::Vector2f offset;

            if (!scaledVector(direction, distance, offset)) return false;

            const sf::Vector2f position = object->transform.position();
            sf::Vector2f nextPosition;

            if (!checkedFloat(static_cast<double>(position.x) + offset.x, nextPosition.x) ||
                !checkedFloat(static_cast<double>(position.y) + offset.y, nextPosition.y))
            {
                return false;
            }

            object->transform.setPosition(nextPosition);
            return true;
        }

        sf::Vector2f transformPoint(const GameObject& object, sf::Vector2f localPoint)
        {
            const sf::Vector2f position = object.transform.position();
            const sf::Vector2f scale = object.transform.scale();
            const double radians = static_cast<double>(object.transform.rotation()) * PI / 180.0;
            const double cosine = std::cos(radians);
            const double sine = std::sin(radians);
            const double localX = static_cast<double>(localPoint.x) * scale.x;
            const double localY = static_cast<double>(localPoint.y) * scale.y;
            sf::Vector2f result;

            if (!checkedFloat(static_cast<double>(position.x) + localX * cosine - localY * sine,
                              result.x) ||
                !checkedFloat(static_cast<double>(position.y) + localX * sine + localY * cosine,
                              result.y))
            {
                const float invalid = std::numeric_limits<float>::quiet_NaN();
                return {invalid, invalid};
            }

            return result;
        }

        double inverseMass(const RigidBody2D* body)
        {
            if (body == nullptr || body->bodyType() != BodyType2D::Dynamic || !body->isAwake())
            {
                return 0.0;
            }

            return 1.0 / static_cast<double>(body->mass());
        }

        double inverseInertia(const RigidBody2D* body)
        {
            if (body == nullptr || body->bodyType() != BodyType2D::Dynamic || !body->isAwake())
            {
                return 0.0;
            }

            return static_cast<double>(body->inverseInertia());
        }

        sf::Vector2f effectiveVelocity(const RigidBody2D* body, sf::Vector2f point)
        {
            if (body == nullptr || body->bodyType() == BodyType2D::Static) return {0.f, 0.f};

            return body->velocityAtWorldPoint(point);
        }

        double relativeVelocityAlong(const RigidBody2D* first, const RigidBody2D* second,
                                     sf::Vector2f point, sf::Vector2f direction)
        {
            const sf::Vector2f firstVelocity = effectiveVelocity(first, point);
            const sf::Vector2f secondVelocity = effectiveVelocity(second, point);
            return dot(secondVelocity, direction) - dot(firstVelocity, direction);
        }

        double directionalInverseMass(const RigidBody2D* firstBody, const GameObject* firstObject,
                                      const RigidBody2D* secondBody, const GameObject* secondObject,
                                      sf::Vector2f point, sf::Vector2f direction)
        {
            double result = inverseMass(firstBody) + inverseMass(secondBody);

            if (firstObject != nullptr)
            {
                const sf::Vector2f lever = point - firstObject->transform.position();
                const double angularLeverage = cross(lever, direction);
                result += angularLeverage * angularLeverage * inverseInertia(firstBody);
            }

            if (secondObject != nullptr)
            {
                const sf::Vector2f lever = point - secondObject->transform.position();
                const double angularLeverage = cross(lever, direction);
                result += angularLeverage * angularLeverage * inverseInertia(secondBody);
            }

            return result;
        }

        bool bodyHasMotion(const RigidBody2D* body)
        {
            if (body == nullptr || body->bodyType() == BodyType2D::Static) return false;
            if (body->bodyType() == BodyType2D::Kinematic) return true;

            return body->isAwake();
        }

        void wakeForConstraint(RigidBody2D* first, RigidBody2D* second)
        {
            const bool firstMoves = bodyHasMotion(first);
            const bool secondMoves = bodyHasMotion(second);

            if (first != nullptr && first->bodyType() == BodyType2D::Dynamic && !first->isAwake() &&
                secondMoves)
            {
                first->wakeUp();
            }

            if (second != nullptr && second->bodyType() == BodyType2D::Dynamic &&
                !second->isAwake() && firstMoves)
            {
                second->wakeUp();
            }
        }

        float sanitizeUnitInterval(float value, float fallback)
        {
            return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : fallback;
        }

        float sanitizeNonNegative(float value, float fallback)
        {
            return std::isfinite(value) && value >= 0.f ? value : fallback;
        }

        PhysicsWorld2DConfig sanitizeConfig(PhysicsWorld2DConfig config)
        {
            if (!isFinite(config.gravity)) config.gravity = DEFAULT_GRAVITY;

            if (config.velocityIterations == 0)
                config.velocityIterations = DEFAULT_VELOCITY_ITERATIONS;
            if (config.positionIterations == 0)
                config.positionIterations = DEFAULT_POSITION_ITERATIONS;

            config.velocityIterations =
                std::min(config.velocityIterations, MAXIMUM_SOLVER_ITERATIONS);
            config.positionIterations =
                std::min(config.positionIterations, MAXIMUM_SOLVER_ITERATIONS);
            config.positionCorrectionPercent = sanitizeUnitInterval(
                config.positionCorrectionPercent, DEFAULT_POSITION_CORRECTION_PERCENT);
            config.penetrationSlop =
                sanitizeNonNegative(config.penetrationSlop, DEFAULT_PENETRATION_SLOP);
            config.restitutionVelocityThreshold = sanitizeNonNegative(
                config.restitutionVelocityThreshold, DEFAULT_RESTITUTION_VELOCITY_THRESHOLD);
            config.groundedNormalThreshold = sanitizeUnitInterval(
                config.groundedNormalThreshold, DEFAULT_GROUNDED_NORMAL_THRESHOLD);

            switch (config.broadPhaseMode)
            {
            case PhysicsBroadPhaseMode2D::UniformGrid:
            case PhysicsBroadPhaseMode2D::BruteForce:
                break;
            default:
                config.broadPhaseMode = PhysicsBroadPhaseMode2D::UniformGrid;
                break;
            }

            if (!std::isfinite(config.broadPhaseCellSize) || config.broadPhaseCellSize <= 0.f)
                config.broadPhaseCellSize = DEFAULT_BROAD_PHASE_CELL_SIZE;
            if (config.broadPhaseMaxCellsPerProxy == 0)
                config.broadPhaseMaxCellsPerProxy = DEFAULT_BROAD_PHASE_MAX_CELLS_PER_PROXY;

            if (config.maximumCcdSubsteps == 0)
                config.maximumCcdSubsteps = DEFAULT_MAXIMUM_CCD_SUBSTEPS;
            config.maximumCcdSubsteps = std::min(config.maximumCcdSubsteps, MAXIMUM_CCD_SUBSTEPS);
            config.ccdMotionThreshold =
                sanitizeUnitInterval(config.ccdMotionThreshold, DEFAULT_CCD_MOTION_THRESHOLD);
            if (config.ccdMotionThreshold <= 0.f)
                config.ccdMotionThreshold = DEFAULT_CCD_MOTION_THRESHOLD;

            config.sleepLinearVelocityThreshold = sanitizeNonNegative(
                config.sleepLinearVelocityThreshold, DEFAULT_SLEEP_LINEAR_VELOCITY_THRESHOLD);
            config.sleepAngularVelocityThreshold = sanitizeNonNegative(
                config.sleepAngularVelocityThreshold, DEFAULT_SLEEP_ANGULAR_VELOCITY_THRESHOLD);
            config.timeToSleep = sanitizeNonNegative(config.timeToSleep, DEFAULT_TIME_TO_SLEEP);

            return config;
        }

        bool contactConstraintLess(const ContactConstraint2D& left,
                                   const ContactConstraint2D& right)
        {
            return detail::physicsContactLess(left.contact, right.contact);
        }

        bool jointConstraintLess(const JointConstraint2D& left, const JointConstraint2D& right)
        {
            return left.joint != nullptr && right.joint != nullptr &&
                   left.joint->id() < right.joint->id();
        }

        bool sameContact(const PhysicsContact2D& left, const PhysicsContact2D& right)
        {
            return left.firstColliderId == right.firstColliderId &&
                   left.secondColliderId == right.secondColliderId;
        }

        void mergeFrameContact(std::vector<PhysicsContact2D>& contacts,
                               const PhysicsContact2D& contact)
        {
            const auto position = std::lower_bound(contacts.begin(), contacts.end(), contact,
                                                   detail::physicsContactLess);

            if (position != contacts.end() && sameContact(*position, contact))
            {
                *position = contact;
            }
            else
            {
                contacts.insert(position, contact);
            }
        }

        std::pair<GameObjectId, GameObjectId> objectPair(GameObjectId first, GameObjectId second)
        {
            if (second < first) std::swap(first, second);
            return {first, second};
        }

        double colliderMinimumExtent(const Collider2D& collider)
        {
            if (const auto* box = dynamic_cast<const BoxCollider2D*>(&collider))
            {
                const sf::Vector2f half = box->worldHalfExtents();
                if (!isFinite(half)) return std::numeric_limits<double>::infinity();
                return std::max(
                    std::min(static_cast<double>(half.x) * 2.0, static_cast<double>(half.y) * 2.0),
                    MINIMUM_GEOMETRY_EXTENT);
            }

            if (const auto* circle = dynamic_cast<const CircleCollider2D*>(&collider))
            {
                const double diameter = static_cast<double>(circle->worldRadius()) * 2.0;
                return std::isfinite(diameter) ? std::max(diameter, MINIMUM_GEOMETRY_EXTENT)
                                               : std::numeric_limits<double>::infinity();
            }

            return std::numeric_limits<double>::infinity();
        }
    }

    PhysicsWorld2D::PhysicsWorld2D() : PhysicsWorld2D(PhysicsWorld2DConfig{}) {}

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
        if (!m_config.warmStarting) m_contactImpulseCache.clear();
    }

    const PhysicsBroadPhaseStats2D& PhysicsWorld2D::broadPhaseStats() const
    {
        return m_broadPhaseStats;
    }

    const PhysicsStepStats2D& PhysicsWorld2D::stepStats() const
    {
        return m_stepStats;
    }

    const std::vector<PhysicsContact2D>& PhysicsWorld2D::contacts() const
    {
        return m_contacts;
    }

    const std::vector<PhysicsContactEvent2D>& PhysicsWorld2D::contactEvents() const
    {
        return m_contactEvents;
    }

    bool PhysicsWorld2D::isTouching(GameObjectId firstObjectId, GameObjectId secondObjectId) const
    {
        for (const PhysicsContact2D& contact : m_contacts)
        {
            if ((contact.firstObjectId == firstObjectId &&
                 contact.secondObjectId == secondObjectId) ||
                (contact.firstObjectId == secondObjectId &&
                 contact.secondObjectId == firstObjectId))
            {
                return true;
            }
        }

        return false;
    }

    bool PhysicsWorld2D::isColliderTouching(ColliderId firstColliderId,
                                            ColliderId secondColliderId) const
    {
        for (const PhysicsContact2D& contact : m_contacts)
        {
            if ((contact.firstColliderId == firstColliderId &&
                 contact.secondColliderId == secondColliderId) ||
                (contact.firstColliderId == secondColliderId &&
                 contact.secondColliderId == firstColliderId))
            {
                return true;
            }
        }

        return false;
    }

    void PhysicsWorld2D::reset()
    {
        m_broadPhaseStats = PhysicsBroadPhaseStats2D{};
        m_stepStats = PhysicsStepStats2D{};
        m_contacts.clear();
        m_contactEvents.clear();
        m_contactImpulseCache.clear();
        m_contactSceneToken.reset();
    }

    void PhysicsWorld2D::reset(Scene& scene)
    {
        resetPhysicsStates(scene);
        reset();
    }

    void PhysicsWorld2D::step(Scene& scene, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.f) return;

        const std::shared_ptr<void> sceneToken = scene.m_handleState;

        if (m_contactSceneToken.lock() != sceneToken)
        {
            reset();
            m_contactSceneToken = sceneToken;
        }

        const std::vector<PhysicsContact2D> previousContacts = m_contacts;
        std::vector<PhysicsContact2D> frameContacts;

        m_broadPhaseStats = PhysicsBroadPhaseStats2D{};
        m_stepStats = PhysicsStepStats2D{};
        m_contacts.clear();
        m_contactEvents.clear();
        resetPhysicsStates(scene);

        const std::uint32_t substepCount = calculateCcdSubsteps(scene, deltaTime);
        const float substepDelta = deltaTime / static_cast<float>(substepCount);
        m_stepStats.ccdSubstepCount = substepCount;

        for (std::uint32_t substep = 0; substep < substepCount; ++substep)
        {
            integrateRigidBodies(scene, substepDelta);
            solveSubstep(scene, substepDelta, frameContacts);
        }

        for (const std::unique_ptr<GameObject>& gameObject : scene.gameObjects())
        {
            if (gameObject == nullptr) continue;
            if (RigidBody2D* body = gameObject->getComponent<RigidBody2D>()) body->clearForces();
        }

        m_contacts = std::move(frameContacts);
        updateSleeping(scene, deltaTime);
        detail::buildPhysicsContactEvents(previousContacts, m_contacts, m_contactEvents);
    }

    std::uint32_t PhysicsWorld2D::calculateCcdSubsteps(Scene& scene, float deltaTime) const
    {
        if (!m_config.continuousCollisionDetection || m_config.maximumCcdSubsteps <= 1)
        {
            return 1;
        }

        double minimumExtent = std::numeric_limits<double>::infinity();
        double maximumTranslation = 0.0;

        for (const std::unique_ptr<GameObject>& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();
            if (!isPhysicsParticipant(scene, gameObject)) continue;

            for (Collider2D* collider : gameObject->getComponents<Collider2D>())
            {
                if (collider != nullptr && collider->isActive())
                    minimumExtent = std::min(minimumExtent, colliderMinimumExtent(*collider));
            }

            const RigidBody2D* body = gameObject->getComponent<RigidBody2D>();
            if (body == nullptr || !body->isActive() || !bodyHasMotion(body)) continue;

            const sf::Vector2f velocity = body->velocity();
            if (!isFinite(velocity)) continue;
            maximumTranslation =
                std::max(maximumTranslation,
                         std::hypot(static_cast<double>(velocity.x), velocity.y) * deltaTime);
        }

        if (!std::isfinite(minimumExtent) || maximumTranslation <= 0.0) return 1;

        const double allowedMotion =
            std::max(minimumExtent * static_cast<double>(m_config.ccdMotionThreshold),
                     MINIMUM_GEOMETRY_EXTENT);
        const double required = std::ceil(maximumTranslation / allowedMotion);

        if (!std::isfinite(required) || required <= 1.0) return 1;

        return static_cast<std::uint32_t>(
            std::min(required, static_cast<double>(m_config.maximumCcdSubsteps)));
    }

    void PhysicsWorld2D::solveSubstep(Scene& scene, float deltaTime,
                                      std::vector<PhysicsContact2D>& frameContacts)
    {
        BroadPhaseStepData2D stepData = buildBroadPhaseStepData(scene);
        m_broadPhaseStats.proxyCount =
            std::max(m_broadPhaseStats.proxyCount, stepData.stats.proxyCount);
        m_broadPhaseStats.occupiedCellCount =
            std::max(m_broadPhaseStats.occupiedCellCount, stepData.stats.occupiedCellCount);
        m_broadPhaseStats.fallbackProxyCount =
            std::max(m_broadPhaseStats.fallbackProxyCount, stepData.stats.fallbackProxyCount);
        m_broadPhaseStats.bruteForcePairCount =
            std::max(m_broadPhaseStats.bruteForcePairCount, stepData.stats.bruteForcePairCount);
        m_broadPhaseStats.candidatePairCount += stepData.stats.candidatePairCount;

        std::vector<PhysicsProxy2D>& proxies = stepData.proxies;
        std::vector<ContactConstraint2D> constraints;
        std::vector<JointConstraint2D> joints;
        std::set<std::pair<GameObjectId, GameObjectId>> collisionExclusions;

        for (const std::unique_ptr<GameObject>& firstObjectPtr : scene.gameObjects())
        {
            GameObject* firstObject = firstObjectPtr.get();
            if (!isPhysicsParticipant(scene, firstObject)) continue;

            for (DistanceJoint2D* joint : firstObject->getComponents<DistanceJoint2D>())
            {
                if (joint == nullptr || !joint->isActive() ||
                    joint->connectedObjectId() == InvalidGameObjectId)
                {
                    continue;
                }

                GameObject* secondObject = scene.findGameObjectById(joint->connectedObjectId());
                if (!isPhysicsParticipant(scene, secondObject) || secondObject == firstObject)
                {
                    continue;
                }

                RigidBody2D* firstBody = firstObject->getComponent<RigidBody2D>();
                RigidBody2D* secondBody = secondObject->getComponent<RigidBody2D>();
                if (firstBody != nullptr && !firstBody->isActive()) firstBody = nullptr;
                if (secondBody != nullptr && !secondBody->isActive()) secondBody = nullptr;

                if (!joint->collideConnected())
                {
                    collisionExclusions.insert(objectPair(firstObject->id(), secondObject->id()));
                }

                const sf::Vector2f firstAnchor = transformPoint(*firstObject, joint->localAnchor());
                const sf::Vector2f secondAnchor =
                    transformPoint(*secondObject, joint->connectedLocalAnchor());
                if (!isFinite(firstAnchor) || !isFinite(secondAnchor)) continue;

                const sf::Vector2f delta = secondAnchor - firstAnchor;
                const double length = std::hypot(static_cast<double>(delta.x), delta.y);
                if (!std::isfinite(length)) continue;

                const sf::Vector2f normal = length > 0.0
                                                ? sf::Vector2f{static_cast<float>(delta.x / length),
                                                               static_cast<float>(delta.y / length)}
                                                : sf::Vector2f{1.f, 0.f};
                const double error = length - joint->restLength();

                if (std::fabs(error) > m_config.penetrationSlop)
                {
                    if (firstBody != nullptr && firstBody->bodyType() == BodyType2D::Dynamic)
                        firstBody->wakeUp();
                    if (secondBody != nullptr && secondBody->bodyType() == BodyType2D::Dynamic)
                        secondBody->wakeUp();
                }

                joints.push_back({joint, firstObject, secondObject, firstBody, secondBody,
                                  firstAnchor, secondAnchor, normal, error});
            }
        }

        auto addPair =
            [this, &constraints, &collisionExclusions](PhysicsProxy2D& left, PhysicsProxy2D& right)
        {
            PhysicsProxy2D* first = &left;
            PhysicsProxy2D* second = &right;

            if (first->object == nullptr || second->object == nullptr ||
                first->collider == nullptr || second->collider == nullptr ||
                first->object == second->object)
            {
                return;
            }

            if (std::make_tuple(second->object->id(), second->collider->id()) <
                std::make_tuple(first->object->id(), first->collider->id()))
            {
                std::swap(first, second);
            }

            if (collisionExclusions.count(objectPair(first->object->id(), second->object->id())) !=
                    0 ||
                !first->collider->canCollideWith(*second->collider))
            {
                return;
            }

            ++m_broadPhaseStats.narrowPhaseTestCount;
            CollisionManifold2D manifold;

            if (!computeCollisionManifold(*first->collider, *second->collider, manifold) ||
                !isFinite(manifold.normal) || !isFinite(manifold.point) ||
                !std::isfinite(manifold.penetration) || manifold.penetration < 0.f)
            {
                return;
            }

            PhysicsContact2D contact;
            contact.firstObjectId = first->object->id();
            contact.secondObjectId = second->object->id();
            contact.firstColliderId = first->collider->id();
            contact.secondColliderId = second->collider->id();
            contact.firstColliderType = first->collider->type();
            contact.secondColliderType = second->collider->type();
            contact.manifold = manifold;
            contact.sensor = first->collider->isSensor() || second->collider->isSensor();
            first->collider->setColliding(true);
            second->collider->setColliding(true);

            if (contact.sensor)
            {
                constraints.push_back({first, second, contact});
                return;
            }

            wakeForConstraint(first->body, second->body);
            const PhysicsMaterial2D& firstMaterial = first->collider->material();
            const PhysicsMaterial2D& secondMaterial = second->collider->material();
            const double initialNormalVelocity =
                relativeVelocityAlong(first->body, second->body, manifold.point, manifold.normal);
            double restitutionBias = 0.0;

            if (initialNormalVelocity < -m_config.restitutionVelocityThreshold)
            {
                restitutionBias = -static_cast<double>(std::max(firstMaterial.restitution,
                                                                secondMaterial.restitution)) *
                                  initialNormalVelocity;
            }

            ContactConstraint2D constraint{
                first,
                second,
                contact,
                restitutionBias,
                std::sqrt(static_cast<double>(firstMaterial.staticFriction) *
                          secondMaterial.staticFriction),
                std::sqrt(static_cast<double>(firstMaterial.dynamicFriction) *
                          secondMaterial.dynamicFriction)};

            if (m_config.warmStarting)
            {
                const ContactImpulseKey2D key{contact.firstColliderId, contact.secondColliderId};
                const auto cached = m_contactImpulseCache.find(key);

                if (cached != m_contactImpulseCache.end())
                {
                    constraint.accumulatedNormalImpulse = cached->second.normal;
                    constraint.accumulatedTangentImpulse = cached->second.tangent;
                    constraint.warmStarted = true;
                }
            }

            constraints.push_back(constraint);
        };

        for (const detail::BroadPhasePair2D& pair : stepData.broadPhaseResult.pairs)
        {
            addPair(proxies[pair.first], proxies[pair.second]);
        }

        std::sort(constraints.begin(), constraints.end(), contactConstraintLess);
        std::sort(joints.begin(), joints.end(), jointConstraintLess);
        m_stepStats.contactConstraintCount =
            std::max(m_stepStats.contactConstraintCount, constraints.size());
        m_stepStats.jointConstraintCount =
            std::max(m_stepStats.jointConstraintCount, joints.size());

        auto applyDirectionalImpulse =
            [](RigidBody2D* body, sf::Vector2f direction, double magnitude, sf::Vector2f point)
        {
            if (body == nullptr || body->bodyType() != BodyType2D::Dynamic || !body->isAwake() ||
                !isFinite(direction) || !std::isfinite(magnitude))
            {
                return;
            }

            sf::Vector2f nextVelocity;
            if (!checkedSolverFloat(static_cast<double>(body->m_velocity.x) +
                                        static_cast<double>(direction.x) * magnitude / body->m_mass,
                                    nextVelocity.x) ||
                !checkedSolverFloat(static_cast<double>(body->m_velocity.y) +
                                        static_cast<double>(direction.y) * magnitude / body->m_mass,
                                    nextVelocity.y))
            {
                return;
            }

            float nextAngularVelocity = body->m_angularVelocity;
            if (body->owner() != nullptr && !body->m_fixedRotation)
            {
                const sf::Vector2f lever = point - body->owner()->transform.position();
                const double angularImpulse = cross(lever, direction) * magnitude;
                if (!checkedSolverFloat(static_cast<double>(body->m_angularVelocity) +
                                            angularImpulse / body->m_inertia,
                                        nextAngularVelocity))
                {
                    return;
                }
            }

            body->m_velocity = nextVelocity;
            body->m_angularVelocity = nextAngularVelocity;
        };

        auto applyPairImpulse = [&applyDirectionalImpulse](ContactConstraint2D& constraint,
                                                           sf::Vector2f direction, double magnitude)
        {
            applyDirectionalImpulse(constraint.first->body, direction, -magnitude,
                                    constraint.contact.manifold.point);
            applyDirectionalImpulse(constraint.second->body, direction, magnitude,
                                    constraint.contact.manifold.point);
        };

        if (m_config.warmStarting)
        {
            for (ContactConstraint2D& constraint : constraints)
            {
                if (!constraint.warmStarted || constraint.contact.sensor) continue;
                applyPairImpulse(constraint, constraint.contact.manifold.normal,
                                 constraint.accumulatedNormalImpulse);
                const sf::Vector2f tangent{-constraint.contact.manifold.normal.y,
                                           constraint.contact.manifold.normal.x};
                applyPairImpulse(constraint, tangent, constraint.accumulatedTangentImpulse);
                ++m_stepStats.warmStartedContactCount;
            }
        }

        auto solveContactVelocity = [&applyPairImpulse](ContactConstraint2D& constraint)
        {
            if (constraint.contact.sensor) return;

            const sf::Vector2f point = constraint.contact.manifold.point;
            const sf::Vector2f normal = constraint.contact.manifold.normal;
            const double normalMass = directionalInverseMass(
                constraint.first->body, constraint.first->object, constraint.second->body,
                constraint.second->object, point, normal);
            if (normalMass <= 0.0 || !std::isfinite(normalMass)) return;

            const double normalVelocity = relativeVelocityAlong(
                constraint.first->body, constraint.second->body, point, normal);
            const double normalDelta = (constraint.restitutionBias - normalVelocity) / normalMass;
            if (!std::isfinite(normalDelta)) return;

            const double previousNormal = constraint.accumulatedNormalImpulse;
            constraint.accumulatedNormalImpulse = std::max(previousNormal + normalDelta, 0.0);
            applyPairImpulse(constraint, normal,
                             constraint.accumulatedNormalImpulse - previousNormal);

            const sf::Vector2f tangent{-normal.y, normal.x};
            const double tangentMass = directionalInverseMass(
                constraint.first->body, constraint.first->object, constraint.second->body,
                constraint.second->object, point, tangent);
            if (tangentMass <= 0.0 || !std::isfinite(tangentMass)) return;

            const double tangentVelocity = relativeVelocityAlong(
                constraint.first->body, constraint.second->body, point, tangent);
            const double tangentDelta = -tangentVelocity / tangentMass;
            const double previousTangent = constraint.accumulatedTangentImpulse;
            const double candidate = previousTangent + tangentDelta;
            if (!std::isfinite(candidate)) return;

            const double staticLimit =
                constraint.staticFriction * constraint.accumulatedNormalImpulse;
            if (std::fabs(candidate) <= staticLimit)
            {
                constraint.accumulatedTangentImpulse = candidate;
            }
            else
            {
                const double dynamicLimit =
                    constraint.dynamicFriction * constraint.accumulatedNormalImpulse;
                constraint.accumulatedTangentImpulse =
                    std::clamp(candidate, -dynamicLimit, dynamicLimit);
            }

            applyPairImpulse(constraint, tangent,
                             constraint.accumulatedTangentImpulse - previousTangent);
        };

        auto solveJointVelocity =
            [&applyDirectionalImpulse, deltaTime](JointConstraint2D& constraint)
        {
            if (constraint.joint == nullptr) return;
            const sf::Vector2f point = (constraint.firstAnchor + constraint.secondAnchor) * 0.5f;
            const double effectiveMass = directionalInverseMass(
                constraint.firstBody, constraint.firstObject, constraint.secondBody,
                constraint.secondObject, point, constraint.normal);
            if (effectiveMass <= 0.0 || !std::isfinite(effectiveMass)) return;

            const double relativeVelocity = relativeVelocityAlong(
                constraint.firstBody, constraint.secondBody, point, constraint.normal);
            const double bias = static_cast<double>(constraint.joint->stiffness()) *
                                constraint.error / std::max(static_cast<double>(deltaTime), 1e-9);
            const double impulseMagnitude =
                -(relativeVelocity * (1.0 + constraint.joint->damping()) + bias) / effectiveMass;
            applyDirectionalImpulse(constraint.firstBody, constraint.normal, -impulseMagnitude,
                                    constraint.firstAnchor);
            applyDirectionalImpulse(constraint.secondBody, constraint.normal, impulseMagnitude,
                                    constraint.secondAnchor);
        };

        for (std::uint32_t iteration = 0; iteration < m_config.velocityIterations; ++iteration)
        {
            for (ContactConstraint2D& constraint : constraints)
                solveContactVelocity(constraint);
            for (JointConstraint2D& joint : joints)
                solveJointVelocity(joint);
        }

        auto solveContactPosition = [this](ContactConstraint2D& constraint)
        {
            if (constraint.contact.sensor) return false;
            CollisionManifold2D manifold;
            if (!computeCollisionManifold(*constraint.first->collider, *constraint.second->collider,
                                          manifold))
            {
                return false;
            }

            const double firstInverseMass = inverseMass(constraint.first->body);
            const double secondInverseMass = inverseMass(constraint.second->body);
            const double inverseMassSum = firstInverseMass + secondInverseMass;
            if (inverseMassSum <= 0.0 || !std::isfinite(inverseMassSum)) return false;

            const double penetration =
                std::max(static_cast<double>(manifold.penetration) - m_config.penetrationSlop, 0.0);
            if (penetration <= 0.0) return false;
            const double correction = m_config.positionCorrectionPercent * penetration;
            bool moved = false;

            if (firstInverseMass > 0.0)
                moved = moveObject(constraint.first->object, manifold.normal,
                                   -correction * firstInverseMass / inverseMassSum) ||
                        moved;
            if (secondInverseMass > 0.0)
                moved = moveObject(constraint.second->object, manifold.normal,
                                   correction * secondInverseMass / inverseMassSum) ||
                        moved;
            return moved;
        };

        auto solveJointPosition = [](JointConstraint2D& constraint)
        {
            if (constraint.joint == nullptr || constraint.firstObject == nullptr ||
                constraint.secondObject == nullptr)
            {
                return false;
            }

            const sf::Vector2f firstAnchor =
                transformPoint(*constraint.firstObject, constraint.joint->localAnchor());
            const sf::Vector2f secondAnchor =
                transformPoint(*constraint.secondObject, constraint.joint->connectedLocalAnchor());
            if (!isFinite(firstAnchor) || !isFinite(secondAnchor)) return false;

            const sf::Vector2f delta = secondAnchor - firstAnchor;
            const double length = std::hypot(static_cast<double>(delta.x), delta.y);
            if (!std::isfinite(length) || length <= 0.0) return false;
            const sf::Vector2f normal{static_cast<float>(delta.x / length),
                                      static_cast<float>(delta.y / length)};
            const double error = length - constraint.joint->restLength();
            const double firstInverseMass = inverseMass(constraint.firstBody);
            const double secondInverseMass = inverseMass(constraint.secondBody);
            const double inverseMassSum = firstInverseMass + secondInverseMass;
            if (inverseMassSum <= 0.0 || std::fabs(error) <= MINIMUM_GEOMETRY_EXTENT) return false;

            const double correction = static_cast<double>(constraint.joint->stiffness()) * error;
            bool moved = false;
            if (firstInverseMass > 0.0)
                moved = moveObject(constraint.firstObject, normal,
                                   correction * firstInverseMass / inverseMassSum) ||
                        moved;
            if (secondInverseMass > 0.0)
                moved = moveObject(constraint.secondObject, normal,
                                   -correction * secondInverseMass / inverseMassSum) ||
                        moved;
            return moved;
        };

        for (std::uint32_t iteration = 0; iteration < m_config.positionIterations; ++iteration)
        {
            bool correctedAny = false;
            for (ContactConstraint2D& constraint : constraints)
                correctedAny = solveContactPosition(constraint) || correctedAny;
            for (JointConstraint2D& joint : joints)
                correctedAny = solveJointPosition(joint) || correctedAny;
            if (!correctedAny) break;
        }

        std::map<ContactImpulseKey2D, CachedContactImpulse2D> nextCache;

        for (const ContactConstraint2D& constraint : constraints)
        {
            mergeFrameContact(frameContacts, constraint.contact);
            const sf::Vector2f normal = constraint.contact.manifold.normal;

            if (!constraint.contact.sensor)
            {
                if (constraint.first->body != nullptr &&
                    constraint.first->body->bodyType() == BodyType2D::Dynamic &&
                    normal.y >= m_config.groundedNormalThreshold)
                {
                    constraint.first->body->setGrounded(true);
                }

                if (constraint.second->body != nullptr &&
                    constraint.second->body->bodyType() == BodyType2D::Dynamic &&
                    normal.y <= -m_config.groundedNormalThreshold)
                {
                    constraint.second->body->setGrounded(true);
                }

                nextCache[{constraint.contact.firstColliderId,
                           constraint.contact.secondColliderId}] = {
                    constraint.accumulatedNormalImpulse, constraint.accumulatedTangentImpulse};
            }
        }

        if (m_config.warmStarting)
            m_contactImpulseCache = std::move(nextCache);
        else
            m_contactImpulseCache.clear();
    }

    void PhysicsWorld2D::updateSleeping(Scene& scene, float deltaTime)
    {
        for (const std::unique_ptr<GameObject>& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();
            if (!isPhysicsParticipant(scene, gameObject)) continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();
            if (body == nullptr || !body->isActive()) continue;

            if (body->bodyType() != BodyType2D::Dynamic)
            {
                ++m_stepStats.activeBodyCount;
                continue;
            }

            if (!m_config.sleeping || !body->allowsSleep())
            {
                body->wakeUp();
            }
            else if (body->isAwake())
            {
                const sf::Vector2f velocity = body->velocity();
                const double speed = std::hypot(static_cast<double>(velocity.x), velocity.y);
                const double angularSpeed = std::fabs(static_cast<double>(body->angularVelocity()));

                if (speed < m_config.sleepLinearVelocityThreshold &&
                    angularSpeed < m_config.sleepAngularVelocityThreshold)
                {
                    body->setSleepTimer(body->sleepTimer() + deltaTime);
                    if (body->sleepTimer() >= m_config.timeToSleep) body->sleep();
                }
                else
                {
                    body->setSleepTimer(0.f);
                }
            }

            if (body->isAwake())
                ++m_stepStats.activeBodyCount;
            else
                ++m_stepStats.sleepingBodyCount;
        }
    }

    bool PhysicsWorld2D::isPhysicsParticipant(const Scene& scene,
                                              const GameObject* gameObject) const
    {
        return gameObject != nullptr && gameObject->isActive() && !gameObject->isDestroyQueued() &&
               scene.isFixedStepParticipant(*gameObject);
    }

    void PhysicsWorld2D::resetPhysicsStates(Scene& scene)
    {
        for (const std::unique_ptr<GameObject>& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();
            if (gameObject == nullptr) continue;

            for (Collider2D* collider : gameObject->getComponents<Collider2D>())
            {
                if (collider != nullptr) collider->setColliding(false);
            }

            if (RigidBody2D* body = gameObject->getComponent<RigidBody2D>())
                body->setGrounded(false);
        }
    }

    void PhysicsWorld2D::integrateRigidBodies(Scene& scene, float deltaTime)
    {
        for (const std::unique_ptr<GameObject>& gameObjectPtr : scene.gameObjects())
        {
            GameObject* gameObject = gameObjectPtr.get();
            if (!isPhysicsParticipant(scene, gameObject)) continue;

            RigidBody2D* body = gameObject->getComponent<RigidBody2D>();
            if (body != nullptr && body->isActive()) body->integrate(deltaTime, m_config.gravity);
        }
    }
}

#pragma once

#include <Lorenzo2D/Physics/PhysicsContact2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace l2d
{
    class Scene;
    class GameObject;

    enum class PhysicsBroadPhaseMode2D
    {
        UniformGrid,
        BruteForce
    };

    struct PhysicsWorld2DConfig
    {
        sf::Vector2f gravity = {0.f, 980.f};

        // Zero selects the default. Larger values are clamped to 64 so
        // untrusted configuration cannot stall a simulation step.
        std::uint32_t velocityIterations = 8;
        std::uint32_t positionIterations = 3;

        float positionCorrectionPercent = 0.8f;
        float penetrationSlop = 0.01f;
        float restitutionVelocityThreshold = 1.f;
        float groundedNormalThreshold = 0.7f;

        PhysicsBroadPhaseMode2D broadPhaseMode = PhysicsBroadPhaseMode2D::UniformGrid;
        float broadPhaseCellSize = 128.f;
        std::uint32_t broadPhaseMaxCellsPerProxy = 256;

        bool continuousCollisionDetection = true;
        std::uint32_t maximumCcdSubsteps = 32;
        float ccdMotionThreshold = 0.5f;

        bool warmStarting = true;

        bool sleeping = true;
        float sleepLinearVelocityThreshold = 1.f;
        float sleepAngularVelocityThreshold = 0.05f;
        float timeToSleep = 0.5f;
    };

    struct PhysicsStepStats2D
    {
        std::uint32_t ccdSubstepCount = 0;
        std::size_t activeBodyCount = 0;
        std::size_t sleepingBodyCount = 0;
        std::size_t contactConstraintCount = 0;
        std::size_t jointConstraintCount = 0;
        std::size_t warmStartedContactCount = 0;
    };

    struct PhysicsBroadPhaseStats2D
    {
        std::size_t proxyCount = 0;
        std::size_t occupiedCellCount = 0;
        std::size_t fallbackProxyCount = 0;
        std::size_t bruteForcePairCount = 0;
        std::size_t candidatePairCount = 0;
        std::size_t narrowPhaseTestCount = 0;
    };

    class PhysicsWorld2D
    {
      public:
        PhysicsWorld2D();
        explicit PhysicsWorld2D(const PhysicsWorld2DConfig& config);

        PhysicsWorld2D(const PhysicsWorld2D&) = delete;
        PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;
        PhysicsWorld2D(PhysicsWorld2D&&) = delete;
        PhysicsWorld2D& operator=(PhysicsWorld2D&&) = delete;

        const PhysicsWorld2DConfig& config() const;
        void setConfig(const PhysicsWorld2DConfig& config);

        const PhysicsBroadPhaseStats2D& broadPhaseStats() const;
        const PhysicsStepStats2D& stepStats() const;

        const std::vector<PhysicsContact2D>& contacts() const;
        const std::vector<PhysicsContactEvent2D>& contactEvents() const;

        bool isTouching(GameObjectId firstObjectId, GameObjectId secondObjectId) const;
        bool isColliderTouching(ColliderId firstColliderId, ColliderId secondColliderId) const;

        void reset();
        void reset(Scene& scene);
        void step(Scene& scene, float deltaTime);

      private:
        struct BroadPhaseStepData2D;

        BroadPhaseStepData2D buildBroadPhaseStepData(Scene& scene) const;

        bool isPhysicsParticipant(const Scene& scene, const GameObject* gameObject) const;

        void resetPhysicsStates(Scene& scene);
        void integrateRigidBodies(Scene& scene, float deltaTime);
        std::uint32_t calculateCcdSubsteps(Scene& scene, float deltaTime) const;
        void solveSubstep(Scene& scene, float deltaTime,
                          std::vector<PhysicsContact2D>& frameContacts);
        void updateSleeping(Scene& scene, float deltaTime);

        struct CachedContactImpulse2D
        {
            double normal = 0.0;
            double tangent = 0.0;
        };

        using ContactImpulseKey2D = std::pair<ColliderId, ColliderId>;

      private:
        PhysicsWorld2DConfig m_config;
        PhysicsBroadPhaseStats2D m_broadPhaseStats;
        PhysicsStepStats2D m_stepStats;

        std::vector<PhysicsContact2D> m_contacts;
        std::vector<PhysicsContactEvent2D> m_contactEvents;

        std::map<ContactImpulseKey2D, CachedContactImpulse2D> m_contactImpulseCache;

        std::weak_ptr<void> m_contactSceneToken;
    };
}

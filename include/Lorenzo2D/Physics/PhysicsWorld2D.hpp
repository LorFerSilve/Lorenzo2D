#pragma once

#include <Lorenzo2D/Physics/PhysicsContact2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace l2d
{
    class Scene;
    class GameObject;

    struct PhysicsWorld2DConfig
    {
        sf::Vector2f gravity = { 0.f, 980.f };

        std::uint32_t velocityIterations = 8;
        std::uint32_t positionIterations = 3;

        float positionCorrectionPercent = 0.8f;
        float penetrationSlop = 0.01f;
        float restitutionVelocityThreshold = 1.f;
        float groundedNormalThreshold = 0.7f;
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

        const std::vector<PhysicsContact2D>& contacts() const;
        const std::vector<PhysicsContactEvent2D>& contactEvents() const;

        bool isTouching(
            GameObjectId firstObjectId,
            GameObjectId secondObjectId
        ) const;

        void reset();
        void reset(Scene& scene);
        void step(Scene& scene, float deltaTime);

    private:
        bool isPhysicsParticipant(
            const Scene& scene,
            const GameObject* gameObject
        ) const;

        void resetPhysicsStates(Scene& scene);
        void integrateRigidBodies(Scene& scene, float deltaTime);

    private:
        PhysicsWorld2DConfig m_config;

        std::vector<PhysicsContact2D> m_contacts;
        std::vector<PhysicsContactEvent2D> m_contactEvents;

        std::weak_ptr<void> m_contactSceneToken;
    };
}

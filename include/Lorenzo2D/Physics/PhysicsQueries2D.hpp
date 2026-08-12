#pragma once

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace l2d
{
    class Scene;

    struct PhysicsQueryFilter2D
    {
        std::uint32_t categoryMask = std::numeric_limits<std::uint32_t>::max();
        bool includeSensors = false;
        GameObjectId ignoredObject = InvalidGameObjectId;
    };

    struct PhysicsQueryHit2D
    {
        GameObjectHandle object;
        ColliderId colliderId = InvalidColliderId;
        sf::Vector2f point = {0.f, 0.f};
        sf::Vector2f normal = {0.f, 0.f};
        float distance = 0.f;
        float fraction = 0.f;
        float penetration = 0.f;
        bool sensor = false;
    };

    // An immutable snapshot of active scene colliders. Build one per fixed tick
    // and reuse it for batches of gameplay, AI, and navigation queries.
    class PhysicsQueryContext2D
    {
      public:
        explicit PhysicsQueryContext2D(Scene& scene);

        std::size_t proxyCount() const;

        std::optional<PhysicsQueryHit2D> raycast(sf::Vector2f start, sf::Vector2f end,
                                                 const PhysicsQueryFilter2D& filter = {}) const;
        std::vector<PhysicsQueryHit2D> raycastAll(sf::Vector2f start, sf::Vector2f end,
                                                  const PhysicsQueryFilter2D& filter = {}) const;
        std::vector<PhysicsQueryHit2D> pointQuery(sf::Vector2f point,
                                                  const PhysicsQueryFilter2D& filter = {}) const;
        std::vector<PhysicsQueryHit2D> overlapCircle(sf::Vector2f center, float radius,
                                                     const PhysicsQueryFilter2D& filter = {}) const;
        std::vector<PhysicsQueryHit2D> overlapBox(sf::Vector2f center, sf::Vector2f size,
                                                  float rotationDegrees = 0.f,
                                                  const PhysicsQueryFilter2D& filter = {}) const;
        std::vector<PhysicsQueryHit2D> overlapCapsule(
            sf::Vector2f center, float radius, float height, float rotationDegrees = 0.f,
            const PhysicsQueryFilter2D& filter = {}) const;
        std::optional<PhysicsQueryHit2D> castCircle(sf::Vector2f start, sf::Vector2f end,
                                                    float radius,
                                                    const PhysicsQueryFilter2D& filter = {}) const;
        std::optional<PhysicsQueryHit2D> castBox(sf::Vector2f start, sf::Vector2f end,
                                                 sf::Vector2f size, float rotationDegrees = 0.f,
                                                 const PhysicsQueryFilter2D& filter = {}) const;
        std::optional<PhysicsQueryHit2D> castCapsule(sf::Vector2f start, sf::Vector2f end,
                                                     float radius, float height,
                                                     float rotationDegrees = 0.f,
                                                     const PhysicsQueryFilter2D& filter = {}) const;

      private:
        struct Impl;
        std::shared_ptr<const Impl> m_impl;
    };
}

#pragma once

#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsMaterial2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace l2d
{
    class GameObject;
    class Scene;

    struct RectangleRendererPrefab
    {
        sf::Vector2f size = {100.f, 100.f};
        sf::Color color = sf::Color::White;
    };

    struct CircleRendererPrefab
    {
        float radius = 50.f;
        sf::Color color = sf::Color::White;
    };

    struct RigidBodyPrefab
    {
        BodyType2D bodyType = BodyType2D::Dynamic;
        sf::Vector2f velocity = {0.f, 0.f};
        sf::Vector2f acceleration = {0.f, 0.f};
        float mass = 1.f;
        bool useGravity = false;
        float gravityScale = 1.f;
    };

    struct ColliderPrefabProperties
    {
        sf::Vector2f offset = {0.f, 0.f};
        PhysicsMaterial2D material;
        CollisionFilter2D filter;
        bool sensor = false;
    };

    struct BoxColliderPrefab
    {
        sf::Vector2f size = {100.f, 100.f};
        ColliderPrefabProperties properties;
    };

    struct CircleColliderPrefab
    {
        float radius = 50.f;
        ColliderPrefabProperties properties;
    };

    // A data-only object template. Custom gameplay components can be attached
    // after instantiation without coupling serialization to game code.
    struct Prefab
    {
        std::string name = "GameObject";
        std::string tag;
        TransformState transform;
        bool active = true;

        std::optional<RectangleRendererPrefab> rectangleRenderer;
        std::optional<CircleRendererPrefab> circleRenderer;
        std::optional<RigidBodyPrefab> rigidBody;
        std::optional<BoxColliderPrefab> boxCollider;
        std::optional<CircleColliderPrefab> circleCollider;
    };

    bool isValidPrefab(const Prefab& prefab);
    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab);

    class PrefabLibrary
    {
      public:
        // Stores a validated snapshot under a non-empty key. Existing entries
        // are replaced only after validation succeeds.
        bool store(std::string key, Prefab prefab);
        bool remove(const std::string& key);
        void clear();

        bool contains(const std::string& key) const;
        const Prefab* find(const std::string& key) const;
        std::size_t size() const;

        GameObject* instantiate(Scene& scene, const std::string& key) const;

      private:
        std::unordered_map<std::string, Prefab> m_prefabs;
    };
}

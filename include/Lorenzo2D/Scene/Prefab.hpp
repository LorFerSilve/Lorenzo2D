#pragma once

#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Assets/AssetId.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Navigation/PathFollower2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsMaterial2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace l2d
{
    class AssetManager;
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

    struct RenderOrderPrefab
    {
        std::int32_t layer = 0;
        float depth = 0.f;
        std::int32_t order = 0;
        std::uint8_t depthMode = 0u;
    };

    struct SpriteRendererPrefab
    {
        AssetId texture;
        sf::IntRect textureRect;
        sf::Vector2f size = {0.f, 0.f};
        sf::Color color = sf::Color::White;
        sf::Vector2f origin = {0.f, 0.f};
        bool flipX = false;
        bool flipY = false;
        RenderOrderPrefab renderOrder;
    };

    struct AnimatorPrefab
    {
        std::vector<AssetId> clips;
        AssetId initialClip;
        float playbackSpeed = 1.f;
        bool playing = true;
    };

    struct SerializedComponentPrefab
    {
        std::string type;
        std::uint32_t version = 1u;
        bool required = true;
        std::string data;
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

    struct CharacterMotorPrefab
    {
        CharacterMotorConfig2D config;
    };

    struct TopDownControllerPrefab
    {
        TopDownControllerConfig2D config;
    };

    struct GridStepControllerPrefab
    {
        GridStepControllerConfig2D config;
    };

    struct PlatformerControllerPrefab
    {
        PlatformerControllerConfig2D config;
    };

    struct PathFollowerPrefab
    {
        PathFollowerConfig2D config;
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

    struct CapsuleColliderPrefab
    {
        float radius = 25.f;
        float height = 100.f;
        ColliderPrefabProperties properties;
    };

    struct ConvexPolygonColliderPrefab
    {
        std::vector<sf::Vector2f> vertices = {{-50.f, 50.f}, {0.f, -50.f}, {50.f, 50.f}};
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
        std::int32_t zOrder = 0;

        std::optional<RectangleRendererPrefab> rectangleRenderer;
        std::optional<CircleRendererPrefab> circleRenderer;
        std::optional<SpriteRendererPrefab> spriteRenderer;
        std::optional<AnimatorPrefab> animator;
        std::optional<RigidBodyPrefab> rigidBody;
        std::optional<CharacterMotorPrefab> characterMotor;
        std::optional<TopDownControllerPrefab> topDownController;
        std::optional<GridStepControllerPrefab> gridStepController;
        std::optional<PlatformerControllerPrefab> platformerController;
        std::optional<PathFollowerPrefab> pathFollower;
        std::optional<BoxColliderPrefab> boxCollider;
        std::optional<CircleColliderPrefab> circleCollider;
        std::optional<CapsuleColliderPrefab> capsuleCollider;
        std::optional<ConvexPolygonColliderPrefab> convexPolygonCollider;
        std::vector<SerializedComponentPrefab> customComponents;
    };

    bool isValidPrefab(const Prefab& prefab);
    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab);
    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab, AssetManager& assets);

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

#pragma once

#include "Lorenzo2D/ECS/GameObject.hpp"
#include "Lorenzo2D/Scene/GameObjectHandle.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace l2d
{
    class LevelSerializer;
    class PhysicsWorld2D;
    struct RenderContext2D;
    class SceneManager;
    class TileMap;

    class Scene
    {
      public:
        explicit Scene(std::string name = "Scene");
        ~Scene();

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        const std::string& name() const;
        const std::string& getName() const;

        // References and raw pointers returned by Scene are borrowed and are
        // invalidated when their object is destroyed or the Scene is cleared.
        // Use GameObjectHandle when identity must survive deferred work.
        GameObject& createGameObject(const std::string& name = "GameObject");

        GameObjectHandle createHandle(GameObject& gameObject);
        GameObjectHandle createHandle(GameObjectId id);

        GameObject* findGameObjectById(GameObjectId id);
        const GameObject* findGameObjectById(GameObjectId id) const;

        GameObject* findGameObjectByName(const std::string& name);
        const GameObject* findGameObjectByName(const std::string& name) const;

        std::vector<GameObject*> findGameObjectsByTag(const std::string& tag);
        std::vector<const GameObject*> findGameObjectsByTag(const std::string& tag) const;

        std::vector<GameObject*> findActiveGameObjectsByTag(const std::string& tag);
        std::vector<const GameObject*> findActiveGameObjectsByTag(const std::string& tag) const;

        std::size_t countGameObjectsByTag(const std::string& tag) const;
        std::size_t countActiveGameObjectsByTag(const std::string& tag) const;

        void destroyGameObject(GameObject& gameObject);
        void destroyQueuedGameObjects();
        std::size_t destroyQueuedGameObjectCount() const;

        std::size_t gameObjectCount() const;
        std::size_t activeGameObjectCount() const;

        const std::vector<std::unique_ptr<GameObject>>& gameObjects() const;

        void fixedUpdate(float deltaTime);
        void update(float deltaTime);
        void render(sf::RenderWindow& window);
        void render(sf::RenderWindow& window, float interpolationAlpha);
        void render(sf::RenderWindow& window, const RenderContext2D& context);

        void clear();

      private:
        GameObject* findOwnedGameObjectById(GameObjectId id);
        const GameObject* findOwnedGameObjectById(GameObjectId id) const;

        void beginDispatch();
        void endDispatch();
        void destroyQueuedGameObjectsImmediately();
        void rollbackGameObjectsFrom(std::size_t firstIndex);
        void advanceFixedUpdateGeneration();
        bool isFixedStepParticipant(const GameObject& gameObject) const;

      private:
        std::string m_name;
        std::vector<std::unique_ptr<GameObject>> m_gameObjects;
        std::unordered_map<GameObjectId, GameObject*> m_gameObjectsById;

        std::size_t m_dispatchDepth = 0;
        bool m_destroySweepDeferred = false;
        bool m_clearDeferred = false;
        bool m_fixedUpdateInProgress = false;
        std::uint64_t m_fixedUpdateGeneration = 0;

        std::shared_ptr<detail::SceneHandleState> m_handleState;
        SceneManager* m_ownerManager = nullptr;

        friend class LevelSerializer;
        friend class PhysicsWorld2D;
        friend class SceneManager;
        friend class TileMap;
    };
}

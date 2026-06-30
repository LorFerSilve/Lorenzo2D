#pragma once

#include "Lorenzo2D/ECS/GameObject.hpp"
#include "Lorenzo2D/Scene/GameObjectHandle.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace l2d
{
    class Scene
    {
    public:
        explicit Scene(std::string name = "Scene");

        const std::string& name() const;
        const std::string& getName() const;

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

        std::vector<std::unique_ptr<GameObject>>& gameObjects();
        const std::vector<std::unique_ptr<GameObject>>& gameObjects() const;

        void update(float deltaTime);
        void render(sf::RenderWindow& window);

        void clear();

    private:
        std::string m_name;
        std::vector<std::unique_ptr<GameObject>> m_gameObjects;
    };
}
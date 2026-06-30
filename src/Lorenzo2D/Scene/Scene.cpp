#include "Lorenzo2D/Scene/Scene.hpp"

#include <algorithm>
#include <utility>

namespace l2d
{
    Scene::Scene(std::string name)
        : m_name(std::move(name))
    {
    }

    const std::string& Scene::name() const
    {
        return m_name;
    }

    const std::string& Scene::getName() const
    {
        return m_name;
    }

    GameObject& Scene::createGameObject(const std::string& name)
    {
        std::unique_ptr<GameObject> gameObject =
            std::make_unique<GameObject>(name);

        GameObject* rawGameObject = gameObject.get();

        m_gameObjects.push_back(std::move(gameObject));

        return *rawGameObject;
    }

    GameObjectHandle Scene::createHandle(GameObject& gameObject)
    {
        for (const std::unique_ptr<GameObject>& currentGameObject : m_gameObjects)
        {
            if (currentGameObject.get() == &gameObject)
            {
                if (currentGameObject->isDestroyQueued())
                    return GameObjectHandle();

                return GameObjectHandle(this, currentGameObject->id());
            }
        }

        return GameObjectHandle();
    }

    GameObjectHandle Scene::createHandle(GameObjectId id)
    {
        GameObject* gameObject = findGameObjectById(id);

        if (gameObject == nullptr)
            return GameObjectHandle();

        if (gameObject->isDestroyQueued())
            return GameObjectHandle();

        return GameObjectHandle(this, id);
    }

    GameObject* Scene::findGameObjectById(GameObjectId id)
    {
        if (id == InvalidGameObjectId)
            return nullptr;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->id() == id)
                return gameObject.get();
        }

        return nullptr;
    }

    const GameObject* Scene::findGameObjectById(GameObjectId id) const
    {
        if (id == InvalidGameObjectId)
            return nullptr;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->id() == id)
                return gameObject.get();
        }

        return nullptr;
    }

    GameObject* Scene::findGameObjectByName(const std::string& name)
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->name() == name)
                return gameObject.get();
        }

        return nullptr;
    }

    const GameObject* Scene::findGameObjectByName(const std::string& name) const
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->name() == name)
                return gameObject.get();
        }

        return nullptr;
    }

    std::vector<GameObject*> Scene::findGameObjectsByTag(const std::string& tag)
    {
        std::vector<GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<const GameObject*> Scene::findGameObjectsByTag(
        const std::string& tag
    ) const
    {
        std::vector<const GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<GameObject*> Scene::findActiveGameObjectsByTag(
        const std::string& tag
    )
    {
        std::vector<GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<const GameObject*> Scene::findActiveGameObjectsByTag(
        const std::string& tag
    ) const
    {
        std::vector<const GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::size_t Scene::countGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->hasTag(tag))
                count++;
        }

        return count;
    }

    std::size_t Scene::countActiveGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive() && gameObject->hasTag(tag))
                count++;
        }

        return count;
    }

    void Scene::destroyGameObject(GameObject& gameObject)
    {
        for (const std::unique_ptr<GameObject>& currentGameObject : m_gameObjects)
        {
            if (currentGameObject.get() == &gameObject)
            {
                currentGameObject->destroy();
                return;
            }
        }
    }

    void Scene::destroyQueuedGameObjects()
    {
        std::vector<std::unique_ptr<GameObject>>::iterator newEnd =
            std::remove_if(
                m_gameObjects.begin(),
                m_gameObjects.end(),
                [](const std::unique_ptr<GameObject>& gameObject)
                {
                    return gameObject->isDestroyQueued();
                }
            );

        m_gameObjects.erase(newEnd, m_gameObjects.end());
    }

    std::size_t Scene::destroyQueuedGameObjectCount() const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isDestroyQueued())
                count++;
        }

        return count;
    }

    std::size_t Scene::gameObjectCount() const
    {
        return m_gameObjects.size();
    }

    std::size_t Scene::activeGameObjectCount() const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive())
                count++;
        }

        return count;
    }

    std::vector<std::unique_ptr<GameObject>>& Scene::gameObjects()
    {
        return m_gameObjects;
    }

    const std::vector<std::unique_ptr<GameObject>>& Scene::gameObjects() const
    {
        return m_gameObjects;
    }

    void Scene::update(float deltaTime)
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive() && !gameObject->isDestroyQueued())
                gameObject->update(deltaTime);
        }
    }

    void Scene::render(sf::RenderWindow& window)
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isActive() && !gameObject->isDestroyQueued())
                gameObject->render(window);
        }
    }

    void Scene::clear()
    {
        m_gameObjects.clear();
    }
}
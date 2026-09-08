#include "Lorenzo2D/Scene/Scene.hpp"

#include "Lorenzo2D/Scene/SceneManager.hpp"

#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace l2d
{
    Scene::Scene(std::string name)
        : m_name(std::move(name)), m_handleState(std::make_shared<detail::SceneHandleState>())
    {
        m_handleState->scene = this;
    }

    Scene::~Scene()
    {
        m_handleState->scene = nullptr;
        m_gameObjectsById.clear();
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
        std::unique_ptr<GameObject> gameObject = std::make_unique<GameObject>(name);

        GameObject* rawGameObject = gameObject.get();

        if (rawGameObject->id() == InvalidGameObjectId)
        {
            throw std::overflow_error("Game object IDs are exhausted.");
        }

        const auto insertion = m_gameObjectsById.emplace(rawGameObject->id(), rawGameObject);

        if (!insertion.second)
        {
            throw std::logic_error("Duplicate game object ID.");
        }

        try
        {
            m_gameObjects.push_back(std::move(gameObject));
        }
        catch (...)
        {
            m_gameObjectsById.erase(insertion.first);
            throw;
        }

        return *rawGameObject;
    }

    GameObjectHandle Scene::createHandle(GameObject& gameObject)
    {
        GameObject* ownedGameObject = findOwnedGameObjectById(gameObject.id());

        if (ownedGameObject != &gameObject) return GameObjectHandle();

        if (ownedGameObject->isDestroyQueued()) return GameObjectHandle();

        return GameObjectHandle(m_handleState, ownedGameObject->id());
    }

    GameObjectHandle Scene::createHandle(GameObjectId id)
    {
        GameObject* gameObject = findGameObjectById(id);

        if (gameObject == nullptr) return GameObjectHandle();

        if (gameObject->isDestroyQueued()) return GameObjectHandle();

        return GameObjectHandle(m_handleState, id);
    }

    GameObject* Scene::findGameObjectById(GameObjectId id)
    {
        GameObject* gameObject = findOwnedGameObjectById(id);

        if (gameObject == nullptr || gameObject->isDestroyQueued()) return nullptr;

        return gameObject;
    }

    const GameObject* Scene::findGameObjectById(GameObjectId id) const
    {
        const GameObject* gameObject = findOwnedGameObjectById(id);

        if (gameObject == nullptr || gameObject->isDestroyQueued()) return nullptr;

        return gameObject;
    }

    GameObject* Scene::findGameObjectByName(const std::string& name)
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->name() == name)
                return gameObject.get();
        }

        return nullptr;
    }

    const GameObject* Scene::findGameObjectByName(const std::string& name) const
    {
        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->name() == name)
                return gameObject.get();
        }

        return nullptr;
    }

    std::vector<GameObject*> Scene::findGameObjectsByTag(const std::string& tag)
    {
        std::vector<GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<const GameObject*> Scene::findGameObjectsByTag(const std::string& tag) const
    {
        std::vector<const GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<GameObject*> Scene::findActiveGameObjectsByTag(const std::string& tag)
    {
        std::vector<GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->isActive() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::vector<const GameObject*> Scene::findActiveGameObjectsByTag(const std::string& tag) const
    {
        std::vector<const GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->isActive() && gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::size_t Scene::countGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->hasTag(tag)) count++;
        }

        return count;
    }

    std::size_t Scene::countActiveGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->isActive() && gameObject->hasTag(tag))
                count++;
        }

        return count;
    }

    void Scene::destroyGameObject(GameObject& gameObject)
    {
        GameObject* ownedGameObject = findOwnedGameObjectById(gameObject.id());

        if (ownedGameObject == &gameObject) ownedGameObject->destroy();
    }

    void Scene::destroyQueuedGameObjects()
    {
        if (m_dispatchDepth > 0)
        {
            m_destroySweepDeferred = true;
            return;
        }

        destroyQueuedGameObjectsImmediately();
    }

    void Scene::destroyQueuedGameObjectsImmediately()
    {
        std::vector<std::unique_ptr<GameObject>>::iterator newEnd =
            std::remove_if(m_gameObjects.begin(), m_gameObjects.end(),
                           [this](const std::unique_ptr<GameObject>& gameObject)
                           {
                               if (!gameObject->isDestroyQueued()) return false;

                               m_gameObjectsById.erase(gameObject->id());
                               return true;
                           });

        m_gameObjects.erase(newEnd, m_gameObjects.end());
    }

    void Scene::rollbackGameObjectsFrom(std::size_t firstIndex)
    {
        if (firstIndex >= m_gameObjects.size()) return;

        if (m_dispatchDepth > 0u)
        {
            for (std::size_t index = firstIndex; index < m_gameObjects.size(); ++index)
                if (m_gameObjects[index] != nullptr) m_gameObjects[index]->destroy();

            m_destroySweepDeferred = true;
            return;
        }

        for (std::size_t index = firstIndex; index < m_gameObjects.size(); ++index)
            if (m_gameObjects[index] != nullptr) m_gameObjectsById.erase(m_gameObjects[index]->id());

        m_gameObjects.erase(m_gameObjects.begin() + static_cast<std::ptrdiff_t>(firstIndex),
                            m_gameObjects.end());
    }

    std::size_t Scene::destroyQueuedGameObjectCount() const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (gameObject->isDestroyQueued()) count++;
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
            if (!gameObject->isDestroyQueued() && gameObject->isActive()) count++;
        }

        return count;
    }

    const std::vector<std::unique_ptr<GameObject>>& Scene::gameObjects() const
    {
        return m_gameObjects;
    }

    void Scene::fixedUpdate(float deltaTime)
    {
        if (m_fixedUpdateInProgress) return;

        m_fixedUpdateInProgress = true;
        beginDispatch();

        try
        {
            advanceFixedUpdateGeneration();

            const std::size_t gameObjectCount = m_gameObjects.size();

            for (std::size_t index = 0; index < gameObjectCount; ++index)
            {
                GameObject* gameObject = m_gameObjects[index].get();

                if (gameObject != nullptr && gameObject->isActive() &&
                    !gameObject->isDestroyQueued())
                {
                    gameObject->m_fixedUpdateGeneration = m_fixedUpdateGeneration;
                    gameObject->transform.capturePrevious();
                }
            }

            for (std::size_t index = 0; index < gameObjectCount; ++index)
            {
                if (m_clearDeferred) break;

                GameObject* gameObject = m_gameObjects[index].get();

                if (gameObject != nullptr && gameObject->isActive() &&
                    !gameObject->isDestroyQueued() && isFixedStepParticipant(*gameObject))
                {
                    gameObject->update(deltaTime);
                }
            }
        }
        catch (...)
        {
            m_fixedUpdateInProgress = false;
            endDispatch();
            throw;
        }

        m_fixedUpdateInProgress = false;
        endDispatch();
    }

    void Scene::advanceFixedUpdateGeneration()
    {
        if (m_fixedUpdateGeneration == std::numeric_limits<std::uint64_t>::max())
        {
            for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
            {
                if (gameObject != nullptr) gameObject->m_fixedUpdateGeneration = 0;
            }

            m_fixedUpdateGeneration = 1;
            return;
        }

        ++m_fixedUpdateGeneration;
    }

    bool Scene::isFixedStepParticipant(const GameObject& gameObject) const
    {
        return m_fixedUpdateGeneration == 0 ||
               gameObject.m_fixedUpdateGeneration == m_fixedUpdateGeneration;
    }

    void Scene::update(float deltaTime)
    {
        fixedUpdate(deltaTime);
    }

    void Scene::render(sf::RenderWindow& window)
    {
        render(window, 1.f);
    }

    void Scene::render(sf::RenderWindow& window, float interpolationAlpha)
    {
        render(window, RenderContext2D{interpolationAlpha});
    }

    void Scene::render(sf::RenderWindow& window, const RenderContext2D& context)
    {
        beginDispatch();

        try
        {
            RenderQueue2D queue;
            queue.build(*this, context);

            for (const RenderQueueEntry2D& entry : queue.entries())
            {
                if (m_clearDeferred) break;

                if (GameObject* gameObject = entry.gameObject.get())
                {
                    gameObject->render(window, context);
                }
            }
        }
        catch (...)
        {
            endDispatch();
            throw;
        }

        endDispatch();
    }

    void Scene::clear()
    {
        if (m_dispatchDepth > 0)
        {
            for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
            {
                if (gameObject != nullptr) gameObject->destroy();
            }

            m_clearDeferred = true;
            return;
        }

        m_clearDeferred = false;
        m_destroySweepDeferred = false;
        m_gameObjectsById.clear();
        m_gameObjects.clear();
    }

    GameObject* Scene::findOwnedGameObjectById(GameObjectId id)
    {
        if (id == InvalidGameObjectId) return nullptr;

        const auto iterator = m_gameObjectsById.find(id);

        if (iterator == m_gameObjectsById.end()) return nullptr;

        return iterator->second;
    }

    const GameObject* Scene::findOwnedGameObjectById(GameObjectId id) const
    {
        if (id == InvalidGameObjectId) return nullptr;

        const auto iterator = m_gameObjectsById.find(id);

        if (iterator == m_gameObjectsById.end()) return nullptr;

        return iterator->second;
    }

    void Scene::beginDispatch()
    {
        if (m_ownerManager != nullptr) m_ownerManager->beginDispatch();

        m_dispatchDepth++;
    }

    void Scene::endDispatch()
    {
        m_dispatchDepth--;

        if (m_dispatchDepth > 0)
        {
            if (m_ownerManager != nullptr) m_ownerManager->endDispatch();

            return;
        }

        if (m_clearDeferred)
        {
            m_clearDeferred = false;
            m_destroySweepDeferred = false;
            m_gameObjectsById.clear();
            m_gameObjects.clear();
        }
        else if (m_destroySweepDeferred)
        {
            m_destroySweepDeferred = false;
            destroyQueuedGameObjectsImmediately();
        }

        SceneManager* ownerManager = m_ownerManager;

        if (ownerManager != nullptr) ownerManager->endDispatch();
    }
}

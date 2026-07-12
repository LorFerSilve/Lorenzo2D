#include "Lorenzo2D/Scene/Scene.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace l2d
{
    Scene::Scene(std::string name)
        : m_name(std::move(name)),
        m_handleState(std::make_shared<detail::SceneHandleState>())
    {
        m_handleState->scene = this;
    }

    Scene::~Scene()
    {
        m_handleState->scene = nullptr;
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

                return GameObjectHandle(m_handleState, currentGameObject->id());
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

        return GameObjectHandle(m_handleState, id);
    }

    GameObject* Scene::findGameObjectById(GameObjectId id)
    {
        if (id == InvalidGameObjectId)
            return nullptr;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->id() == id)
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
            if (!gameObject->isDestroyQueued() && gameObject->id() == id)
                return gameObject.get();
        }

        return nullptr;
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

    std::vector<const GameObject*> Scene::findGameObjectsByTag(
        const std::string& tag
    ) const
    {
        std::vector<const GameObject*> result;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->hasTag(tag))
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
            if (!gameObject->isDestroyQueued() &&
                gameObject->isActive() &&
                gameObject->hasTag(tag))
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
            if (!gameObject->isDestroyQueued() &&
                gameObject->isActive() &&
                gameObject->hasTag(tag))
                result.push_back(gameObject.get());
        }

        return result;
    }

    std::size_t Scene::countGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() && gameObject->hasTag(tag))
                count++;
        }

        return count;
    }

    std::size_t Scene::countActiveGameObjectsByTag(const std::string& tag) const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
        {
            if (!gameObject->isDestroyQueued() &&
                gameObject->isActive() &&
                gameObject->hasTag(tag))
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
            if (!gameObject->isDestroyQueued() && gameObject->isActive())
                count++;
        }

        return count;
    }

    const std::vector<std::unique_ptr<GameObject>>& Scene::gameObjects() const
    {
        return m_gameObjects;
    }

    void Scene::fixedUpdate(float deltaTime)
    {
        beginDispatch();

        try
        {
            advanceFixedUpdateGeneration();

            const std::size_t gameObjectCount = m_gameObjects.size();

            for (std::size_t index = 0; index < gameObjectCount; ++index)
            {
                GameObject* gameObject = m_gameObjects[index].get();

                if (gameObject != nullptr && !gameObject->isDestroyQueued())
                {
                    gameObject->m_fixedUpdateGeneration =
                        m_fixedUpdateGeneration;
                    gameObject->transform.capturePrevious();
                }
            }

            for (std::size_t index = 0; index < gameObjectCount; ++index)
            {
                if (m_clearDeferred)
                    break;

                GameObject* gameObject = m_gameObjects[index].get();

                if (gameObject != nullptr &&
                    gameObject->isActive() &&
                    !gameObject->isDestroyQueued())
                {
                    gameObject->update(deltaTime);
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

    void Scene::advanceFixedUpdateGeneration()
    {
        if (
            m_fixedUpdateGeneration ==
            std::numeric_limits<std::uint64_t>::max()
        )
        {
            for (const std::unique_ptr<GameObject>& gameObject : m_gameObjects)
            {
                if (gameObject != nullptr)
                    gameObject->m_fixedUpdateGeneration = 0;
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

    void Scene::render(
        sf::RenderWindow& window,
        float interpolationAlpha
    )
    {
        beginDispatch();

        try
        {
            const std::size_t gameObjectCount = m_gameObjects.size();

            for (std::size_t index = 0; index < gameObjectCount; ++index)
            {
                if (m_clearDeferred)
                    break;

                GameObject* gameObject = m_gameObjects[index].get();

                if (gameObject != nullptr &&
                    gameObject->isActive() &&
                    !gameObject->isDestroyQueued())
                {
                    gameObject->render(window, interpolationAlpha);
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
                if (gameObject != nullptr)
                    gameObject->destroy();
            }

            m_clearDeferred = true;
            return;
        }

        m_clearDeferred = false;
        m_destroySweepDeferred = false;
        m_gameObjects.clear();
    }

    void Scene::beginDispatch()
    {
        m_dispatchDepth++;
    }

    void Scene::endDispatch()
    {
        m_dispatchDepth--;

        if (m_dispatchDepth > 0)
            return;

        if (m_clearDeferred)
        {
            m_clearDeferred = false;
            m_destroySweepDeferred = false;
            m_gameObjects.clear();
            return;
        }

        if (m_destroySweepDeferred)
        {
            m_destroySweepDeferred = false;
            destroyQueuedGameObjectsImmediately();
        }
    }
}

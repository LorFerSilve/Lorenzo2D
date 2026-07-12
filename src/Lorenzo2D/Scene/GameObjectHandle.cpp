#include "Lorenzo2D/Scene/GameObjectHandle.hpp"

#include "Lorenzo2D/Scene/Scene.hpp"

namespace l2d
{
    GameObjectHandle::GameObjectHandle()
    {
    }

    GameObjectHandle::GameObjectHandle(
        const std::shared_ptr<detail::SceneHandleState>& sceneState,
        GameObjectId id
    )
        : m_sceneState(sceneState),
        m_id(id)
    {
    }

    GameObjectId GameObjectHandle::id() const
    {
        return m_id;
    }

    GameObjectId GameObjectHandle::getId() const
    {
        return m_id;
    }

    Scene* GameObjectHandle::scene() const
    {
        const std::shared_ptr<detail::SceneHandleState> sceneState =
            m_sceneState.lock();

        if (sceneState == nullptr)
            return nullptr;

        return sceneState->scene;
    }

    GameObject* GameObjectHandle::get() const
    {
        Scene* currentScene = scene();

        if (currentScene == nullptr)
            return nullptr;

        if (m_id == InvalidGameObjectId)
            return nullptr;

        GameObject* gameObject = currentScene->findGameObjectById(m_id);

        if (gameObject == nullptr)
            return nullptr;

        if (gameObject->isDestroyQueued())
            return nullptr;

        return gameObject;
    }

    bool GameObjectHandle::isValid() const
    {
        return get() != nullptr;
    }

    void GameObjectHandle::reset()
    {
        m_sceneState.reset();
        m_id = InvalidGameObjectId;
    }
}

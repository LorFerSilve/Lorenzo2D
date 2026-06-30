#include "Lorenzo2D/Scene/GameObjectHandle.hpp"

#include "Lorenzo2D/Scene/Scene.hpp"

namespace l2d
{
    GameObjectHandle::GameObjectHandle()
    {
    }

    GameObjectHandle::GameObjectHandle(Scene* scene, GameObjectId id)
        : m_scene(scene),
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
        return m_scene;
    }

    GameObject* GameObjectHandle::get() const
    {
        if (m_scene == nullptr)
            return nullptr;

        if (m_id == InvalidGameObjectId)
            return nullptr;

        GameObject* gameObject = m_scene->findGameObjectById(m_id);

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
        m_scene = nullptr;
        m_id = InvalidGameObjectId;
    }
}
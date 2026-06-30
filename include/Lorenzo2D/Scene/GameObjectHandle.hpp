#pragma once

#include "Lorenzo2D/ECS/GameObject.hpp"

namespace l2d
{
    class Scene;

    class GameObjectHandle
    {
    public:
        GameObjectHandle();
        GameObjectHandle(Scene* scene, GameObjectId id);

        GameObjectId id() const;
        GameObjectId getId() const;

        Scene* scene() const;

        GameObject* get() const;
        bool isValid() const;

        void reset();

    private:
        Scene* m_scene = nullptr;
        GameObjectId m_id = InvalidGameObjectId;
    };
}
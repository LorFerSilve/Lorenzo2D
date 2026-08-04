#pragma once

#include "Lorenzo2D/ECS/GameObject.hpp"

#include <memory>

namespace l2d
{
    class Scene;

    namespace detail
    {
        struct SceneHandleState
        {
            Scene* scene = nullptr;
        };
    }

    class GameObjectHandle
    {
      public:
        GameObjectHandle();

        GameObjectId id() const;
        GameObjectId getId() const;

        Scene* scene() const;

        GameObject* get() const;
        bool isValid() const;

        void reset();

      private:
        GameObjectHandle(const std::shared_ptr<detail::SceneHandleState>& sceneState,
                         GameObjectId id);

      private:
        std::weak_ptr<detail::SceneHandleState> m_sceneState;
        GameObjectId m_id = InvalidGameObjectId;

        friend class Scene;
    };
}

#pragma once

#include "Lorenzo2D/ECS/Component.hpp"
#include "Lorenzo2D/ECS/Transform.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace l2d
{
    class Scene;

    using GameObjectId = std::uint64_t;

    constexpr GameObjectId InvalidGameObjectId = 0;

    class GameObject
    {
      public:
        explicit GameObject(std::string name = "GameObject");

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = delete;
        GameObject& operator=(GameObject&&) = delete;

        GameObjectId id() const;
        GameObjectId getId() const;

        const std::string& name() const;
        const std::string& getName() const;
        void setName(std::string name);

        const std::string& tag() const;
        const std::string& getTag() const;
        void setTag(std::string tag);
        bool hasTag(const std::string& tag) const;

        bool isActive() const;
        void setActive(bool active);

        void destroy();
        bool isDestroyQueued() const;

        template <typename T, typename... Args> T& addComponent(Args&&... args)
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from l2d::Component.");

            std::unique_ptr<T> component = std::make_unique<T>(std::forward<Args>(args)...);

            component->setOwner(this);

            T* rawComponent = component.get();
            m_components.push_back(std::move(component));

            return *rawComponent;
        }

        template <typename T> T* getComponent()
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from l2d::Component.");

            for (const std::unique_ptr<Component>& component : m_components)
            {
                T* casted = dynamic_cast<T*>(component.get());

                if (casted != nullptr) return casted;
            }

            return nullptr;
        }

        template <typename T> const T* getComponent() const
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from l2d::Component.");

            for (const std::unique_ptr<Component>& component : m_components)
            {
                const T* casted = dynamic_cast<const T*>(component.get());

                if (casted != nullptr) return casted;
            }

            return nullptr;
        }

        template <typename T> bool hasComponent() const
        {
            return getComponent<T>() != nullptr;
        }

        template <typename T> std::vector<T*> getComponents()
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from l2d::Component.");

            std::vector<T*> matchingComponents;

            for (const std::unique_ptr<Component>& component : m_components)
            {
                if (T* casted = dynamic_cast<T*>(component.get()))
                {
                    matchingComponents.push_back(casted);
                }
            }

            return matchingComponents;
        }

        template <typename T> std::vector<const T*> getComponents() const
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from l2d::Component.");

            std::vector<const T*> matchingComponents;

            for (const std::unique_ptr<Component>& component : m_components)
            {
                if (const T* casted = dynamic_cast<const T*>(component.get()))
                {
                    matchingComponents.push_back(casted);
                }
            }

            return matchingComponents;
        }

        void update(float deltaTime);
        void render(sf::RenderWindow& window);
        void render(sf::RenderWindow& window, float interpolationAlpha);

        Transform transform;

      private:
        GameObjectId m_id = InvalidGameObjectId;

        std::string m_name;
        std::string m_tag;

        bool m_active = true;
        bool m_destroyQueued = false;
        std::uint64_t m_fixedUpdateGeneration = 0;

        std::vector<std::unique_ptr<Component>> m_components;

        friend class Scene;
    };
}

#include "Lorenzo2D/ECS/GameObject.hpp"

#include <cstddef>
#include <utility>

namespace
{
    l2d::GameObjectId g_nextGameObjectId = 1;

    l2d::GameObjectId allocateGameObjectId()
    {
        const l2d::GameObjectId id = g_nextGameObjectId;
        g_nextGameObjectId++;

        return id;
    }
}

namespace l2d
{
    GameObject::GameObject(std::string name) : m_id(allocateGameObjectId()), m_name(std::move(name))
    {
    }

    GameObjectId GameObject::id() const
    {
        return m_id;
    }

    GameObjectId GameObject::getId() const
    {
        return m_id;
    }

    const std::string& GameObject::name() const
    {
        return m_name;
    }

    const std::string& GameObject::getName() const
    {
        return m_name;
    }

    void GameObject::setName(std::string name)
    {
        m_name = std::move(name);
    }

    const std::string& GameObject::tag() const
    {
        return m_tag;
    }

    const std::string& GameObject::getTag() const
    {
        return m_tag;
    }

    void GameObject::setTag(std::string tag)
    {
        m_tag = std::move(tag);
    }

    bool GameObject::hasTag(const std::string& tag) const
    {
        return m_tag == tag;
    }

    bool GameObject::isActive() const
    {
        return m_active;
    }

    void GameObject::setActive(bool active)
    {
        m_active = active;
    }

    std::int32_t GameObject::zOrder() const
    {
        return m_zOrder;
    }

    void GameObject::setZOrder(std::int32_t zOrder)
    {
        m_zOrder = zOrder;
    }

    void GameObject::destroy()
    {
        m_destroyQueued = true;
        m_active = false;
    }

    bool GameObject::isDestroyQueued() const
    {
        return m_destroyQueued;
    }

    void GameObject::update(float deltaTime)
    {
        if (!m_active || m_destroyQueued) return;

        const std::size_t componentCount = m_components.size();

        for (std::size_t index = 0; index < componentCount; ++index)
        {
            if (!m_active || m_destroyQueued) return;

            Component* component = m_components[index].get();

            if (component->isActive()) component->onUpdate(deltaTime);
        }
    }

    void GameObject::render(sf::RenderWindow& window)
    {
        render(window, 1.f);
    }

    void GameObject::render(sf::RenderWindow& window, float interpolationAlpha)
    {
        if (!m_active || m_destroyQueued) return;

        const std::size_t componentCount = m_components.size();

        for (std::size_t index = 0; index < componentCount; ++index)
        {
            if (!m_active || m_destroyQueued) return;

            Component* component = m_components[index].get();

            if (component->isActive()) component->onRender(window, interpolationAlpha);
        }
    }
}

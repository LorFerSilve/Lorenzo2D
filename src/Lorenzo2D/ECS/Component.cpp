#include <Lorenzo2D/ECS/Component.hpp>

namespace l2d
{
    Component::Component() : m_owner(nullptr), m_active(true) {}

    bool Component::isActive() const
    {
        return m_active;
    }

    void Component::setActive(bool active)
    {
        m_active = active;
    }

    GameObject* Component::owner()
    {
        return m_owner;
    }

    const GameObject* Component::owner() const
    {
        return m_owner;
    }

    void Component::onUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    void Component::onRender(sf::RenderWindow& window)
    {
        (void)window;
    }

    void Component::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        (void)interpolationAlpha;
        onRender(window);
    }

    void Component::setOwner(GameObject* owner)
    {
        m_owner = owner;
    }
}

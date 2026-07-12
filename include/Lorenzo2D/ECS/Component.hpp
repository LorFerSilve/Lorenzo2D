#pragma once

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class GameObject;

    class Component
    {
    public:
        Component();
        virtual ~Component() = default;

        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;
        Component(Component&&) = delete;
        Component& operator=(Component&&) = delete;

        bool isActive() const;
        void setActive(bool active);

        GameObject* owner();
        const GameObject* owner() const;

        virtual void onUpdate(float deltaTime);
        virtual void onRender(sf::RenderWindow& window);
        virtual void onRender(
            sf::RenderWindow& window,
            float interpolationAlpha
        );

    private:
        void setOwner(GameObject* owner);

    private:
        GameObject* m_owner;
        bool m_active;

        friend class GameObject;
    };
}

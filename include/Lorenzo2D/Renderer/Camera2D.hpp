#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class Camera2D
    {
    public:
        explicit Camera2D(sf::Vector2f size = { 1280.f, 720.f });

        void setCenter(sf::Vector2f center);
        const sf::Vector2f& center() const;

        void move(sf::Vector2f offset); 

        void setSize(sf::Vector2f size);
        const sf::Vector2f& size() const;

        void setZoom(float zoom);
        float zoom() const;

        void setFollowSmoothness(float smoothness);
        float followSmoothness() const;

        void follow(sf::Vector2f target, float deltaTime);

        void setBounds(sf::Vector2f min, sf::Vector2f max);
        void clearBounds();

        bool hasBounds() const;
        const sf::Vector2f& boundsMin() const;
        const sf::Vector2f& boundsMax() const;

        void applyTo(sf::RenderWindow& window) const;

        const sf::View& view() const;
    private:
        void updateViewSize();
        sf::Vector2f clampedCenter(sf::Vector2f center) const;

    private:
        sf::View m_view;

        sf::Vector2f m_center;
        sf::Vector2f m_baseSize;

        float m_zoom;
        float m_followSmoothness;

        bool m_hasBounds;
        sf::Vector2f m_boundsMin;
        sf::Vector2f m_boundsMax;
    };
}
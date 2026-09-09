#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace sf
{
    class RenderTarget;
    class RenderWindow;
}

namespace l2d
{
    class Camera2D
    {
      public:
        explicit Camera2D(sf::Vector2f size = {1280.f, 720.f});

        // Nonfinite or out-of-domain coordinates are rejected. Checked
        // variants report rejection; legacy void adapters remain compatible.
        [[nodiscard]] bool trySetCenter(sf::Vector2f center);
        void setCenter(sf::Vector2f center);
        const sf::Vector2f& center() const;

        [[nodiscard]] bool tryMove(sf::Vector2f offset);
        void move(sf::Vector2f offset);

        // Each axis is kept finite and positive. Extreme values are clamped
        // so the effective SFML view remains invertible.
        void setSize(sf::Vector2f size);
        const sf::Vector2f& size() const;

        // Zoom is kept finite, positive, and compatible with the base size.
        void setZoom(float zoom);
        float zoom() const;

        void setFollowSmoothness(float smoothness);
        float followSmoothness() const;

        [[nodiscard]] bool tryFollow(sf::Vector2f target, float deltaTime);
        void follow(sf::Vector2f target, float deltaTime);

        // Invalid bounds are rejected without changing the current bounds.
        [[nodiscard]] bool trySetBounds(sf::Vector2f min, sf::Vector2f max);
        void setBounds(sf::Vector2f min, sf::Vector2f max);
        void clearBounds();

        bool hasBounds() const;
        const sf::Vector2f& boundsMin() const;
        const sf::Vector2f& boundsMax() const;

        void applyTo(sf::RenderTarget& target) const;
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

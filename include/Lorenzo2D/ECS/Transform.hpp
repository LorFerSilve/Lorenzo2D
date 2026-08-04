#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    struct TransformState
    {
        sf::Vector2f position = {0.f, 0.f};
        float rotation = 0.f;
        sf::Vector2f scale = {1.f, 1.f};
    };

    class Scene;

    class Transform
    {
      public:
        Transform();
        // A nonfinite position initializes the whole vector to zero.
        explicit Transform(sf::Vector2f position);

        const sf::Vector2f& position() const;
        // Nonfinite vectors are rejected transactionally.
        void setPosition(sf::Vector2f position);
        // Nonfinite offsets and unrepresentable sums are rejected.
        void move(sf::Vector2f offset);

        float rotation() const;
        // Nonfinite angles are rejected.
        void setRotation(float rotation);
        // Nonfinite angles and unrepresentable sums are rejected.
        void rotate(float angle);

        const sf::Vector2f& scale() const;
        // Nonfinite vectors are rejected. Finite zero and negative scale
        // remain valid for hiding and mirroring.
        void setScale(sf::Vector2f scale);

        TransformState interpolated(float alpha) const;
        void resetInterpolation();

      private:
        void capturePrevious();
        void synchronizePreviousBeforeFirstSnapshot();

      private:
        TransformState m_current;
        TransformState m_previous;
        bool m_hasHistory = false;

        friend class Scene;
    };
}

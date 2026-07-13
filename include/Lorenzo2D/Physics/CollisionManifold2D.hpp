#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class Collider2D;

    struct CollisionManifold2D
    {
        sf::Vector2f normal = { 0.f, 0.f };
        sf::Vector2f point = { 0.f, 0.f };
        float penetration = 0.f;
    };

    bool computeCollisionManifold(
        const Collider2D& colliderA,
        const Collider2D& colliderB,
        CollisionManifold2D& manifold
    );
}

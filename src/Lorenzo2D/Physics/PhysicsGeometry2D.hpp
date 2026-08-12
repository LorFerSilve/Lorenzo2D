#pragma once

#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <vector>

namespace l2d
{
    class Collider2D;

    namespace detail
    {
        enum class GeometryType2D
        {
            Circle,
            Capsule,
            Polygon
        };

        struct Aabb2D
        {
            sf::Vector2f minimum = {0.f, 0.f};
            sf::Vector2f maximum = {0.f, 0.f};
        };

        struct CircleGeometry2D
        {
            sf::Vector2f center = {0.f, 0.f};
            float radius = 0.f;
        };

        struct CapsuleGeometry2D
        {
            sf::Vector2f first = {0.f, 0.f};
            sf::Vector2f second = {0.f, 0.f};
            float radius = 0.f;
        };

        struct PolygonGeometry2D
        {
            std::vector<sf::Vector2f> vertices;
        };

        struct ColliderGeometry2D
        {
            GeometryType2D type = GeometryType2D::Circle;
            CircleGeometry2D circle;
            CapsuleGeometry2D capsule;
            PolygonGeometry2D polygon;
            Aabb2D bounds;
        };

        struct GeometryRayHit2D
        {
            sf::Vector2f point = {0.f, 0.f};
            sf::Vector2f normal = {0.f, 0.f};
            float fraction = 0.f;
        };

        bool buildColliderGeometry(const Collider2D& collider, ColliderGeometry2D& geometry);
        bool makeCircleGeometry(sf::Vector2f center, float radius, ColliderGeometry2D& geometry);
        bool makeBoxGeometry(sf::Vector2f center, sf::Vector2f size, float rotationDegrees,
                             ColliderGeometry2D& geometry);
        void translateGeometry(ColliderGeometry2D& geometry, sf::Vector2f offset);

        bool aabbOverlaps(const Aabb2D& first, const Aabb2D& second);
        bool pointInsideGeometry(const ColliderGeometry2D& geometry, sf::Vector2f point);
        sf::Vector2f geometryCenter(const ColliderGeometry2D& geometry);
        sf::Vector2f supportPoint(const ColliderGeometry2D& geometry, sf::Vector2f direction);

        bool computeGeometryManifold(const ColliderGeometry2D& first,
                                     const ColliderGeometry2D& second,
                                     CollisionManifold2D& manifold);
        bool raycastGeometry(const ColliderGeometry2D& geometry, sf::Vector2f start,
                             sf::Vector2f end, GeometryRayHit2D& hit);
        bool castCircleAgainstGeometry(sf::Vector2f start, sf::Vector2f end, float radius,
                                       const ColliderGeometry2D& target, GeometryRayHit2D& hit);
        bool castPolygonAgainstGeometry(const ColliderGeometry2D& polygon, sf::Vector2f movement,
                                        const ColliderGeometry2D& target, GeometryRayHit2D& hit);
    }
}

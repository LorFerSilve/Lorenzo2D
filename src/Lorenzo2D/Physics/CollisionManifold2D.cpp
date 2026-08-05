#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;

        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        double dot(sf::Vector2f left, sf::Vector2f right)
        {
            return static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y;
        }

        sf::Vector2f finiteVector(double x, double y)
        {
            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > maximum ||
                std::fabs(y) > maximum)
            {
                const float invalid = std::numeric_limits<float>::quiet_NaN();
                return {invalid, invalid};
            }

            return {static_cast<float>(x), static_cast<float>(y)};
        }

        sf::Vector2f scaled(sf::Vector2f value, double scale)
        {
            return finiteVector(static_cast<double>(value.x) * scale,
                                static_cast<double>(value.y) * scale);
        }

        sf::Vector2f add(sf::Vector2f left, sf::Vector2f right)
        {
            return finiteVector(static_cast<double>(left.x) + right.x,
                                static_cast<double>(left.y) + right.y);
        }

        bool setManifold(sf::Vector2f normal, sf::Vector2f point, double penetration,
                         CollisionManifold2D& manifold)
        {
            if (!isFinite(normal) || !isFinite(point) || !std::isfinite(penetration) ||
                penetration < 0.0 || penetration > std::numeric_limits<float>::max())
            {
                return false;
            }

            manifold.normal = normal;
            manifold.point = point;
            manifold.penetration = static_cast<float>(penetration);
            return true;
        }

        std::array<sf::Vector2f, 2> boxAxes(const BoxCollider2D& box)
        {
            const double radians = static_cast<double>(box.worldRotation()) * PI / 180.0;
            const float cosine = static_cast<float>(std::cos(radians));
            const float sine = static_cast<float>(std::sin(radians));
            return {sf::Vector2f{cosine, sine}, sf::Vector2f{-sine, cosine}};
        }

        double projectedRadius(const BoxCollider2D& box, const std::array<sf::Vector2f, 2>& axes,
                               sf::Vector2f direction)
        {
            const sf::Vector2f half = box.worldHalfExtents();
            return std::fabs(dot(axes[0], direction)) * half.x +
                   std::fabs(dot(axes[1], direction)) * half.y;
        }

        sf::Vector2f supportPoint(const BoxCollider2D& box, const std::array<sf::Vector2f, 2>& axes,
                                  sf::Vector2f direction)
        {
            const sf::Vector2f half = box.worldHalfExtents();
            const double firstProjection = dot(axes[0], direction);
            const double secondProjection = dot(axes[1], direction);
            const double firstExtent =
                firstProjection > 1e-9 ? half.x : (firstProjection < -1e-9 ? -half.x : 0.f);
            const double secondExtent =
                secondProjection > 1e-9 ? half.y : (secondProjection < -1e-9 ? -half.y : 0.f);
            return add(add(box.center(), scaled(axes[0], firstExtent)),
                       scaled(axes[1], secondExtent));
        }

        bool circleCircleManifold(const CircleCollider2D& circleA, const CircleCollider2D& circleB,
                                  CollisionManifold2D& manifold)
        {
            const sf::Vector2f centerA = circleA.center();
            const sf::Vector2f centerB = circleB.center();
            const double radiusA = circleA.worldRadius();
            const double radiusB = circleB.worldRadius();

            if (!isFinite(centerA) || !isFinite(centerB) || !std::isfinite(radiusA) ||
                !std::isfinite(radiusB))
            {
                return false;
            }

            const double deltaX = static_cast<double>(centerB.x) - centerA.x;
            const double deltaY = static_cast<double>(centerB.y) - centerA.y;
            const double distance = std::hypot(deltaX, deltaY);
            const double radiusSum = radiusA + radiusB;

            if (!std::isfinite(distance) || distance > radiusSum) return false;

            const sf::Vector2f normal = distance > 0.0
                                            ? finiteVector(deltaX / distance, deltaY / distance)
                                            : sf::Vector2f{1.f, 0.f};
            const sf::Vector2f point = add(centerA, scaled(normal, radiusA));
            return setManifold(normal, point, radiusSum - distance, manifold);
        }

        bool boxBoxManifold(const BoxCollider2D& boxA, const BoxCollider2D& boxB,
                            CollisionManifold2D& manifold)
        {
            const sf::Vector2f centerA = boxA.center();
            const sf::Vector2f centerB = boxB.center();
            const sf::Vector2f halfA = boxA.worldHalfExtents();
            const sf::Vector2f halfB = boxB.worldHalfExtents();
            const auto axesA = boxAxes(boxA);
            const auto axesB = boxAxes(boxB);

            if (!isFinite(centerA) || !isFinite(centerB) || !isFinite(halfA) || !isFinite(halfB))
            {
                return false;
            }

            const sf::Vector2f delta = centerB - centerA;
            const std::array<sf::Vector2f, 4> candidateAxes = {axesA[0], axesA[1], axesB[0],
                                                               axesB[1]};
            double minimumOverlap = std::numeric_limits<double>::infinity();
            sf::Vector2f bestNormal = {1.f, 0.f};

            for (const sf::Vector2f axis : candidateAxes)
            {
                const double centerDistance = dot(delta, axis);
                const double overlap = projectedRadius(boxA, axesA, axis) +
                                       projectedRadius(boxB, axesB, axis) -
                                       std::fabs(centerDistance);

                if (!std::isfinite(overlap) || overlap < 0.0) return false;

                if (overlap < minimumOverlap)
                {
                    minimumOverlap = overlap;
                    bestNormal = centerDistance < 0.0 ? sf::Vector2f{-axis.x, -axis.y} : axis;
                }
            }

            const sf::Vector2f pointA = supportPoint(boxA, axesA, bestNormal);
            const sf::Vector2f pointB = supportPoint(boxB, axesB, {-bestNormal.x, -bestNormal.y});
            const sf::Vector2f point =
                finiteVector((static_cast<double>(pointA.x) + pointB.x) * 0.5,
                             (static_cast<double>(pointA.y) + pointB.y) * 0.5);
            return setManifold(bestNormal, point, minimumOverlap, manifold);
        }

        bool circleBoxManifold(const CircleCollider2D& circle, const BoxCollider2D& box,
                               CollisionManifold2D& manifold)
        {
            const sf::Vector2f circleCenter = circle.center();
            const sf::Vector2f boxCenter = box.center();
            const sf::Vector2f half = box.worldHalfExtents();
            const double radius = circle.worldRadius();
            const auto axes = boxAxes(box);

            if (!isFinite(circleCenter) || !isFinite(boxCenter) || !isFinite(half) ||
                !std::isfinite(radius))
            {
                return false;
            }

            const sf::Vector2f relative = circleCenter - boxCenter;
            const double localX = dot(relative, axes[0]);
            const double localY = dot(relative, axes[1]);
            const double closestX =
                std::clamp(localX, -static_cast<double>(half.x), static_cast<double>(half.x));
            const double closestY =
                std::clamp(localY, -static_cast<double>(half.y), static_cast<double>(half.y));
            const sf::Vector2f closest =
                add(add(boxCenter, scaled(axes[0], closestX)), scaled(axes[1], closestY));
            const double deltaX = static_cast<double>(closest.x) - circleCenter.x;
            const double deltaY = static_cast<double>(closest.y) - circleCenter.y;
            const double distance = std::hypot(deltaX, deltaY);

            if (!std::isfinite(distance) || distance > radius) return false;

            if (distance > 0.0)
            {
                return setManifold(finiteVector(deltaX / distance, deltaY / distance), closest,
                                   radius - distance, manifold);
            }

            const double distanceX = static_cast<double>(half.x) - std::fabs(localX);
            const double distanceY = static_cast<double>(half.y) - std::fabs(localY);
            sf::Vector2f normal;
            sf::Vector2f point;
            double faceDistance = 0.0;

            if (distanceX <= distanceY)
            {
                const double side = localX >= 0.0 ? 1.0 : -1.0;
                normal = scaled(axes[0], -side);
                point =
                    add(add(boxCenter, scaled(axes[0], side * half.x)), scaled(axes[1], localY));
                faceDistance = distanceX;
            }
            else
            {
                const double side = localY >= 0.0 ? 1.0 : -1.0;
                normal = scaled(axes[1], -side);
                point =
                    add(add(boxCenter, scaled(axes[0], localX)), scaled(axes[1], side * half.y));
                faceDistance = distanceY;
            }

            return setManifold(normal, point, radius + faceDistance, manifold);
        }
    }

    bool computeCollisionManifold(const Collider2D& colliderA, const Collider2D& colliderB,
                                  CollisionManifold2D& manifold)
    {
        manifold = CollisionManifold2D{};

        if (colliderA.type() == ColliderType::Circle && colliderB.type() == ColliderType::Circle)
        {
            const auto* circleA = dynamic_cast<const CircleCollider2D*>(&colliderA);
            const auto* circleB = dynamic_cast<const CircleCollider2D*>(&colliderB);
            return circleA != nullptr && circleB != nullptr &&
                   circleCircleManifold(*circleA, *circleB, manifold);
        }

        if (colliderA.type() == ColliderType::Circle && colliderB.type() == ColliderType::Box)
        {
            const auto* circle = dynamic_cast<const CircleCollider2D*>(&colliderA);
            const auto* box = dynamic_cast<const BoxCollider2D*>(&colliderB);
            return circle != nullptr && box != nullptr &&
                   circleBoxManifold(*circle, *box, manifold);
        }

        if (colliderA.type() == ColliderType::Box && colliderB.type() == ColliderType::Circle)
        {
            const auto* box = dynamic_cast<const BoxCollider2D*>(&colliderA);
            const auto* circle = dynamic_cast<const CircleCollider2D*>(&colliderB);

            if (box == nullptr || circle == nullptr || !circleBoxManifold(*circle, *box, manifold))
            {
                return false;
            }

            manifold.normal = {-manifold.normal.x, -manifold.normal.y};
            return true;
        }

        if (colliderA.type() == ColliderType::Box && colliderB.type() == ColliderType::Box)
        {
            const auto* boxA = dynamic_cast<const BoxCollider2D*>(&colliderA);
            const auto* boxB = dynamic_cast<const BoxCollider2D*>(&colliderB);
            return boxA != nullptr && boxB != nullptr && boxBoxManifold(*boxA, *boxB, manifold);
        }

        return false;
    }
}

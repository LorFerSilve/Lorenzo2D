#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        bool isFinite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float finiteFloat(double value)
        {
            if (std::isnan(value)) return 0.f;

            const double maximum = static_cast<double>(std::numeric_limits<float>::max());

            value = std::clamp(value, -maximum, maximum);
            return static_cast<float>(value);
        }

        sf::Vector2f finiteVector(double x, double y)
        {
            return {finiteFloat(x), finiteFloat(y)};
        }

        bool setManifold(sf::Vector2f normal, sf::Vector2f point, double penetration,
                         CollisionManifold2D& manifold)
        {
            if (!isFinite(normal) || !isFinite(point) || !std::isfinite(penetration) ||
                penetration < 0.0)
            {
                return false;
            }

            manifold.normal = normal;
            manifold.point = point;
            manifold.penetration = finiteFloat(penetration);
            return true;
        }

        bool circleCircleManifold(const CircleCollider2D& circleA, const CircleCollider2D& circleB,
                                  CollisionManifold2D& manifold)
        {
            const sf::Vector2f centerA = circleA.center();
            const sf::Vector2f centerB = circleB.center();

            if (!isFinite(centerA) || !isFinite(centerB)) return false;

            const double deltaX = static_cast<double>(centerB.x) - centerA.x;
            const double deltaY = static_cast<double>(centerB.y) - centerA.y;
            const double distance = std::hypot(deltaX, deltaY);
            const double radiusA = static_cast<double>(circleA.radius());
            const double radiusB = static_cast<double>(circleB.radius());
            const double radiusSum = radiusA + radiusB;

            if (!std::isfinite(distance) || !std::isfinite(radiusSum) || distance > radiusSum)
            {
                return false;
            }

            sf::Vector2f normal = {1.f, 0.f};

            if (distance > 0.0)
            {
                normal = finiteVector(deltaX / distance, deltaY / distance);
            }

            const double pointOnA_X = static_cast<double>(centerA.x) + normal.x * radiusA;
            const double pointOnA_Y = static_cast<double>(centerA.y) + normal.y * radiusA;
            const double pointOnB_X = static_cast<double>(centerB.x) - normal.x * radiusB;
            const double pointOnB_Y = static_cast<double>(centerB.y) - normal.y * radiusB;

            return setManifold(normal,
                               finiteVector(pointOnA_X + (pointOnB_X - pointOnA_X) * 0.5,
                                            pointOnA_Y + (pointOnB_Y - pointOnA_Y) * 0.5),
                               radiusSum - distance, manifold);
        }

        bool boxBoxManifold(const BoxCollider2D& boxA, const BoxCollider2D& boxB,
                            CollisionManifold2D& manifold)
        {
            const sf::Vector2f minimumA = boxA.min();
            const sf::Vector2f maximumA = boxA.max();
            const sf::Vector2f minimumB = boxB.min();
            const sf::Vector2f maximumB = boxB.max();

            if (!isFinite(minimumA) || !isFinite(maximumA) || !isFinite(minimumB) ||
                !isFinite(maximumB))
            {
                return false;
            }

            const double overlapX =
                std::min(static_cast<double>(maximumA.x), static_cast<double>(maximumB.x)) -
                std::max(static_cast<double>(minimumA.x), static_cast<double>(minimumB.x));
            const double overlapY =
                std::min(static_cast<double>(maximumA.y), static_cast<double>(maximumB.y)) -
                std::max(static_cast<double>(minimumA.y), static_cast<double>(minimumB.y));

            if (overlapX < 0.0 || overlapY < 0.0) return false;

            // Directional face distances produce the actual minimum
            // translation even when either box fully contains the other.
            // Strict comparisons retain +X, -X, +Y, -Y as the tie order.
            double penetration = static_cast<double>(maximumA.x) - minimumB.x;
            sf::Vector2f normal = {1.f, 0.f};

            const double moveFirstRight = static_cast<double>(maximumB.x) - minimumA.x;

            if (moveFirstRight < penetration)
            {
                penetration = moveFirstRight;
                normal = {-1.f, 0.f};
            }

            const double moveFirstUp = static_cast<double>(maximumA.y) - minimumB.y;

            if (moveFirstUp < penetration)
            {
                penetration = moveFirstUp;
                normal = {0.f, 1.f};
            }

            const double moveFirstDown = static_cast<double>(maximumB.y) - minimumA.y;

            if (moveFirstDown < penetration)
            {
                penetration = moveFirstDown;
                normal = {0.f, -1.f};
            }

            const double overlapMinimumX =
                std::max(static_cast<double>(minimumA.x), static_cast<double>(minimumB.x));
            const double overlapMaximumX =
                std::min(static_cast<double>(maximumA.x), static_cast<double>(maximumB.x));
            const double overlapMinimumY =
                std::max(static_cast<double>(minimumA.y), static_cast<double>(minimumB.y));
            const double overlapMaximumY =
                std::min(static_cast<double>(maximumA.y), static_cast<double>(maximumB.y));

            return setManifold(
                normal,
                finiteVector(overlapMinimumX + (overlapMaximumX - overlapMinimumX) * 0.5,
                             overlapMinimumY + (overlapMaximumY - overlapMinimumY) * 0.5),
                penetration, manifold);
        }

        bool circleBoxManifold(const CircleCollider2D& circle, const BoxCollider2D& box,
                               CollisionManifold2D& manifold)
        {
            const sf::Vector2f center = circle.center();
            const sf::Vector2f minimum = box.min();
            const sf::Vector2f maximum = box.max();

            if (!isFinite(center) || !isFinite(minimum) || !isFinite(maximum))
            {
                return false;
            }

            const double centerX = static_cast<double>(center.x);
            const double centerY = static_cast<double>(center.y);
            const double minimumX = static_cast<double>(minimum.x);
            const double minimumY = static_cast<double>(minimum.y);
            const double maximumX = static_cast<double>(maximum.x);
            const double maximumY = static_cast<double>(maximum.y);
            const double radius = static_cast<double>(circle.radius());
            const double closestX = std::clamp(centerX, minimumX, maximumX);
            const double closestY = std::clamp(centerY, minimumY, maximumY);
            const double deltaX = closestX - centerX;
            const double deltaY = closestY - centerY;
            const double distance = std::hypot(deltaX, deltaY);

            if (!std::isfinite(distance) || distance > radius) return false;

            if (distance > 0.0)
            {
                return setManifold(finiteVector(deltaX / distance, deltaY / distance),
                                   finiteVector(closestX, closestY), radius - distance, manifold);
            }

            const double distanceToLeft = centerX - minimumX;
            const double distanceToRight = maximumX - centerX;
            const double distanceToTop = centerY - minimumY;
            const double distanceToBottom = maximumY - centerY;

            double nearestDistance = distanceToLeft;
            sf::Vector2f normal = {1.f, 0.f};
            sf::Vector2f point = finiteVector(minimumX, centerY);

            if (distanceToRight < nearestDistance)
            {
                nearestDistance = distanceToRight;
                normal = {-1.f, 0.f};
                point = finiteVector(maximumX, centerY);
            }

            if (distanceToTop < nearestDistance)
            {
                nearestDistance = distanceToTop;
                normal = {0.f, 1.f};
                point = finiteVector(centerX, minimumY);
            }

            if (distanceToBottom < nearestDistance)
            {
                nearestDistance = distanceToBottom;
                normal = {0.f, -1.f};
                point = finiteVector(centerX, maximumY);
            }

            return setManifold(normal, point, radius + nearestDistance, manifold);
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

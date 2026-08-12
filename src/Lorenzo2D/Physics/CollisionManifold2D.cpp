#include <Lorenzo2D/Physics/CollisionManifold2D.hpp>

#include <Lorenzo2D/Physics/Collider2D.hpp>

#include "PhysicsGeometry2D.hpp"

namespace l2d
{
    bool computeCollisionManifold(const Collider2D& colliderA, const Collider2D& colliderB,
                                  CollisionManifold2D& manifold)
    {
        detail::ColliderGeometry2D first;
        detail::ColliderGeometry2D second;
        manifold = {};
        return detail::buildColliderGeometry(colliderA, first) &&
               detail::buildColliderGeometry(colliderB, second) &&
               detail::computeGeometryManifold(first, second, manifold);
    }
}

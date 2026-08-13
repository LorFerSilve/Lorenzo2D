#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "PhysicsGeometry2D.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

namespace l2d
{
    namespace
    {
        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float distance(sf::Vector2f first, sf::Vector2f second)
        {
            return std::hypot(second.x - first.x, second.y - first.y);
        }

        bool hitLess(const PhysicsQueryHit2D& first, const PhysicsQueryHit2D& second)
        {
            if (first.distance != second.distance) return first.distance < second.distance;
            return first.colliderId < second.colliderId;
        }
    }

    struct PhysicsQueryContext2D::Impl
    {
        struct Proxy
        {
            GameObjectHandle object;
            GameObjectId objectId = InvalidGameObjectId;
            ColliderId colliderId = InvalidColliderId;
            std::uint32_t categoryBits = 0u;
            bool sensor = false;
            detail::ColliderGeometry2D geometry;
        };

        std::vector<Proxy> proxies;

        static bool accepts(const Proxy& proxy, const PhysicsQueryFilter2D& filter)
        {
            return (proxy.categoryBits & filter.categoryMask) != 0u &&
                   (filter.includeSensors || !proxy.sensor) &&
                   (filter.ignoredObject == InvalidGameObjectId ||
                    proxy.objectId != filter.ignoredObject);
        }

        static PhysicsQueryHit2D makeHit(const Proxy& proxy,
                                         const detail::GeometryRayHit2D& geometryHit,
                                         float pathLength)
        {
            PhysicsQueryHit2D hit;
            hit.object = proxy.object;
            hit.colliderId = proxy.colliderId;
            hit.point = geometryHit.point;
            hit.normal = geometryHit.normal;
            hit.fraction = geometryHit.fraction;
            hit.distance = pathLength * geometryHit.fraction;
            hit.categoryBits = proxy.categoryBits;
            hit.sensor = proxy.sensor;
            return hit;
        }

        std::optional<PhysicsQueryHit2D> earliestCast(
            sf::Vector2f start, sf::Vector2f end, const PhysicsQueryFilter2D& filter,
            const std::function<bool(const detail::ColliderGeometry2D&, detail::GeometryRayHit2D&)>&
                cast) const
        {
            const float pathLength = distance(start, end);
            std::optional<PhysicsQueryHit2D> result;
            for (const Proxy& proxy : proxies)
            {
                if (!accepts(proxy, filter)) continue;
                detail::GeometryRayHit2D geometryHit;
                if (!cast(proxy.geometry, geometryHit)) continue;
                PhysicsQueryHit2D candidate = makeHit(proxy, geometryHit, pathLength);
                if (!result || hitLess(candidate, *result)) result = std::move(candidate);
            }
            return result;
        }

        std::vector<PhysicsQueryHit2D> allCasts(
            sf::Vector2f start, sf::Vector2f end, const PhysicsQueryFilter2D& filter,
            const std::function<bool(const detail::ColliderGeometry2D&, detail::GeometryRayHit2D&)>&
                cast) const
        {
            const float pathLength = distance(start, end);
            std::vector<PhysicsQueryHit2D> hits;
            for (const Proxy& proxy : proxies)
            {
                if (!accepts(proxy, filter)) continue;
                detail::GeometryRayHit2D geometryHit;
                if (cast(proxy.geometry, geometryHit))
                    hits.push_back(makeHit(proxy, geometryHit, pathLength));
            }
            std::sort(hits.begin(), hits.end(), hitLess);
            return hits;
        }
    };

    PhysicsQueryContext2D::PhysicsQueryContext2D(Scene& scene) : m_impl()
    {
        auto impl = std::make_shared<Impl>();
        impl->proxies.reserve(scene.gameObjects().size());
        for (const std::unique_ptr<GameObject>& objectPointer : scene.gameObjects())
        {
            GameObject* object = objectPointer.get();
            if (object == nullptr || !object->isActive() || object->isDestroyQueued()) continue;
            for (const Collider2D* collider : object->getComponents<Collider2D>())
            {
                if (collider == nullptr || !collider->isActive()) continue;
                Impl::Proxy proxy;
                if (!detail::buildColliderGeometry(*collider, proxy.geometry)) continue;
                proxy.object = scene.createHandle(*object);
                proxy.objectId = object->id();
                proxy.colliderId = collider->id();
                proxy.categoryBits = collider->filter().categoryBits;
                proxy.sensor = collider->isSensor();
                impl->proxies.push_back(std::move(proxy));
            }
        }
        std::sort(impl->proxies.begin(), impl->proxies.end(),
                  [](const Impl::Proxy& first, const Impl::Proxy& second)
                  {
                      if (first.objectId != second.objectId)
                          return first.objectId < second.objectId;
                      return first.colliderId < second.colliderId;
                  });
        m_impl = std::move(impl);
    }

    std::size_t PhysicsQueryContext2D::proxyCount() const
    {
        return m_impl->proxies.size();
    }

    std::optional<PhysicsQueryHit2D> PhysicsQueryContext2D::raycast(
        sf::Vector2f start, sf::Vector2f end, const PhysicsQueryFilter2D& filter) const
    {
        if (!finite(start) || !finite(end)) return {};
        const float pathLength = distance(start, end);
        std::optional<PhysicsQueryHit2D> result;
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter)) continue;
            detail::GeometryRayHit2D geometryHit;
            if (!detail::raycastGeometry(proxy.geometry, start, end, geometryHit)) continue;
            PhysicsQueryHit2D candidate = Impl::makeHit(proxy, geometryHit, pathLength);
            if (!result || hitLess(candidate, *result)) result = std::move(candidate);
        }
        return result;
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::raycastAll(
        sf::Vector2f start, sf::Vector2f end, const PhysicsQueryFilter2D& filter) const
    {
        std::vector<PhysicsQueryHit2D> hits;
        if (!finite(start) || !finite(end)) return hits;
        const float pathLength = distance(start, end);
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter)) continue;
            detail::GeometryRayHit2D geometryHit;
            if (detail::raycastGeometry(proxy.geometry, start, end, geometryHit))
                hits.push_back(Impl::makeHit(proxy, geometryHit, pathLength));
        }
        std::sort(hits.begin(), hits.end(), hitLess);
        return hits;
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::pointQuery(
        sf::Vector2f point, const PhysicsQueryFilter2D& filter) const
    {
        std::vector<PhysicsQueryHit2D> hits;
        if (!finite(point)) return hits;
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter) ||
                !detail::pointInsideGeometry(proxy.geometry, point))
                continue;
            PhysicsQueryHit2D hit;
            hit.object = proxy.object;
            hit.colliderId = proxy.colliderId;
            hit.point = point;
            hit.categoryBits = proxy.categoryBits;
            hit.sensor = proxy.sensor;
            hits.push_back(std::move(hit));
        }
        std::sort(hits.begin(), hits.end(), hitLess);
        return hits;
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::overlapCircle(
        sf::Vector2f center, float radius, const PhysicsQueryFilter2D& filter) const
    {
        std::vector<PhysicsQueryHit2D> hits;
        detail::ColliderGeometry2D query;
        if (!detail::makeCircleGeometry(center, radius, query)) return hits;
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter) ||
                !detail::aabbOverlaps(query.bounds, proxy.geometry.bounds))
                continue;
            CollisionManifold2D manifold;
            if (!detail::computeGeometryManifold(query, proxy.geometry, manifold)) continue;
            PhysicsQueryHit2D hit;
            hit.object = proxy.object;
            hit.colliderId = proxy.colliderId;
            hit.normal = {-manifold.normal.x, -manifold.normal.y};
            hit.point = detail::supportPoint(proxy.geometry, hit.normal);
            hit.distance = distance(center, hit.point);
            hit.penetration = manifold.penetration;
            hit.categoryBits = proxy.categoryBits;
            hit.sensor = proxy.sensor;
            hits.push_back(std::move(hit));
        }
        std::sort(hits.begin(), hits.end(), hitLess);
        return hits;
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::overlapBox(
        sf::Vector2f center, sf::Vector2f size, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        std::vector<PhysicsQueryHit2D> hits;
        detail::ColliderGeometry2D query;
        if (!detail::makeBoxGeometry(center, size, rotationDegrees, query)) return hits;
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter) ||
                !detail::aabbOverlaps(query.bounds, proxy.geometry.bounds))
                continue;
            CollisionManifold2D manifold;
            if (!detail::computeGeometryManifold(query, proxy.geometry, manifold)) continue;
            PhysicsQueryHit2D hit;
            hit.object = proxy.object;
            hit.colliderId = proxy.colliderId;
            hit.normal = {-manifold.normal.x, -manifold.normal.y};
            hit.point = detail::supportPoint(proxy.geometry, hit.normal);
            hit.distance = distance(center, hit.point);
            hit.penetration = manifold.penetration;
            hit.categoryBits = proxy.categoryBits;
            hit.sensor = proxy.sensor;
            hits.push_back(std::move(hit));
        }
        std::sort(hits.begin(), hits.end(), hitLess);
        return hits;
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::overlapCapsule(
        sf::Vector2f center, float radius, float height, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        std::vector<PhysicsQueryHit2D> hits;
        detail::ColliderGeometry2D query;
        if (!detail::makeCapsuleGeometry(center, radius, height, rotationDegrees, query))
            return hits;
        for (const Impl::Proxy& proxy : m_impl->proxies)
        {
            if (!Impl::accepts(proxy, filter) ||
                !detail::aabbOverlaps(query.bounds, proxy.geometry.bounds))
                continue;
            CollisionManifold2D manifold;
            if (!detail::computeGeometryManifold(query, proxy.geometry, manifold)) continue;
            PhysicsQueryHit2D hit;
            hit.object = proxy.object;
            hit.colliderId = proxy.colliderId;
            hit.normal = {-manifold.normal.x, -manifold.normal.y};
            hit.point = detail::supportPoint(proxy.geometry, hit.normal);
            hit.distance = distance(center, hit.point);
            hit.penetration = manifold.penetration;
            hit.categoryBits = proxy.categoryBits;
            hit.sensor = proxy.sensor;
            hits.push_back(std::move(hit));
        }
        std::sort(hits.begin(), hits.end(), hitLess);
        return hits;
    }

    std::optional<PhysicsQueryHit2D> PhysicsQueryContext2D::castCircle(
        sf::Vector2f start, sf::Vector2f end, float radius,
        const PhysicsQueryFilter2D& filter) const
    {
        if (!finite(start) || !finite(end) || !std::isfinite(radius) || radius < 0.f) return {};
        return m_impl->earliestCast(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castCircleAgainstGeometry(start, end, radius, target, hit); });
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::castCircleAll(
        sf::Vector2f start, sf::Vector2f end, float radius,
        const PhysicsQueryFilter2D& filter) const
    {
        if (!finite(start) || !finite(end) || !std::isfinite(radius) || radius < 0.f) return {};
        return m_impl->allCasts(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castCircleAgainstGeometry(start, end, radius, target, hit); });
    }

    std::optional<PhysicsQueryHit2D> PhysicsQueryContext2D::castBox(
        sf::Vector2f start, sf::Vector2f end, sf::Vector2f size, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        detail::ColliderGeometry2D query;
        if (!finite(start) || !finite(end) ||
            !detail::makeBoxGeometry(start, size, rotationDegrees, query))
            return {};
        const sf::Vector2f movement = end - start;
        return m_impl->earliestCast(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castPolygonAgainstGeometry(query, movement, target, hit); });
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::castBoxAll(
        sf::Vector2f start, sf::Vector2f end, sf::Vector2f size, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        detail::ColliderGeometry2D query;
        if (!finite(start) || !finite(end) ||
            !detail::makeBoxGeometry(start, size, rotationDegrees, query))
            return {};
        const sf::Vector2f movement = end - start;
        return m_impl->allCasts(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castPolygonAgainstGeometry(query, movement, target, hit); });
    }

    std::optional<PhysicsQueryHit2D> PhysicsQueryContext2D::castCapsule(
        sf::Vector2f start, sf::Vector2f end, float radius, float height, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        detail::ColliderGeometry2D query;
        if (!finite(start) || !finite(end) ||
            !detail::makeCapsuleGeometry(start, radius, height, rotationDegrees, query))
            return {};
        const sf::Vector2f movement = end - start;
        return m_impl->earliestCast(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castCapsuleAgainstGeometry(query, movement, target, hit); });
    }

    std::vector<PhysicsQueryHit2D> PhysicsQueryContext2D::castCapsuleAll(
        sf::Vector2f start, sf::Vector2f end, float radius, float height, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        detail::ColliderGeometry2D query;
        if (!finite(start) || !finite(end) ||
            !detail::makeCapsuleGeometry(start, radius, height, rotationDegrees, query))
            return {};
        const sf::Vector2f movement = end - start;
        return m_impl->allCasts(
            start, end, filter,
            [&](const detail::ColliderGeometry2D& target, detail::GeometryRayHit2D& hit)
            { return detail::castCapsuleAgainstGeometry(query, movement, target, hit); });
    }

    PhysicsQueryContext2D PhysicsWorld2D::createQueryContext(Scene& scene) const
    {
        return PhysicsQueryContext2D(scene);
    }

    std::optional<PhysicsQueryHit2D> PhysicsWorld2D::raycast(
        Scene& scene, sf::Vector2f start, sf::Vector2f end,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).raycast(start, end, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::raycastAll(
        Scene& scene, sf::Vector2f start, sf::Vector2f end,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).raycastAll(start, end, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::pointQuery(
        Scene& scene, sf::Vector2f point, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).pointQuery(point, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::overlapCircle(
        Scene& scene, sf::Vector2f center, float radius, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).overlapCircle(center, radius, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::overlapBox(
        Scene& scene, sf::Vector2f center, sf::Vector2f size, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).overlapBox(center, size, rotationDegrees, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::overlapCapsule(
        Scene& scene, sf::Vector2f center, float radius, float height, float rotationDegrees,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).overlapCapsule(center, radius, height, rotationDegrees,
                                                           filter);
    }

    std::optional<PhysicsQueryHit2D> PhysicsWorld2D::castCircle(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, float radius,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castCircle(start, end, radius, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::castCircleAll(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, float radius,
        const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castCircleAll(start, end, radius, filter);
    }

    std::optional<PhysicsQueryHit2D> PhysicsWorld2D::castBox(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, sf::Vector2f size,
        float rotationDegrees, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castBox(start, end, size, rotationDegrees, filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::castBoxAll(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, sf::Vector2f size,
        float rotationDegrees, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castBoxAll(start, end, size, rotationDegrees, filter);
    }

    std::optional<PhysicsQueryHit2D> PhysicsWorld2D::castCapsule(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, float radius, float height,
        float rotationDegrees, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castCapsule(start, end, radius, height, rotationDegrees,
                                                        filter);
    }

    std::vector<PhysicsQueryHit2D> PhysicsWorld2D::castCapsuleAll(
        Scene& scene, sf::Vector2f start, sf::Vector2f end, float radius, float height,
        float rotationDegrees, const PhysicsQueryFilter2D& filter) const
    {
        return PhysicsQueryContext2D(scene).castCapsuleAll(start, end, radius, height,
                                                           rotationDegrees, filter);
    }
}

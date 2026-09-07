#include "PhysicsGeometry2D.hpp"

#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/Collider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace l2d::detail
{
    namespace
    {
        constexpr double Epsilon = 1e-9;
        constexpr double ContactTolerance = 1e-6;
        constexpr double Pi = 3.14159265358979323846;

        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        double dot(sf::Vector2f first, sf::Vector2f second)
        {
            return static_cast<double>(first.x) * second.x +
                   static_cast<double>(first.y) * second.y;
        }

        double cross(sf::Vector2f first, sf::Vector2f second)
        {
            return static_cast<double>(first.x) * second.y -
                   static_cast<double>(first.y) * second.x;
        }

        double lengthSquared(sf::Vector2f value)
        {
            return dot(value, value);
        }

        double length(sf::Vector2f value)
        {
            return std::sqrt(lengthSquared(value));
        }

        sf::Vector2f vectorFromDoubles(double x, double y)
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

        sf::Vector2f multiply(sf::Vector2f value, double scalar)
        {
            return vectorFromDoubles(static_cast<double>(value.x) * scalar,
                                     static_cast<double>(value.y) * scalar);
        }

        sf::Vector2f normalized(sf::Vector2f value, sf::Vector2f fallback = {1.f, 0.f})
        {
            const double magnitude = length(value);
            if (!std::isfinite(magnitude) || magnitude <= Epsilon) return fallback;
            return multiply(value, 1.0 / magnitude);
        }

        double polygonSignedArea(const PolygonGeometry2D& polygon)
        {
            double result = 0.0;
            for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
            {
                const sf::Vector2f current = polygon.vertices[index];
                const sf::Vector2f next = polygon.vertices[(index + 1u) % polygon.vertices.size()];
                result += cross(current, next);
            }
            return result * 0.5;
        }

        sf::Vector2f outwardNormal(const PolygonGeometry2D& polygon, std::size_t edgeIndex)
        {
            const sf::Vector2f first = polygon.vertices[edgeIndex];
            const sf::Vector2f second =
                polygon.vertices[(edgeIndex + 1u) % polygon.vertices.size()];
            const sf::Vector2f edge = second - first;
            return polygonSignedArea(polygon) >= 0.0 ? normalized({edge.y, -edge.x})
                                                     : normalized({-edge.y, edge.x});
        }

        bool computeBounds(ColliderGeometry2D& geometry)
        {
            sf::Vector2f minimum;
            sf::Vector2f maximum;

            if (geometry.type == GeometryType2D::Circle)
            {
                if (!finite(geometry.circle.center) || !std::isfinite(geometry.circle.radius) ||
                    geometry.circle.radius < 0.f)
                    return false;
                minimum = geometry.circle.center -
                          sf::Vector2f{geometry.circle.radius, geometry.circle.radius};
                maximum = geometry.circle.center +
                          sf::Vector2f{geometry.circle.radius, geometry.circle.radius};
            }
            else if (geometry.type == GeometryType2D::Capsule)
            {
                if (!finite(geometry.capsule.first) || !finite(geometry.capsule.second) ||
                    !std::isfinite(geometry.capsule.radius) || geometry.capsule.radius < 0.f)
                    return false;
                minimum = {std::min(geometry.capsule.first.x, geometry.capsule.second.x) -
                               geometry.capsule.radius,
                           std::min(geometry.capsule.first.y, geometry.capsule.second.y) -
                               geometry.capsule.radius};
                maximum = {std::max(geometry.capsule.first.x, geometry.capsule.second.x) +
                               geometry.capsule.radius,
                           std::max(geometry.capsule.first.y, geometry.capsule.second.y) +
                               geometry.capsule.radius};
            }
            else
            {
                if (geometry.polygon.vertices.size() < 3u) return false;
                minimum = geometry.polygon.vertices.front();
                maximum = minimum;
                for (const sf::Vector2f vertex : geometry.polygon.vertices)
                {
                    if (!finite(vertex)) return false;
                    minimum.x = std::min(minimum.x, vertex.x);
                    minimum.y = std::min(minimum.y, vertex.y);
                    maximum.x = std::max(maximum.x, vertex.x);
                    maximum.y = std::max(maximum.y, vertex.y);
                }
            }

            if (!finite(minimum) || !finite(maximum) || minimum.x > maximum.x ||
                minimum.y > maximum.y)
                return false;
            geometry.bounds = {minimum, maximum};
            return true;
        }

        sf::Vector2f closestPointOnSegment(sf::Vector2f point, sf::Vector2f first,
                                           sf::Vector2f second)
        {
            const sf::Vector2f edge = second - first;
            const double denominator = lengthSquared(edge);
            if (denominator <= Epsilon) return first;
            const double parameter = std::clamp(dot(point - first, edge) / denominator, 0.0, 1.0);
            return first + multiply(edge, parameter);
        }

        void closestPointsOnSegments(sf::Vector2f firstA, sf::Vector2f secondA, sf::Vector2f firstB,
                                     sf::Vector2f secondB, sf::Vector2f& pointA,
                                     sf::Vector2f& pointB)
        {
            const sf::Vector2f directionA = secondA - firstA;
            const sf::Vector2f directionB = secondB - firstB;
            const sf::Vector2f offset = firstA - firstB;
            const double a = dot(directionA, directionA);
            const double e = dot(directionB, directionB);
            const double f = dot(directionB, offset);
            double s = 0.0;
            double t = 0.0;

            if (a <= Epsilon && e <= Epsilon)
            {
                pointA = firstA;
                pointB = firstB;
                return;
            }
            if (a <= Epsilon)
            {
                t = std::clamp(f / e, 0.0, 1.0);
            }
            else
            {
                const double c = dot(directionA, offset);
                if (e <= Epsilon)
                {
                    s = std::clamp(-c / a, 0.0, 1.0);
                }
                else
                {
                    const double b = dot(directionA, directionB);
                    const double denominator = a * e - b * b;
                    if (std::fabs(denominator) > Epsilon)
                        s = std::clamp((b * f - c * e) / denominator, 0.0, 1.0);
                    t = (b * s + f) / e;
                    if (t < 0.0)
                    {
                        t = 0.0;
                        s = std::clamp(-c / a, 0.0, 1.0);
                    }
                    else if (t > 1.0)
                    {
                        t = 1.0;
                        s = std::clamp((b - c) / a, 0.0, 1.0);
                    }
                }
            }
            pointA = firstA + multiply(directionA, s);
            pointB = firstB + multiply(directionB, t);
        }

        void projectPolygon(const PolygonGeometry2D& polygon, sf::Vector2f axis, double& minimum,
                            double& maximum)
        {
            minimum = maximum = dot(polygon.vertices.front(), axis);
            for (const sf::Vector2f vertex : polygon.vertices)
            {
                const double projection = dot(vertex, axis);
                minimum = std::min(minimum, projection);
                maximum = std::max(maximum, projection);
            }
        }

        void projectCapsule(const CapsuleGeometry2D& capsule, sf::Vector2f axis, double& minimum,
                            double& maximum)
        {
            const double first = dot(capsule.first, axis);
            const double second = dot(capsule.second, axis);
            minimum = std::min(first, second) - capsule.radius;
            maximum = std::max(first, second) + capsule.radius;
        }

        bool intervalPenetration(double firstMinimum, double firstMaximum, double secondMinimum,
                                 double secondMaximum, double& penetration)
        {
            if (firstMaximum < secondMinimum - ContactTolerance ||
                secondMaximum < firstMinimum - ContactTolerance)
                return false;
            penetration = std::min(firstMaximum - secondMinimum, secondMaximum - firstMinimum);
            return std::isfinite(penetration);
        }

        bool setManifold(sf::Vector2f normal, sf::Vector2f point, double penetration,
                         CollisionManifold2D& manifold)
        {
            if (!finite(normal) || !finite(point) || !std::isfinite(penetration) ||
                penetration < -ContactTolerance || penetration > std::numeric_limits<float>::max())
                return false;
            manifold.normal = normal;
            manifold.point = point;
            manifold.penetration = static_cast<float>(std::max(0.0, penetration));
            return true;
        }

        bool circleCircle(const CircleGeometry2D& first, const CircleGeometry2D& second,
                          CollisionManifold2D& manifold)
        {
            const sf::Vector2f delta = second.center - first.center;
            const double distance = length(delta);
            const double radius = static_cast<double>(first.radius) + second.radius;
            if (!std::isfinite(distance) || distance > radius + ContactTolerance) return false;
            const sf::Vector2f normal = normalized(delta);
            const sf::Vector2f firstPoint = first.center + multiply(normal, first.radius);
            const sf::Vector2f secondPoint = second.center - multiply(normal, second.radius);
            return setManifold(normal, multiply(firstPoint + secondPoint, 0.5), radius - distance,
                               manifold);
        }

        bool polygonPolygon(const PolygonGeometry2D& first, const PolygonGeometry2D& second,
                            CollisionManifold2D& manifold)
        {
            double minimumOverlap = std::numeric_limits<double>::infinity();
            sf::Vector2f bestAxis = {1.f, 0.f};
            const sf::Vector2f delta =
                geometryCenter({GeometryType2D::Polygon, {}, {}, second, {}}) -
                geometryCenter({GeometryType2D::Polygon, {}, {}, first, {}});

            const std::array<const PolygonGeometry2D*, 2> polygons = {&first, &second};
            for (const PolygonGeometry2D* polygon : polygons)
            {
                for (std::size_t index = 0; index < polygon->vertices.size(); ++index)
                {
                    sf::Vector2f axis = outwardNormal(*polygon, index);
                    if (dot(delta, axis) < 0.0) axis = {-axis.x, -axis.y};
                    double firstMin = 0.0;
                    double firstMax = 0.0;
                    double secondMin = 0.0;
                    double secondMax = 0.0;
                    projectPolygon(first, axis, firstMin, firstMax);
                    projectPolygon(second, axis, secondMin, secondMax);
                    double overlap = 0.0;
                    if (!intervalPenetration(firstMin, firstMax, secondMin, secondMax, overlap))
                        return false;
                    if (overlap < minimumOverlap)
                    {
                        minimumOverlap = overlap;
                        bestAxis = axis;
                    }
                }
            }

            ColliderGeometry2D firstGeometry;
            firstGeometry.type = GeometryType2D::Polygon;
            firstGeometry.polygon = first;
            ColliderGeometry2D secondGeometry;
            secondGeometry.type = GeometryType2D::Polygon;
            secondGeometry.polygon = second;
            const sf::Vector2f firstPoint = supportPoint(firstGeometry, bestAxis);
            const sf::Vector2f secondPoint =
                supportPoint(secondGeometry, {-bestAxis.x, -bestAxis.y});
            return setManifold(bestAxis, multiply(firstPoint + secondPoint, 0.5), minimumOverlap,
                               manifold);
        }

        bool circlePolygon(const CircleGeometry2D& circle, const PolygonGeometry2D& polygon,
                           CollisionManifold2D& manifold)
        {
            std::vector<sf::Vector2f> axes;
            axes.reserve(polygon.vertices.size() * 2u);
            for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
            {
                axes.push_back(outwardNormal(polygon, index));
                const sf::Vector2f vertexAxis = polygon.vertices[index] - circle.center;
                if (lengthSquared(vertexAxis) > Epsilon) axes.push_back(normalized(vertexAxis));
            }

            const sf::Vector2f polygonCenter = [&]()
            {
                sf::Vector2f center = {0.f, 0.f};
                for (const sf::Vector2f vertex : polygon.vertices)
                    center += vertex;
                return multiply(center, 1.0 / static_cast<double>(polygon.vertices.size()));
            }();
            const sf::Vector2f centerDelta = polygonCenter - circle.center;
            double minimumOverlap = std::numeric_limits<double>::infinity();
            sf::Vector2f bestAxis = {1.f, 0.f};

            for (sf::Vector2f axis : axes)
            {
                if (dot(centerDelta, axis) < 0.0) axis = {-axis.x, -axis.y};
                const double circleCenterProjection = dot(circle.center, axis);
                const double circleMin = circleCenterProjection - circle.radius;
                const double circleMax = circleCenterProjection + circle.radius;
                double polygonMin = 0.0;
                double polygonMax = 0.0;
                projectPolygon(polygon, axis, polygonMin, polygonMax);
                double overlap = 0.0;
                if (!intervalPenetration(circleMin, circleMax, polygonMin, polygonMax, overlap))
                    return false;
                if (overlap < minimumOverlap)
                {
                    minimumOverlap = overlap;
                    bestAxis = axis;
                }
            }

            ColliderGeometry2D polygonGeometry;
            polygonGeometry.type = GeometryType2D::Polygon;
            polygonGeometry.polygon = polygon;
            const sf::Vector2f firstPoint = circle.center + multiply(bestAxis, circle.radius);
            const sf::Vector2f secondPoint =
                supportPoint(polygonGeometry, {-bestAxis.x, -bestAxis.y});
            return setManifold(bestAxis, multiply(firstPoint + secondPoint, 0.5), minimumOverlap,
                               manifold);
        }

        bool capsuleCircle(const CapsuleGeometry2D& capsule, const CircleGeometry2D& circle,
                           CollisionManifold2D& manifold)
        {
            const sf::Vector2f closest =
                closestPointOnSegment(circle.center, capsule.first, capsule.second);
            const sf::Vector2f delta = circle.center - closest;
            const double distance = length(delta);
            const double radius = static_cast<double>(capsule.radius) + circle.radius;
            if (!std::isfinite(distance) || distance > radius + ContactTolerance) return false;
            const sf::Vector2f fallback = normalized(
                {capsule.second.y - capsule.first.y, capsule.first.x - capsule.second.x});
            const sf::Vector2f normal = normalized(delta, fallback);
            const sf::Vector2f firstPoint = closest + multiply(normal, capsule.radius);
            const sf::Vector2f secondPoint = circle.center - multiply(normal, circle.radius);
            return setManifold(normal, multiply(firstPoint + secondPoint, 0.5), radius - distance,
                               manifold);
        }

        bool capsuleCapsule(const CapsuleGeometry2D& first, const CapsuleGeometry2D& second,
                            CollisionManifold2D& manifold)
        {
            sf::Vector2f firstPoint;
            sf::Vector2f secondPoint;
            closestPointsOnSegments(first.first, first.second, second.first, second.second,
                                    firstPoint, secondPoint);
            const sf::Vector2f delta = secondPoint - firstPoint;
            const double distance = length(delta);
            const double radius = static_cast<double>(first.radius) + second.radius;
            if (!std::isfinite(distance) || distance > radius + ContactTolerance) return false;
            const sf::Vector2f centerDelta =
                multiply(second.first + second.second - first.first - first.second, 0.5);
            const sf::Vector2f normal = normalized(delta, normalized(centerDelta));
            const sf::Vector2f surfaceA = firstPoint + multiply(normal, first.radius);
            const sf::Vector2f surfaceB = secondPoint - multiply(normal, second.radius);
            return setManifold(normal, multiply(surfaceA + surfaceB, 0.5), radius - distance,
                               manifold);
        }

        bool capsulePolygon(const CapsuleGeometry2D& capsule, const PolygonGeometry2D& polygon,
                            CollisionManifold2D& manifold)
        {
            std::vector<sf::Vector2f> axes;
            axes.reserve(polygon.vertices.size() * 3u + 1u);
            const sf::Vector2f segment = capsule.second - capsule.first;
            if (lengthSquared(segment) > Epsilon)
                axes.push_back(normalized({segment.y, -segment.x}));
            for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
            {
                axes.push_back(outwardNormal(polygon, index));
                for (const sf::Vector2f endpoint : {capsule.first, capsule.second})
                {
                    const sf::Vector2f vertexAxis = polygon.vertices[index] - endpoint;
                    if (lengthSquared(vertexAxis) > Epsilon) axes.push_back(normalized(vertexAxis));
                }
            }

            sf::Vector2f polygonCenter = {0.f, 0.f};
            for (const sf::Vector2f vertex : polygon.vertices)
                polygonCenter += vertex;
            polygonCenter =
                multiply(polygonCenter, 1.0 / static_cast<double>(polygon.vertices.size()));
            const sf::Vector2f capsuleCenter = multiply(capsule.first + capsule.second, 0.5);
            const sf::Vector2f centerDelta = polygonCenter - capsuleCenter;
            double minimumOverlap = std::numeric_limits<double>::infinity();
            sf::Vector2f bestAxis = {1.f, 0.f};

            for (sf::Vector2f axis : axes)
            {
                if (dot(centerDelta, axis) < 0.0) axis = {-axis.x, -axis.y};
                double capsuleMin = 0.0;
                double capsuleMax = 0.0;
                double polygonMin = 0.0;
                double polygonMax = 0.0;
                projectCapsule(capsule, axis, capsuleMin, capsuleMax);
                projectPolygon(polygon, axis, polygonMin, polygonMax);
                double overlap = 0.0;
                if (!intervalPenetration(capsuleMin, capsuleMax, polygonMin, polygonMax, overlap))
                    return false;
                if (overlap < minimumOverlap)
                {
                    minimumOverlap = overlap;
                    bestAxis = axis;
                }
            }

            ColliderGeometry2D capsuleGeometry;
            capsuleGeometry.type = GeometryType2D::Capsule;
            capsuleGeometry.capsule = capsule;
            ColliderGeometry2D polygonGeometry;
            polygonGeometry.type = GeometryType2D::Polygon;
            polygonGeometry.polygon = polygon;
            const sf::Vector2f firstPoint = supportPoint(capsuleGeometry, bestAxis);
            const sf::Vector2f secondPoint =
                supportPoint(polygonGeometry, {-bestAxis.x, -bestAxis.y});
            return setManifold(bestAxis, multiply(firstPoint + secondPoint, 0.5), minimumOverlap,
                               manifold);
        }

        bool rayCircle(const CircleGeometry2D& circle, sf::Vector2f start, sf::Vector2f end,
                       GeometryRayHit2D& hit)
        {
            const sf::Vector2f movement = end - start;
            const sf::Vector2f offset = start - circle.center;
            const double a = lengthSquared(movement);
            const double radiusSquared = static_cast<double>(circle.radius) * circle.radius;
            if (lengthSquared(offset) <= radiusSquared + ContactTolerance)
            {
                hit = {start, {0.f, 0.f}, 0.f};
                return true;
            }
            if (a <= Epsilon) return false;
            const double b = 2.0 * dot(offset, movement);
            const double c = lengthSquared(offset) - radiusSquared;
            const double discriminant = b * b - 4.0 * a * c;
            if (!std::isfinite(discriminant) || discriminant < -ContactTolerance) return false;
            const double root = std::sqrt(std::max(0.0, discriminant));
            const double fraction = (-b - root) / (2.0 * a);
            if (fraction < -ContactTolerance || fraction > 1.0 + ContactTolerance) return false;
            const double clamped = std::clamp(fraction, 0.0, 1.0);
            hit.fraction = static_cast<float>(clamped);
            hit.point = start + multiply(movement, clamped);
            hit.normal = normalized(hit.point - circle.center);
            return finite(hit.point) && finite(hit.normal);
        }

        bool rayPolygon(const PolygonGeometry2D& polygon, sf::Vector2f start, sf::Vector2f end,
                        GeometryRayHit2D& hit)
        {
            if (pointInsideGeometry({GeometryType2D::Polygon, {}, {}, polygon, {}}, start))
            {
                hit = {start, {0.f, 0.f}, 0.f};
                return true;
            }
            const sf::Vector2f movement = end - start;
            if (lengthSquared(movement) <= Epsilon) return false;
            double enter = 0.0;
            double exit = 1.0;
            sf::Vector2f enterNormal = {0.f, 0.f};
            for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
            {
                const sf::Vector2f normal = outwardNormal(polygon, index);
                const double distance = dot(normal, start - polygon.vertices[index]);
                const double denominator = dot(normal, movement);
                if (std::fabs(denominator) <= Epsilon)
                {
                    if (distance > ContactTolerance) return false;
                    continue;
                }
                const double fraction = -distance / denominator;
                if (denominator < 0.0)
                {
                    if (fraction > enter)
                    {
                        enter = fraction;
                        enterNormal = normal;
                    }
                }
                else
                {
                    exit = std::min(exit, fraction);
                }
                if (enter - exit > ContactTolerance) return false;
            }
            if (enter < -ContactTolerance || enter > 1.0 + ContactTolerance) return false;
            hit.fraction = static_cast<float>(std::clamp(enter, 0.0, 1.0));
            hit.point = start + multiply(movement, hit.fraction);
            hit.normal = enterNormal;
            return true;
        }

        bool earlier(const GeometryRayHit2D& candidate, const GeometryRayHit2D& current,
                     bool hasCurrent)
        {
            return !hasCurrent || candidate.fraction < current.fraction;
        }

        bool rayCapsule(const CapsuleGeometry2D& capsule, sf::Vector2f start, sf::Vector2f end,
                        GeometryRayHit2D& hit)
        {
            ColliderGeometry2D capsuleGeometry;
            capsuleGeometry.type = GeometryType2D::Capsule;
            capsuleGeometry.capsule = capsule;
            if (pointInsideGeometry(capsuleGeometry, start))
            {
                hit = {start, {0.f, 0.f}, 0.f};
                return true;
            }

            bool found = false;
            GeometryRayHit2D best;
            for (const sf::Vector2f endpoint : {capsule.first, capsule.second})
            {
                GeometryRayHit2D candidate;
                if (rayCircle({endpoint, capsule.radius}, start, end, candidate) &&
                    earlier(candidate, best, found))
                {
                    best = candidate;
                    found = true;
                }
            }

            const sf::Vector2f segment = capsule.second - capsule.first;
            if (lengthSquared(segment) > Epsilon)
            {
                const sf::Vector2f perpendicular =
                    multiply(normalized({segment.y, -segment.x}), capsule.radius);
                PolygonGeometry2D middle{
                    {capsule.first + perpendicular, capsule.second + perpendicular,
                     capsule.second - perpendicular, capsule.first - perpendicular}};
                GeometryRayHit2D candidate;
                if (rayPolygon(middle, start, end, candidate) && earlier(candidate, best, found))
                {
                    best = candidate;
                    found = true;
                }
            }
            if (found) hit = best;
            return found;
        }

        bool expandedPolygonRaycast(const PolygonGeometry2D& polygon, sf::Vector2f start,
                                    sf::Vector2f end, float radius, GeometryRayHit2D& hit)
        {
            ColliderGeometry2D circleGeometry;
            makeCircleGeometry(start, radius, circleGeometry);
            ColliderGeometry2D polygonGeometry;
            polygonGeometry.type = GeometryType2D::Polygon;
            polygonGeometry.polygon = polygon;
            computeBounds(polygonGeometry);
            CollisionManifold2D initial;
            if (computeGeometryManifold(circleGeometry, polygonGeometry, initial))
            {
                hit = {supportPoint(polygonGeometry, {-initial.normal.x, -initial.normal.y}),
                       {-initial.normal.x, -initial.normal.y},
                       0.f};
                return true;
            }

            const sf::Vector2f movement = end - start;
            if (lengthSquared(movement) <= Epsilon) return false;
            bool found = false;
            GeometryRayHit2D best;

            for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
            {
                const sf::Vector2f first = polygon.vertices[index];
                const sf::Vector2f second =
                    polygon.vertices[(index + 1u) % polygon.vertices.size()];
                const sf::Vector2f normal = outwardNormal(polygon, index);
                const double startDistance = dot(normal, start - first) - radius;
                const double denominator = dot(normal, movement);
                if (startDistance > 0.0 && denominator < -Epsilon)
                {
                    const double fraction = -startDistance / denominator;
                    if (fraction >= 0.0 && fraction <= 1.0)
                    {
                        const sf::Vector2f center = start + multiply(movement, fraction);
                        const sf::Vector2f edge = second - first;
                        const double edgeParameter =
                            dot(center - first, edge) / std::max(Epsilon, lengthSquared(edge));
                        if (edgeParameter >= -ContactTolerance &&
                            edgeParameter <= 1.0 + ContactTolerance)
                        {
                            GeometryRayHit2D candidate;
                            candidate.fraction = static_cast<float>(fraction);
                            candidate.normal = normal;
                            candidate.point = center - multiply(normal, radius);
                            if (earlier(candidate, best, found))
                            {
                                best = candidate;
                                found = true;
                            }
                        }
                    }
                }

                GeometryRayHit2D vertexHit;
                if (rayCircle({first, radius}, start, end, vertexHit) &&
                    earlier(vertexHit, best, found))
                {
                    vertexHit.point = first;
                    best = vertexHit;
                    found = true;
                }
            }

            if (found) hit = best;
            return found;
        }

        PolygonGeometry2D circumscribedCapsule(const CapsuleGeometry2D& capsule)
        {
            constexpr std::size_t HalfSteps = 12u;
            const sf::Vector2f axis = normalized(capsule.second - capsule.first, {0.f, 1.f});
            const double axisAngle = std::atan2(axis.y, axis.x);
            const double step = Pi / static_cast<double>(HalfSteps);
            const double safeRadius = capsule.radius / std::cos(step * 0.5);
            PolygonGeometry2D result;
            result.vertices.reserve((HalfSteps + 1u) * 2u);
            for (std::size_t index = 0; index <= HalfSteps; ++index)
            {
                const double angle = axisAngle - Pi * 0.5 + step * static_cast<double>(index);
                result.vertices.push_back(
                    capsule.second +
                    vectorFromDoubles(std::cos(angle) * safeRadius, std::sin(angle) * safeRadius));
            }
            for (std::size_t index = 0; index <= HalfSteps; ++index)
            {
                const double angle = axisAngle + Pi * 0.5 + step * static_cast<double>(index);
                result.vertices.push_back(
                    capsule.first +
                    vectorFromDoubles(std::cos(angle) * safeRadius, std::sin(angle) * safeRadius));
            }
            return result;
        }

        bool sweptPolygons(const PolygonGeometry2D& moving, sf::Vector2f movement,
                           const PolygonGeometry2D& target, GeometryRayHit2D& hit)
        {
            CollisionManifold2D initial;
            ColliderGeometry2D movingGeometry;
            movingGeometry.type = GeometryType2D::Polygon;
            movingGeometry.polygon = moving;
            ColliderGeometry2D targetGeometry;
            targetGeometry.type = GeometryType2D::Polygon;
            targetGeometry.polygon = target;
            if (computeGeometryManifold(movingGeometry, targetGeometry, initial))
            {
                hit = {supportPoint(targetGeometry, {-initial.normal.x, -initial.normal.y}),
                       {-initial.normal.x, -initial.normal.y},
                       0.f};
                return true;
            }

            double enter = 0.0;
            double exit = 1.0;
            sf::Vector2f hitNormal = {0.f, 0.f};
            const std::array<const PolygonGeometry2D*, 2> polygons = {&moving, &target};
            for (const PolygonGeometry2D* polygon : polygons)
            {
                for (std::size_t index = 0; index < polygon->vertices.size(); ++index)
                {
                    const sf::Vector2f axis = outwardNormal(*polygon, index);
                    double movingMin = 0.0;
                    double movingMax = 0.0;
                    double targetMin = 0.0;
                    double targetMax = 0.0;
                    projectPolygon(moving, axis, movingMin, movingMax);
                    projectPolygon(target, axis, targetMin, targetMax);
                    const double velocity = dot(movement, axis);
                    if (std::fabs(velocity) <= Epsilon)
                    {
                        if (movingMax < targetMin || movingMin > targetMax) return false;
                        continue;
                    }
                    double axisEnter = 0.0;
                    double axisExit = 0.0;
                    sf::Vector2f axisNormal;
                    if (velocity > 0.0)
                    {
                        axisEnter = (targetMin - movingMax) / velocity;
                        axisExit = (targetMax - movingMin) / velocity;
                        axisNormal = {-axis.x, -axis.y};
                    }
                    else
                    {
                        axisEnter = (targetMax - movingMin) / velocity;
                        axisExit = (targetMin - movingMax) / velocity;
                        axisNormal = axis;
                    }
                    if (axisEnter > enter)
                    {
                        enter = axisEnter;
                        hitNormal = axisNormal;
                    }
                    exit = std::min(exit, axisExit);
                    if (enter - exit > ContactTolerance) return false;
                }
            }
            if (enter < -ContactTolerance || enter > 1.0 + ContactTolerance) return false;
            hit.fraction = static_cast<float>(std::clamp(enter, 0.0, 1.0));
            hit.normal = hitNormal;
            hit.point = supportPoint(targetGeometry, hit.normal);
            return true;
        }
    }

    bool buildColliderGeometry(const Collider2D& collider, ColliderGeometry2D& geometry)
    {
        ColliderGeometry2D result;
        if (const auto* box = dynamic_cast<const BoxCollider2D*>(&collider))
        {
            result.type = GeometryType2D::Polygon;
            const auto corners = box->corners();
            // Start with the local X face so equal-penetration contacts retain
            // the legacy box manifold's X-before-Y tie break.
            result.polygon.vertices = {corners[0], corners[3], corners[2], corners[1]};
        }
        else if (const auto* circle = dynamic_cast<const CircleCollider2D*>(&collider))
        {
            result.type = GeometryType2D::Circle;
            result.circle = {circle->center(), circle->worldRadius()};
        }
        else if (const auto* capsule = dynamic_cast<const CapsuleCollider2D*>(&collider))
        {
            result.type = GeometryType2D::Capsule;
            const auto segment = capsule->worldSegment();
            result.capsule = {segment[0], segment[1], capsule->worldRadius()};
        }
        else if (const auto* polygon = dynamic_cast<const ConvexPolygonCollider2D*>(&collider))
        {
            result.type = GeometryType2D::Polygon;
            result.polygon.vertices = polygon->worldVertices();
        }
        else
        {
            return false;
        }
        if (!computeBounds(result)) return false;
        geometry = std::move(result);
        return true;
    }

    bool makeCircleGeometry(sf::Vector2f center, float radius, ColliderGeometry2D& geometry)
    {
        if (!finite(center) || !std::isfinite(radius) || radius < 0.f) return false;
        ColliderGeometry2D result;
        result.type = GeometryType2D::Circle;
        result.circle = {center, radius};
        if (!computeBounds(result)) return false;
        geometry = result;
        return true;
    }

    bool makeBoxGeometry(sf::Vector2f center, sf::Vector2f size, float rotationDegrees,
                         ColliderGeometry2D& geometry)
    {
        if (!finite(center) || !finite(size) || !std::isfinite(rotationDegrees) || size.x <= 0.f ||
            size.y <= 0.f)
            return false;
        const double radians = static_cast<double>(rotationDegrees) * Pi / 180.0;
        const sf::Vector2f axisX = {static_cast<float>(std::cos(radians)),
                                    static_cast<float>(std::sin(radians))};
        const sf::Vector2f axisY = {-axisX.y, axisX.x};
        const sf::Vector2f halfX = multiply(axisX, size.x * 0.5);
        const sf::Vector2f halfY = multiply(axisY, size.y * 0.5);
        ColliderGeometry2D result;
        result.type = GeometryType2D::Polygon;
        result.polygon.vertices = {center - halfX - halfY, center + halfX - halfY,
                                   center + halfX + halfY, center - halfX + halfY};
        if (!computeBounds(result)) return false;
        geometry = std::move(result);
        return true;
    }

    bool makeCapsuleGeometry(sf::Vector2f center, float radius, float height, float rotationDegrees,
                             ColliderGeometry2D& geometry)
    {
        if (!finite(center) || !std::isfinite(radius) || !std::isfinite(height) ||
            !std::isfinite(rotationDegrees) || radius <= 0.f || height < 2.f * radius)
            return false;

        const double radians = static_cast<double>(rotationDegrees) * Pi / 180.0;
        const sf::Vector2f axis = {-static_cast<float>(std::sin(radians)),
                                   static_cast<float>(std::cos(radians))};
        const sf::Vector2f halfSegment = multiply(axis, height * 0.5 - radius);

        ColliderGeometry2D result;
        result.type = GeometryType2D::Capsule;
        result.capsule = {center - halfSegment, center + halfSegment, radius};
        if (!computeBounds(result)) return false;
        geometry = result;
        return true;
    }

    void translateGeometry(ColliderGeometry2D& geometry, sf::Vector2f offset)
    {
        if (!finite(offset)) return;
        if (geometry.type == GeometryType2D::Circle)
            geometry.circle.center += offset;
        else if (geometry.type == GeometryType2D::Capsule)
        {
            geometry.capsule.first += offset;
            geometry.capsule.second += offset;
        }
        else
        {
            for (sf::Vector2f& vertex : geometry.polygon.vertices)
                vertex += offset;
        }
        geometry.bounds.minimum += offset;
        geometry.bounds.maximum += offset;
    }

    bool aabbOverlaps(const Aabb2D& first, const Aabb2D& second)
    {
        return first.minimum.x <= second.maximum.x && first.maximum.x >= second.minimum.x &&
               first.minimum.y <= second.maximum.y && first.maximum.y >= second.minimum.y;
    }

    bool pointInsideGeometry(const ColliderGeometry2D& geometry, sf::Vector2f point)
    {
        if (!finite(point)) return false;
        if (geometry.type == GeometryType2D::Circle)
            return lengthSquared(point - geometry.circle.center) <=
                   static_cast<double>(geometry.circle.radius) * geometry.circle.radius +
                       ContactTolerance;
        if (geometry.type == GeometryType2D::Capsule)
        {
            const sf::Vector2f closest =
                closestPointOnSegment(point, geometry.capsule.first, geometry.capsule.second);
            return lengthSquared(point - closest) <=
                   static_cast<double>(geometry.capsule.radius) * geometry.capsule.radius +
                       ContactTolerance;
        }
        const PolygonGeometry2D& polygon = geometry.polygon;
        if (polygon.vertices.size() < 3u) return false;
        for (std::size_t index = 0; index < polygon.vertices.size(); ++index)
        {
            if (dot(outwardNormal(polygon, index), point - polygon.vertices[index]) >
                ContactTolerance)
                return false;
        }
        return true;
    }

    sf::Vector2f geometryCenter(const ColliderGeometry2D& geometry)
    {
        if (geometry.type == GeometryType2D::Circle) return geometry.circle.center;
        if (geometry.type == GeometryType2D::Capsule)
            return multiply(geometry.capsule.first + geometry.capsule.second, 0.5);
        sf::Vector2f center = {0.f, 0.f};
        for (const sf::Vector2f vertex : geometry.polygon.vertices)
            center += vertex;
        return geometry.polygon.vertices.empty()
                   ? center
                   : multiply(center, 1.0 / static_cast<double>(geometry.polygon.vertices.size()));
    }

    sf::Vector2f supportPoint(const ColliderGeometry2D& geometry, sf::Vector2f direction)
    {
        const sf::Vector2f unit = normalized(direction);
        if (geometry.type == GeometryType2D::Circle)
            return geometry.circle.center + multiply(unit, geometry.circle.radius);
        if (geometry.type == GeometryType2D::Capsule)
        {
            const sf::Vector2f endpoint =
                dot(geometry.capsule.first, unit) > dot(geometry.capsule.second, unit)
                    ? geometry.capsule.first
                    : geometry.capsule.second;
            return endpoint + multiply(unit, geometry.capsule.radius);
        }
        sf::Vector2f result = geometry.polygon.vertices.front();
        double best = dot(result, unit);
        for (const sf::Vector2f vertex : geometry.polygon.vertices)
        {
            const double projection = dot(vertex, unit);
            if (projection > best)
            {
                result = vertex;
                best = projection;
            }
        }
        return result;
    }

    bool computeGeometryManifold(const ColliderGeometry2D& first, const ColliderGeometry2D& second,
                                 CollisionManifold2D& manifold)
    {
        manifold = {};
        if (!aabbOverlaps(first.bounds, second.bounds)) return false;
        bool result = false;
        if (first.type == GeometryType2D::Circle && second.type == GeometryType2D::Circle)
            result = circleCircle(first.circle, second.circle, manifold);
        else if (first.type == GeometryType2D::Polygon && second.type == GeometryType2D::Polygon)
            result = polygonPolygon(first.polygon, second.polygon, manifold);
        else if (first.type == GeometryType2D::Circle && second.type == GeometryType2D::Polygon)
            result = circlePolygon(first.circle, second.polygon, manifold);
        else if (first.type == GeometryType2D::Polygon && second.type == GeometryType2D::Circle)
        {
            result = circlePolygon(second.circle, first.polygon, manifold);
            manifold.normal = {-manifold.normal.x, -manifold.normal.y};
        }
        else if (first.type == GeometryType2D::Capsule && second.type == GeometryType2D::Circle)
            result = capsuleCircle(first.capsule, second.circle, manifold);
        else if (first.type == GeometryType2D::Circle && second.type == GeometryType2D::Capsule)
        {
            result = capsuleCircle(second.capsule, first.circle, manifold);
            manifold.normal = {-manifold.normal.x, -manifold.normal.y};
        }
        else if (first.type == GeometryType2D::Capsule && second.type == GeometryType2D::Capsule)
            result = capsuleCapsule(first.capsule, second.capsule, manifold);
        else if (first.type == GeometryType2D::Capsule && second.type == GeometryType2D::Polygon)
            result = capsulePolygon(first.capsule, second.polygon, manifold);
        else if (first.type == GeometryType2D::Polygon && second.type == GeometryType2D::Capsule)
        {
            result = capsulePolygon(second.capsule, first.polygon, manifold);
            manifold.normal = {-manifold.normal.x, -manifold.normal.y};
        }
        return result;
    }

    bool raycastGeometry(const ColliderGeometry2D& geometry, sf::Vector2f start, sf::Vector2f end,
                         GeometryRayHit2D& hit)
    {
        hit = {};
        if (!finite(start) || !finite(end)) return false;
        if (geometry.type == GeometryType2D::Circle)
            return rayCircle(geometry.circle, start, end, hit);
        if (geometry.type == GeometryType2D::Capsule)
            return rayCapsule(geometry.capsule, start, end, hit);
        return rayPolygon(geometry.polygon, start, end, hit);
    }

    bool castCircleAgainstGeometry(sf::Vector2f start, sf::Vector2f end, float radius,
                                   const ColliderGeometry2D& target, GeometryRayHit2D& hit)
    {
        hit = {};
        if (!finite(start) || !finite(end) || !std::isfinite(radius) || radius < 0.f) return false;
        if (target.type == GeometryType2D::Circle)
        {
            CircleGeometry2D expanded = target.circle;
            expanded.radius += radius;
            if (!rayCircle(expanded, start, end, hit)) return false;
            const sf::Vector2f center = start + multiply(end - start, hit.fraction);
            hit.point = target.circle.center + multiply(hit.normal, target.circle.radius);
            if (hit.fraction == 0.f)
            {
                hit.normal = normalized(center - target.circle.center);
                hit.point = target.circle.center + multiply(hit.normal, target.circle.radius);
            }
            return true;
        }
        if (target.type == GeometryType2D::Capsule)
        {
            CapsuleGeometry2D expanded = target.capsule;
            expanded.radius += radius;
            if (!rayCapsule(expanded, start, end, hit)) return false;
            const sf::Vector2f center = start + multiply(end - start, hit.fraction);
            const sf::Vector2f closest =
                closestPointOnSegment(center, target.capsule.first, target.capsule.second);
            hit.normal = normalized(center - closest);
            hit.point = closest + multiply(hit.normal, target.capsule.radius);
            return true;
        }
        return expandedPolygonRaycast(target.polygon, start, end, radius, hit);
    }

    bool castPolygonAgainstGeometry(const ColliderGeometry2D& polygon, sf::Vector2f movement,
                                    const ColliderGeometry2D& target, GeometryRayHit2D& hit)
    {
        hit = {};
        if (polygon.type != GeometryType2D::Polygon || !finite(movement)) return false;
        if (target.type == GeometryType2D::Polygon)
            return sweptPolygons(polygon.polygon, movement, target.polygon, hit);
        if (target.type == GeometryType2D::Circle)
        {
            ColliderGeometry2D staticPolygon = polygon;
            GeometryRayHit2D inverse;
            if (!castCircleAgainstGeometry(target.circle.center, target.circle.center - movement,
                                           target.circle.radius, staticPolygon, inverse))
                return false;
            hit.fraction = inverse.fraction;
            hit.normal = {-inverse.normal.x, -inverse.normal.y};
            hit.point = target.circle.center + multiply(hit.normal, target.circle.radius);
            return true;
        }
        const PolygonGeometry2D approximation = circumscribedCapsule(target.capsule);
        if (!sweptPolygons(polygon.polygon, movement, approximation, hit)) return false;
        hit.point = supportPoint(target, hit.normal);
        return true;
    }

    bool castCapsuleAgainstGeometry(const ColliderGeometry2D& capsule, sf::Vector2f movement,
                                    const ColliderGeometry2D& target, GeometryRayHit2D& hit)
    {
        hit = {};
        if (capsule.type != GeometryType2D::Capsule || !finite(movement)) return false;

        ColliderGeometry2D approximation;
        approximation.type = GeometryType2D::Polygon;
        approximation.polygon = circumscribedCapsule(capsule.capsule);
        if (!computeBounds(approximation)) return false;
        return castPolygonAgainstGeometry(approximation, movement, target, hit);
    }
}

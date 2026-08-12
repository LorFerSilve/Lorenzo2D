#include "PhysicsTestSupport.hpp"

#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>

#include <limits>

namespace
{
    using namespace l2d::test::physics;

    l2d::BoxCollider2D& addBox(l2d::Scene& scene, const char* name, sf::Vector2f center,
                               sf::Vector2f size)
    {
        l2d::GameObject& object = scene.createGameObject(name);
        object.transform.setPosition(center);
        return object.addComponent<l2d::BoxCollider2D>(size);
    }

    void testRaycastsAreStableAndHandleDegenerateSegments()
    {
        l2d::Scene scene;
        l2d::BoxCollider2D& first = addBox(scene, "first", {-20.f, -10.f}, {10.f, 20.f});
        l2d::BoxCollider2D& second = addBox(scene, "second", {-20.f, -10.f}, {10.f, 20.f});
        addBox(scene, "far", {30.f, -10.f}, {10.f, 20.f});

        l2d::PhysicsQueryContext2D queries(scene);
        const auto hits = queries.raycastAll({-100.f, -10.f}, {100.f, -10.f});
        L2D_REQUIRE_EQUAL(hits.size(), 3u);
        L2D_REQUIRE_EQUAL(hits[0].colliderId, first.id());
        L2D_REQUIRE_EQUAL(hits[1].colliderId, second.id());
        L2D_REQUIRE(hits[0].distance <= hits[2].distance);
        L2D_REQUIRE_APPROX(hits[0].fraction, 0.375f, 0.001f);
        L2D_REQUIRE_APPROX(hits[0].normal, sf::Vector2f(-1.f, 0.f), 0.001f);

        const auto inside = queries.raycast({-20.f, -10.f}, {-20.f, -10.f});
        L2D_REQUIRE(inside.has_value());
        L2D_REQUIRE_APPROX(inside->fraction, 0.f, 0.0001f);
        L2D_REQUIRE_APPROX(inside->normal, sf::Vector2f(0.f, 0.f), 0.0001f);

        const auto miss = queries.raycast({-100.f, 100.f}, {-100.f, 100.f});
        L2D_REQUIRE(!miss.has_value());
    }

    void testFiltersSensorsAndIgnoredObjects()
    {
        l2d::Scene scene;
        l2d::BoxCollider2D& solid = addBox(scene, "solid", {0.f, 0.f}, {10.f, 10.f});
        solid.setFilter({2u, 0xffffffffu});
        l2d::BoxCollider2D& sensor = addBox(scene, "sensor", {20.f, 0.f}, {10.f, 10.f});
        sensor.setFilter({4u, 0xffffffffu});
        sensor.setSensor(true);

        l2d::PhysicsQueryContext2D queries(scene);
        L2D_REQUIRE_EQUAL(queries.raycastAll({-20.f, 0.f}, {40.f, 0.f}).size(), 1u);

        l2d::PhysicsQueryFilter2D sensorFilter;
        sensorFilter.categoryMask = 4u;
        sensorFilter.includeSensors = true;
        const auto sensorHit = queries.raycast({-20.f, 0.f}, {40.f, 0.f}, sensorFilter);
        L2D_REQUIRE(sensorHit.has_value());
        L2D_REQUIRE(sensorHit->sensor);
        L2D_REQUIRE_EQUAL(sensorHit->colliderId, sensor.id());

        l2d::PhysicsQueryFilter2D ignoredFilter;
        ignoredFilter.ignoredObject = solid.owner()->id();
        L2D_REQUIRE(!queries.raycast({-20.f, 0.f}, {15.f, 0.f}, ignoredFilter).has_value());

        l2d::PhysicsQueryFilter2D emptyFilter;
        emptyFilter.categoryMask = 0u;
        emptyFilter.includeSensors = true;
        L2D_REQUIRE(queries.pointQuery({0.f, 0.f}, emptyFilter).empty());
    }

    void testTangencyAndRaycastsAgainstCharacterShapes()
    {
        l2d::Scene scene;
        l2d::GameObject& circleObject = scene.createGameObject("circle");
        l2d::CircleCollider2D& circle = circleObject.addComponent<l2d::CircleCollider2D>(10.f);

        l2d::GameObject& capsuleObject = scene.createGameObject("capsule");
        capsuleObject.transform.setPosition({40.f, 0.f});
        l2d::CapsuleCollider2D& capsule =
            capsuleObject.addComponent<l2d::CapsuleCollider2D>(5.f, 30.f);

        l2d::GameObject& polygonObject = scene.createGameObject("polygon");
        polygonObject.transform.setPosition({80.f, 0.f});
        l2d::ConvexPolygonCollider2D& polygon =
            polygonObject.addComponent<l2d::ConvexPolygonCollider2D>(
                std::vector<sf::Vector2f>{{-8.f, 8.f}, {0.f, -8.f}, {8.f, 8.f}});

        l2d::PhysicsQueryContext2D queries(scene);
        const auto tangent = queries.raycast({-20.f, -10.f}, {20.f, -10.f});
        L2D_REQUIRE(tangent.has_value());
        L2D_REQUIRE_EQUAL(tangent->colliderId, circle.id());
        L2D_REQUIRE_APPROX(tangent->fraction, 0.5f, 0.001f);
        L2D_REQUIRE_APPROX(tangent->normal, sf::Vector2f(0.f, -1.f), 0.001f);

        const auto capsuleHit = queries.raycast({20.f, 0.f}, {60.f, 0.f});
        L2D_REQUIRE(capsuleHit.has_value());
        L2D_REQUIRE_EQUAL(capsuleHit->colliderId, capsule.id());
        const auto polygonHit = queries.raycast({65.f, 0.f}, {95.f, 0.f});
        L2D_REQUIRE(polygonHit.has_value());
        L2D_REQUIRE_EQUAL(polygonHit->colliderId, polygon.id());
    }

    void testPointAndOverlapQueriesSupportAllShapes()
    {
        l2d::Scene scene;
        l2d::GameObject& capsuleObject = scene.createGameObject("capsule");
        capsuleObject.transform.setPosition({-30.f, 10.f});
        capsuleObject.transform.setRotation(30.f);
        l2d::CapsuleCollider2D& capsule =
            capsuleObject.addComponent<l2d::CapsuleCollider2D>(5.f, 30.f);

        l2d::GameObject& slopeObject = scene.createGameObject("slope");
        slopeObject.transform.setPosition({20.f, 0.f});
        slopeObject.transform.setRotation(-15.f);
        l2d::ConvexPolygonCollider2D& slope =
            slopeObject.addComponent<l2d::ConvexPolygonCollider2D>(std::vector<sf::Vector2f>{
                {-15.f, 10.f}, {15.f, 10.f}, {15.f, 0.f}, {-15.f, -10.f}});

        l2d::PhysicsQueryContext2D queries(scene);
        L2D_REQUIRE_EQUAL(queries.proxyCount(), 2u);
        L2D_REQUIRE_EQUAL(queries.pointQuery(capsule.center()).size(), 1u);
        L2D_REQUIRE_EQUAL(queries.pointQuery(slope.center()).size(), 1u);
        L2D_REQUIRE(!queries.overlapCircle({-30.f, 10.f}, 2.f).empty());
        L2D_REQUIRE(!queries.overlapBox({20.f, 0.f}, {8.f, 8.f}, 45.f).empty());
        const auto capsuleOverlap = queries.overlapCapsule({-30.f, 10.f}, 3.f, 20.f, 30.f);
        L2D_REQUIRE(!capsuleOverlap.empty());
        L2D_REQUIRE(capsuleOverlap.front().penetration > 0.f);
        L2D_REQUIRE(queries.overlapBox({200.f, 200.f}, {8.f, 8.f}).empty());
    }

    void testShapeCastsCannotTunnelThroughThinGeometry()
    {
        l2d::Scene scene;
        l2d::BoxCollider2D& wall = addBox(scene, "thin wall", {0.f, 0.f}, {2.f, 100.f});
        l2d::PhysicsWorld2D world(zeroGravityConfig());

        const auto circleHit = world.castCircle(scene, {-100.f, 0.f}, {100.f, 0.f}, 5.f);
        L2D_REQUIRE(circleHit.has_value());
        L2D_REQUIRE_EQUAL(circleHit->colliderId, wall.id());
        L2D_REQUIRE(circleHit->fraction > 0.45f && circleHit->fraction < 0.5f);
        L2D_REQUIRE_APPROX(circleHit->normal, sf::Vector2f(-1.f, 0.f), 0.001f);

        const auto boxHit = world.castBox(scene, {-100.f, 30.f}, {100.f, 30.f}, {10.f, 10.f}, 25.f);
        L2D_REQUIRE(boxHit.has_value());
        L2D_REQUIRE_EQUAL(boxHit->colliderId, wall.id());
        L2D_REQUIRE(boxHit->fraction > 0.4f && boxHit->fraction < 0.5f);
        L2D_REQUIRE_APPROX(boxHit->normal, sf::Vector2f(-1.f, 0.f), 0.001f);

        const auto capsuleHit =
            world.castCapsule(scene, {-100.f, -30.f}, {100.f, -30.f}, 5.f, 20.f);
        L2D_REQUIRE(capsuleHit.has_value());
        L2D_REQUIRE_EQUAL(capsuleHit->colliderId, wall.id());
        L2D_REQUIRE(capsuleHit->fraction > 0.4f && capsuleHit->fraction < 0.5f);
        L2D_REQUIRE_APPROX(capsuleHit->normal, sf::Vector2f(-1.f, 0.f), 0.02f);
    }

    void testQuerySnapshotsAndContactStateStayIndependent()
    {
        l2d::Scene scene;
        BoxBody first = createBox(scene, "first", {0.f, 0.f}, {20.f, 20.f}, true);
        BoxBody second = createBox(scene, "second", {10.f, 0.f}, {20.f, 20.f}, false);
        l2d::PhysicsWorld2D world(zeroGravityConfig());
        fixedStep(scene, world);
        const auto contactsBefore = world.contacts();
        const bool firstColliding = first.collider.isColliding();
        const bool secondColliding = second.collider.isColliding();

        l2d::PhysicsQueryContext2D snapshot(scene);
        second.object.transform.setPosition({1000.f, 0.f});
        L2D_REQUIRE(snapshot.pointQuery({25.f, 10.f}).size() == 1u);
        L2D_REQUIRE(l2d::PhysicsQueryContext2D(scene).pointQuery({25.f, 10.f}).empty());

        (void)snapshot.raycastAll({-100.f, 10.f}, {100.f, 10.f});
        (void)snapshot.overlapCircle({10.f, 10.f}, 50.f);
        requireEquivalentContacts(contactsBefore, world.contacts());
        L2D_REQUIRE_EQUAL(first.collider.isColliding(), firstColliding);
        L2D_REQUIRE_EQUAL(second.collider.isColliding(), secondColliding);
    }

    void testNewColliderManifoldsAndPolygonValidation()
    {
        l2d::CapsuleCollider2D capsule(5.f, 30.f);
        l2d::BoxCollider2D box({20.f, 20.f});
        l2d::ConvexPolygonCollider2D polygon(
            {{-10.f, -10.f}, {10.f, -10.f}, {10.f, 10.f}, {-10.f, 10.f}});
        l2d::CircleCollider2D circle(8.f);
        l2d::CollisionManifold2D manifold;
        L2D_REQUIRE(l2d::computeCollisionManifold(capsule, box, manifold));
        L2D_REQUIRE(l2d::computeCollisionManifold(capsule, polygon, manifold));
        L2D_REQUIRE(l2d::computeCollisionManifold(circle, polygon, manifold));
        L2D_REQUIRE_APPROX(manifold.penetration, 18.f, 0.001f);
        L2D_REQUIRE(l2d::computeCollisionManifold(box, polygon, manifold));
        L2D_REQUIRE(l2d::computeCollisionManifold(capsule, circle, manifold));

        const auto original = polygon.vertices();
        L2D_REQUIRE(
            !polygon.setVertices({{0.f, 0.f}, {10.f, 0.f}, {5.f, 2.f}, {10.f, 10.f}, {0.f, 10.f}}));
        L2D_REQUIRE_EQUAL(polygon.vertices().size(), original.size());
        L2D_REQUIRE(!l2d::ConvexPolygonCollider2D::isValidVertices(
            {{0.f, 0.f}, {std::numeric_limits<float>::infinity(), 0.f}, {0.f, 1.f}}));

        L2D_REQUIRE(polygon.setVertices({{0.f, 0.f}, {0.f, 10.f}, {10.f, 10.f}, {10.f, 0.f}}));
        const auto& normalized = polygon.vertices();
        const float signedTurn =
            (normalized[1].x - normalized[0].x) * (normalized[2].y - normalized[1].y) -
            (normalized[1].y - normalized[0].y) * (normalized[2].x - normalized[1].x);
        L2D_REQUIRE(signedTurn > 0.f);
    }

    void testNewCollidersParticipateInWorldSimulation()
    {
        l2d::Scene scene;
        l2d::GameObject& character = scene.createGameObject("character");
        l2d::RigidBody2D& body = character.addComponent<l2d::RigidBody2D>();
        body.setBodyType(l2d::BodyType2D::Dynamic);
        l2d::CapsuleCollider2D& capsule = character.addComponent<l2d::CapsuleCollider2D>(5.f, 20.f);

        l2d::GameObject& slope = scene.createGameObject("slope");
        l2d::ConvexPolygonCollider2D& polygon = slope.addComponent<l2d::ConvexPolygonCollider2D>(
            std::vector<sf::Vector2f>{{-20.f, 5.f}, {20.f, 5.f}, {20.f, -5.f}, {-20.f, -5.f}});

        l2d::PhysicsWorld2D world(zeroGravityConfig());
        fixedStep(scene, world);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1u);
        L2D_REQUIRE_EQUAL(world.contacts()[0].firstColliderType, l2d::ColliderType::Capsule);
        L2D_REQUIRE_EQUAL(world.contacts()[0].secondColliderType, l2d::ColliderType::ConvexPolygon);
        L2D_REQUIRE(capsule.isColliding());
        L2D_REQUIRE(polygon.isColliding());
    }

    void testInvalidQueriesFailClosed()
    {
        l2d::Scene scene;
        addBox(scene, "box", {0.f, 0.f}, {10.f, 10.f});
        l2d::PhysicsQueryContext2D queries(scene);
        const float invalid = std::numeric_limits<float>::quiet_NaN();
        L2D_REQUIRE(queries.raycastAll({invalid, 0.f}, {0.f, 0.f}).empty());
        L2D_REQUIRE(queries.overlapCircle({0.f, 0.f}, -1.f).empty());
        L2D_REQUIRE(queries.overlapBox({0.f, 0.f}, {0.f, 10.f}).empty());
        L2D_REQUIRE(!queries.castCircle({0.f, 0.f}, {10.f, 0.f}, invalid).has_value());
        L2D_REQUIRE(!queries.castBox({0.f, 0.f}, {10.f, 0.f}, {10.f, invalid}).has_value());
        L2D_REQUIRE(queries.overlapCapsule({0.f, 0.f}, 5.f, 9.f).empty());
        L2D_REQUIRE(!queries.castCapsule({0.f, 0.f}, {10.f, 0.f}, 5.f, 9.f).has_value());
    }
}

int main()
{
    int failures = 0;
    runTest("raycasts are stable and handle degenerate segments",
            testRaycastsAreStableAndHandleDegenerateSegments, failures);
    runTest("query filters cover sensors and ignored objects", testFiltersSensorsAndIgnoredObjects,
            failures);
    runTest("tangency and character-shape raycasts", testTangencyAndRaycastsAgainstCharacterShapes,
            failures);
    runTest("point and overlap queries support all shapes",
            testPointAndOverlapQueriesSupportAllShapes, failures);
    runTest("shape casts cannot tunnel through thin geometry",
            testShapeCastsCannotTunnelThroughThinGeometry, failures);
    runTest("query snapshots do not mutate contact state",
            testQuerySnapshotsAndContactStateStayIndependent, failures);
    runTest("new collider manifolds and polygon validation",
            testNewColliderManifoldsAndPolygonValidation, failures);
    runTest("new colliders participate in world simulation",
            testNewCollidersParticipateInWorldSimulation, failures);
    runTest("invalid queries fail closed", testInvalidQueriesFailClosed, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics query test(s) failed.\n";
        return 1;
    }
    std::cout << "All Lorenzo2D physics query tests passed.\n";
    return 0;
}

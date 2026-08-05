#include "PhysicsTestSupport.hpp"

namespace
{
    using namespace l2d::test::physics;

    void testMergedTileMapColliderKeepsSeamContactContinuous()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({10.f, 10.f});
        tileMap.loadFromLayout(scene, {"####"}, '#', "Ground");

        const l2d::TileMapBuildStats& buildStats = tileMap.buildStats();
        L2D_REQUIRE_EQUAL(buildStats.solidTileCount, 4);
        L2D_REQUIRE_EQUAL(buildStats.collisionRectangleCount, 1);

        l2d::GameObject* ground = nullptr;

        for (const auto& gameObject : scene.gameObjects())
        {
            if (gameObject->getComponent<l2d::BoxCollider2D>() != nullptr)
            {
                L2D_REQUIRE_EQUAL(ground, nullptr);
                ground = gameObject.get();
            }
        }

        L2D_REQUIRE(ground != nullptr);

        l2d::PhysicsWorld2D world(zeroGravityConfig());
        BoxBody probe =
            createBox(scene, "SeamProbe", {4.f, 4.f}, {2.f, 2.f}, true, l2d::BodyType2D::Kinematic);
        probe.collider.setSensor(true);

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.broadPhaseStats().proxyCount,
                          buildStats.collisionRectangleCount + 1);
        L2D_REQUIRE(world.broadPhaseStats().proxyCount < buildStats.solidTileCount + 1);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.isTouching(probe.object.id(), ground->id()));
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);

        // This probe overlaps the old boundary between the first two tiles.
        // A merged collider keeps the same contact alive through that seam.
        probe.object.transform.setPosition({9.f, 4.f});
        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.isTouching(probe.object.id(), ground->id()));
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Stay);

        probe.object.transform.setPosition({14.f, 4.f});
        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.isTouching(probe.object.id(), ground->id()));
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Stay);
    }

    void testMergedTileMapCollidersPreserveIrregularHole()
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.setTileSize({10.f, 10.f});
        tileMap.loadFromLayout(scene, {"###", "#.#", "###"}, '#', "Ring");

        const l2d::TileMapBuildStats& buildStats = tileMap.buildStats();
        L2D_REQUIRE_EQUAL(buildStats.solidTileCount, 8);
        L2D_REQUIRE_EQUAL(buildStats.collisionRectangleCount, 4);

        l2d::PhysicsWorld2D world(zeroGravityConfig());
        BoxBody probe = createBox(scene, "HoleProbe", {14.f, 14.f}, {2.f, 2.f}, true,
                                  l2d::BodyType2D::Kinematic);
        probe.collider.setSensor(true);

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.broadPhaseStats().proxyCount,
                          buildStats.collisionRectangleCount + 1);
        L2D_REQUIRE(world.broadPhaseStats().proxyCount < buildStats.solidTileCount + 1);
        L2D_REQUIRE(world.contacts().empty());

        // Moving just one tile left enters the irregular ring's solid wall.
        probe.object.transform.setPosition({4.f, 14.f});
        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.contacts()[0].sensor);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
    }

}

int main()
{
    int failures = 0;

    runTest("merged tile-map collider keeps seam contact continuous",
            testMergedTileMapColliderKeepsSeamContactContinuous, failures);
    runTest("merged tile-map colliders preserve irregular hole",
            testMergedTileMapCollidersPreserveIrregularHole, failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics tile-map test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics tile-map tests passed.\n";
    return 0;
}

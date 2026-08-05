#include "PhysicsTestSupport.hpp"

namespace
{
    using namespace l2d::test::physics;

    void testUniformGridNegativeBoundaryTangency()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::UniformGrid;
        config.broadPhaseCellSize = 10.f;
        config.broadPhaseMaxCellsPerProxy = 16;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        BoxBody mover = createBox(scene, "NegativeBoundaryMover", {-20.f, -5.f}, {10.f, 10.f}, true,
                                  l2d::BodyType2D::Kinematic);
        BoxBody target =
            createBox(scene, "NegativeBoundaryTarget", {-10.f, -5.f}, {10.f, 10.f}, false);
        mover.collider.setSensor(true);
        target.collider.setSensor(true);

        fixedStep(scene, world);

        const l2d::PhysicsBroadPhaseStats2D& stats = world.broadPhaseStats();
        L2D_REQUIRE_EQUAL(stats.proxyCount, 2);
        L2D_REQUIRE_EQUAL(stats.occupiedCellCount, 6);
        L2D_REQUIRE_EQUAL(stats.fallbackProxyCount, 0);
        L2D_REQUIRE_EQUAL(stats.bruteForcePairCount, 1);
        L2D_REQUIRE_EQUAL(stats.candidatePairCount, 1);
        L2D_REQUIRE_EQUAL(stats.narrowPhaseTestCount, 1);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.contacts()[0].sensor);
        L2D_REQUIRE_APPROX(world.contacts()[0].manifold.penetration, 0.f,
                           kPhysicsComparisonEpsilon);
        L2D_REQUIRE(world.isTouching(mover.object.id(), target.object.id()));
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);
    }

    void testUniformGridDeduplicatesMultiCellPairs()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::UniformGrid;
        config.broadPhaseCellSize = 10.f;
        config.broadPhaseMaxCellsPerProxy = 64;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        BoxBody first = createBox(scene, "FirstMultiCellBody", {0.f, 0.f}, {25.f, 25.f}, true,
                                  l2d::BodyType2D::Kinematic);
        BoxBody second = createBox(scene, "SecondMultiCellBody", {5.f, 5.f}, {25.f, 25.f}, true,
                                   l2d::BodyType2D::Kinematic);
        first.collider.setSensor(true);
        second.collider.setSensor(true);

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.broadPhaseStats().proxyCount, 2);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().occupiedCellCount, 16);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().fallbackProxyCount, 0);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().bruteForcePairCount, 1);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().candidatePairCount, 1);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().narrowPhaseTestCount, 1);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Begin);

        fixedStep(scene, world);

        L2D_REQUIRE_EQUAL(world.broadPhaseStats().candidatePairCount, 1);
        L2D_REQUIRE_EQUAL(world.broadPhaseStats().narrowPhaseTestCount, 1);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents().size(), 1);
        L2D_REQUIRE_EQUAL(world.contactEvents()[0].phase, l2d::PhysicsContactPhase2D::Stay);
    }

    void testUniformGridFallbackAndStaticSuppression()
    {
        const float maximum = std::numeric_limits<float>::max();

        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::UniformGrid;
        config.broadPhaseCellSize = 10.f;
        config.broadPhaseMaxCellsPerProxy = 4;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        BoxBody oversized = createBox(scene, "OversizedMover", {-5.f, -5.f}, {25.f, 25.f}, true,
                                      l2d::BodyType2D::Kinematic);
        BoxBody firstStatic =
            createBox(scene, "FirstOverlappingStatic", {0.f, 0.f}, {2.f, 2.f}, false);
        BoxBody secondStatic =
            createBox(scene, "SecondOverlappingStatic", {1.f, 1.f}, {2.f, 2.f}, false);
        BoxBody invalid = createBox(scene, "InvalidBoundsMover", {maximum * 0.75f, 0.f}, {2.f, 2.f},
                                    true, l2d::BodyType2D::Kinematic);
        invalid.collider.setOffset({maximum * 0.75f, 0.f});
        CircleBody extreme = createCircle(scene, "ExtremeFiniteMover", {maximum * 0.25f, 0.f}, 0.f,
                                          true, l2d::BodyType2D::Kinematic);

        oversized.collider.setSensor(true);
        firstStatic.collider.setSensor(true);
        secondStatic.collider.setSensor(true);
        invalid.collider.setSensor(true);
        extreme.collider.setSensor(true);

        fixedStep(scene, world);

        const l2d::PhysicsBroadPhaseStats2D& stats = world.broadPhaseStats();
        L2D_REQUIRE_EQUAL(stats.proxyCount, 5);
        L2D_REQUIRE_EQUAL(stats.occupiedCellCount, 1);
        L2D_REQUIRE_EQUAL(stats.fallbackProxyCount, 3);
        L2D_REQUIRE_EQUAL(stats.bruteForcePairCount, 9);
        L2D_REQUIRE_EQUAL(stats.candidatePairCount, 6);
        L2D_REQUIRE_EQUAL(stats.narrowPhaseTestCount, 6);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 2);
        L2D_REQUIRE(world.isTouching(oversized.object.id(), firstStatic.object.id()));
        L2D_REQUIRE(world.isTouching(oversized.object.id(), secondStatic.object.id()));
        L2D_REQUIRE(!world.isTouching(firstStatic.object.id(), secondStatic.object.id()));
        L2D_REQUIRE(!invalid.collider.isColliding());
        L2D_REQUIRE(!extreme.collider.isColliding());
    }

    void testUniformGridMatchesBruteForceContactsAndEvents()
    {
        l2d::PhysicsWorld2DConfig gridConfig = zeroGravityConfig();
        gridConfig.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::UniformGrid;
        gridConfig.broadPhaseCellSize = 8.f;
        gridConfig.broadPhaseMaxCellsPerProxy = 16;

        l2d::PhysicsWorld2DConfig bruteForceConfig = gridConfig;
        bruteForceConfig.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::BruteForce;

        l2d::Scene scene;
        l2d::PhysicsWorld2D gridWorld(gridConfig);
        l2d::PhysicsWorld2D bruteForceWorld(bruteForceConfig);

        CircleBody negativeMover = createCircle(scene, "NegativeMover", {-10.f, -2.f}, 2.f, true,
                                                l2d::BodyType2D::Kinematic);
        BoxBody negativeTarget =
            createBox(scene, "NegativeTarget", {-6.f, -2.f}, {4.f, 4.f}, false);
        BoxBody centerMover = createBox(scene, "CenterMover", {12.f, 12.f}, {6.f, 6.f}, true,
                                        l2d::BodyType2D::Kinematic);
        CircleBody centerTarget = createCircle(scene, "CenterTarget", {14.f, 14.f}, 2.f, false);
        CircleBody farMover =
            createCircle(scene, "FarMover", {64.f, -34.f}, 1.f, true, l2d::BodyType2D::Kinematic);
        BoxBody farTarget = createBox(scene, "FarTarget", {64.f, -34.f}, {2.f, 2.f}, false);
        BoxBody firstStatic = createBox(scene, "FirstStaticOnly", {30.f, 30.f}, {4.f, 4.f}, false);
        BoxBody secondStatic =
            createBox(scene, "SecondStaticOnly", {31.f, 31.f}, {4.f, 4.f}, false);
        BoxBody filteredMover = createBox(scene, "FilteredMover", {-40.f, 24.f}, {3.f, 3.f}, true,
                                          l2d::BodyType2D::Kinematic);
        BoxBody filteredTarget =
            createBox(scene, "FilteredTarget", {-39.f, 24.f}, {3.f, 3.f}, false);

        negativeMover.collider.setSensor(true);
        negativeTarget.collider.setSensor(true);
        centerMover.collider.setSensor(true);
        centerTarget.collider.setSensor(true);
        farMover.collider.setSensor(true);
        farTarget.collider.setSensor(true);
        firstStatic.collider.setSensor(true);
        secondStatic.collider.setSensor(true);
        filteredMover.collider.setSensor(true);
        filteredTarget.collider.setSensor(true);
        filteredMover.collider.setFilter({2u, 0u});

        fixedStep(scene, gridWorld);
        fixedStep(scene, bruteForceWorld);

        requireEquivalentContacts(gridWorld.contacts(), bruteForceWorld.contacts());
        requireEquivalentContactEvents(gridWorld.contactEvents(), bruteForceWorld.contactEvents());
        L2D_REQUIRE_EQUAL(gridWorld.contacts().size(), 3);
        L2D_REQUIRE_EQUAL(gridWorld.broadPhaseStats().proxyCount, 10);
        L2D_REQUIRE_EQUAL(gridWorld.broadPhaseStats().fallbackProxyCount, 0);
        L2D_REQUIRE_EQUAL(gridWorld.broadPhaseStats().bruteForcePairCount, 30);
        L2D_REQUIRE_EQUAL(gridWorld.broadPhaseStats().candidatePairCount, 4);
        L2D_REQUIRE_EQUAL(gridWorld.broadPhaseStats().narrowPhaseTestCount, 3);
        L2D_REQUIRE_EQUAL(bruteForceWorld.broadPhaseStats().bruteForcePairCount, 30);
        L2D_REQUIRE_EQUAL(bruteForceWorld.broadPhaseStats().candidatePairCount, 30);
        L2D_REQUIRE_EQUAL(bruteForceWorld.broadPhaseStats().occupiedCellCount, 0);
        L2D_REQUIRE_EQUAL(bruteForceWorld.broadPhaseStats().fallbackProxyCount, 0);

        for (std::size_t index = 1; index < gridWorld.contacts().size(); ++index)
        {
            const l2d::PhysicsContact2D& previous = gridWorld.contacts()[index - 1];
            const l2d::PhysicsContact2D& current = gridWorld.contacts()[index];

            L2D_REQUIRE(std::tie(previous.firstObjectId, previous.secondObjectId) <
                        std::tie(current.firstObjectId, current.secondObjectId));
        }

        fixedStep(scene, gridWorld);
        fixedStep(scene, bruteForceWorld);

        requireEquivalentContacts(gridWorld.contacts(), bruteForceWorld.contacts());
        requireEquivalentContactEvents(gridWorld.contactEvents(), bruteForceWorld.contactEvents());
        L2D_REQUIRE(
            containsContactPhase(gridWorld.contactEvents(), l2d::PhysicsContactPhase2D::Stay));

        negativeMover.object.transform.setPosition({-100.f, -100.f});
        fixedStep(scene, gridWorld);
        fixedStep(scene, bruteForceWorld);

        requireEquivalentContacts(gridWorld.contacts(), bruteForceWorld.contacts());
        requireEquivalentContactEvents(gridWorld.contactEvents(), bruteForceWorld.contactEvents());
        L2D_REQUIRE_EQUAL(gridWorld.contacts().size(), 2);
        L2D_REQUIRE(
            containsContactPhase(gridWorld.contactEvents(), l2d::PhysicsContactPhase2D::End));
        L2D_REQUIRE(
            containsContactPhase(gridWorld.contactEvents(), l2d::PhysicsContactPhase2D::Stay));
    }

    void testUniformGridSparseTelemetryReduction()
    {
        l2d::PhysicsWorld2DConfig config = zeroGravityConfig();
        config.broadPhaseMode = l2d::PhysicsBroadPhaseMode2D::UniformGrid;
        config.broadPhaseCellSize = 10.f;
        config.broadPhaseMaxCellsPerProxy = 16;

        l2d::Scene scene;
        l2d::PhysicsWorld2D world(config);
        BoxBody mover = createBox(scene, "SparseMover", {0.f, 0.f}, {1.f, 1.f}, true,
                                  l2d::BodyType2D::Kinematic);
        BoxBody nearStatic = createBox(scene, "NearStatic", {0.5f, 0.f}, {1.f, 1.f}, false);
        mover.collider.setSensor(true);
        nearStatic.collider.setSensor(true);

        for (std::size_t index = 1; index <= 127; ++index)
        {
            const float x = 100.f + static_cast<float>(index % 16) * 20.f;
            const float y = 100.f + static_cast<float>(index / 16) * 20.f;
            BoxBody distant = createBox(scene, "DistantStatic" + std::to_string(index), {x, y},
                                        {1.f, 1.f}, false);
            distant.collider.setSensor(true);
        }

        fixedStep(scene, world);

        const l2d::PhysicsBroadPhaseStats2D& stats = world.broadPhaseStats();
        L2D_REQUIRE_EQUAL(stats.proxyCount, 129);
        L2D_REQUIRE_EQUAL(stats.occupiedCellCount, 128);
        L2D_REQUIRE_EQUAL(stats.fallbackProxyCount, 0);
        L2D_REQUIRE_EQUAL(stats.bruteForcePairCount, 128);
        L2D_REQUIRE_EQUAL(stats.candidatePairCount, 1);
        L2D_REQUIRE_EQUAL(stats.narrowPhaseTestCount, 1);
        L2D_REQUIRE(stats.candidatePairCount < stats.bruteForcePairCount);
        L2D_REQUIRE_EQUAL(world.contacts().size(), 1);
        L2D_REQUIRE(world.isTouching(mover.object.id(), nearStatic.object.id()));
    }
}

int main()
{
    int failures = 0;

    runTest("uniform grid negative boundary tangency", testUniformGridNegativeBoundaryTangency,
            failures);
    runTest("uniform grid deduplicates multi-cell pairs", testUniformGridDeduplicatesMultiCellPairs,
            failures);
    runTest("uniform grid fallback and static suppression",
            testUniformGridFallbackAndStaticSuppression, failures);
    runTest("uniform grid matches brute-force contacts and events",
            testUniformGridMatchesBruteForceContactsAndEvents, failures);
    runTest("uniform grid sparse telemetry reduction", testUniformGridSparseTelemetryReduction,
            failures);

    if (failures != 0)
    {
        std::cerr << failures << " physics broad-phase test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D physics broad-phase tests passed.\n";
    return 0;
}

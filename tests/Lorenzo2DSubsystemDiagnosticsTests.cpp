#include "TestSupport.hpp"

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <memory>
#include <string_view>

namespace
{
    using l2d::test::runTest;

    void testSceneAndRenderQueueAccumulation()
    {
        l2d::Scene scene;
        l2d::GameObject& active = scene.createGameObject("active");
        l2d::Component& first = active.addComponent<l2d::Component>();
        active.addComponent<l2d::Component>();
        first.setActive(false);

        l2d::GameObject& inactive = scene.createGameObject("inactive");
        inactive.addComponent<l2d::Component>();
        inactive.setActive(false);

        l2d::RenderQueue2D queue;
        queue.build(scene);

        l2d::DiagnosticCounters counters;
        l2d::accumulateSceneDiagnostics(scene, counters);
        l2d::accumulateRenderQueueDiagnostics(queue, counters);

        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::ActiveEntities), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::ActiveComponents), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::RenderedItems), 1u);
    }

    void testPhysicsAndEventCounters()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("collider");
        object.addComponent<l2d::CircleCollider2D>(8.f);

        l2d::PhysicsWorld2D world;
        world.step(scene, 1.f / 60.f);

        l2d::DiagnosticCounters counters;
        l2d::accumulatePhysicsDiagnostics(world, counters);
        l2d::recordPhysicsQueries(counters, 3u);

        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::Colliders), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::PhysicsQueries), 3u);
    }

    void testNavigationAndTilemapAccumulation()
    {
        l2d::NavigationGridConfig2D config;
        config.size = {3u, 1u};
        const l2d::NavigationGrid2D grid(config);
        const l2d::NavigationPath2D path =
            l2d::AStarPathfinder2D{}.findPath(grid, {0, 0}, {2, 0});

        l2d::TileMapRenderStats renderStats;
        renderStats.drawCallCount = 4u;
        renderStats.submittedTileCount = 12u;

        l2d::DiagnosticCounters counters;
        l2d::recordNavigationPathDiagnostics(path, counters, true);
        l2d::accumulateTileMapRenderDiagnostics(renderStats, counters);

        L2D_REQUIRE(path.succeeded());
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::NavigationExpansions),
                          path.visitedNodes);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::NavigationReplans), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::DrawCalls), 4u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::RenderedItems), 12u);
    }

    void testAssetAudioAndSaveAccumulation()
    {
        l2d::AssetManager assets;
        auto clip = std::make_shared<l2d::AnimationClip>("idle");
        L2D_REQUIRE(
            assets.storeAnimationClip("idle", l2d::AnimationClipHandle(std::move(clip))));

        const l2d::LiveFontHandle liveFont = assets.liveFont("ui");
        const l2d::LiveTextureHandle liveTexture = assets.liveTexture("atlas");
        const l2d::LiveSoundBufferHandle liveSound = assets.liveSoundBuffer("click");
        L2D_REQUIRE(static_cast<bool>(liveFont));
        L2D_REQUIRE(static_cast<bool>(liveTexture));
        L2D_REQUIRE(static_cast<bool>(liveSound));

        l2d::AudioSystem audio;

        l2d::DiagnosticCounters counters;
        l2d::accumulateAssetDiagnostics(assets, counters);
        l2d::accumulateAudioDiagnostics(audio, counters);
        l2d::recordSaveWriteDiagnostics(counters, 1024u);
        l2d::recordSaveReadDiagnostics(counters, 768u);

        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::LoadedAssets), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::LiveAssets), 3u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::ActiveAudioVoices), 0u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::SaveBytesWritten), 1024u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::SaveBytesRead), 768u);
    }

    void testAccumulationAndScopeNames()
    {
        l2d::DiagnosticCounters counters;
        l2d::recordPhysicsQueries(counters, 2u);
        l2d::recordPhysicsQueries(counters, 5u);
        l2d::recordSaveWriteDiagnostics(counters, 10u);
        l2d::recordSaveWriteDiagnostics(counters, 20u);

        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::PhysicsQueries), 7u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::SaveBytesWritten), 30u);
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::FixedStep, std::string_view("fixed-step"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Render, std::string_view("render"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Physics, std::string_view("physics"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Navigation, std::string_view("navigation"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Asset, std::string_view("asset"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Ui, std::string_view("ui"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Audio, std::string_view("audio"));
        L2D_REQUIRE_EQUAL(l2d::diagnostic_scope::Save, std::string_view("save"));
    }
}

int main()
{
    if (!l2d::AudioSystem::useNullPlaybackDevice()) return 2;

    int failures = 0;
    runTest("subsystem scene and render queue", testSceneAndRenderQueueAccumulation, failures);
    runTest("subsystem physics events", testPhysicsAndEventCounters, failures);
    runTest("subsystem navigation and tilemap", testNavigationAndTilemapAccumulation, failures);
    runTest("subsystem assets audio save", testAssetAudioAndSaveAccumulation, failures);
    runTest("subsystem accumulation and scopes", testAccumulationAndScopeNames, failures);
    return failures == 0 ? 0 : 1;
}

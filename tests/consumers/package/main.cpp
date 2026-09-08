#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>
#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/PathFollower2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>
#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>
#include <Lorenzo2D/Save/SaveGame.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>
#include <Lorenzo2D/Tilemap/IsometricTileGrid2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapColliderBuilder2D.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <sstream>

int main()
{
    l2d::Transform transform({2.f, 3.f});
    transform.move({4.f, 5.f});

    const sf::Vector2f position = transform.position();

    l2d::AnimationClip clip("idle");
    const bool frameAdded = clip.addFrame({{0, 0}, {16, 16}});

    l2d::TileSet tileSet;
    const bool tileAdded = tileSet.setTileFromGrid('#', {1u, 0u}, {16u, 16u});

    l2d::AsciiTileMapImporter tileImporter;
    tileImporter.mapCharacter('#', 1u);
    l2d::TileMapData tileData;
    const bool tileDataImported = tileImporter.import({"##"}, {16.f, 16.f}, tileData);
    const auto tileColliders = l2d::TileMapColliderBuilder2D::build(tileData);
    tileData.setOrientation(l2d::TileMapOrientation::Isometric);
    const l2d::IsometricTileGrid2D isometricGrid(tileData, {32.f, 16.f});
    const auto isometricRender = isometricGrid.renderPosition({1u, 0u});
    const bool isometricPick =
        isometricRender && isometricGrid.pick(*isometricRender) == l2d::TileMapCell{1u, 0u};

    l2d::ComponentCodecRegistry codecs;
    const bool codecRegistered = codecs.registerCodec(
        "consumer.marker", 1u,
        [](const l2d::GameObject&) { return std::optional<std::string>("{}"); },
        [](l2d::GameObject&, const std::string&) { return true; });

    l2d::LevelDocument level;
    level.objects.push_back(l2d::Prefab{});
    std::ostringstream serialized;
    const bool levelSaved = l2d::LevelSerializer::save(serialized, level);

    l2d::ResourceLocator resources;
    const bool resourceRootAdded = resources.addRoot("assets");

    l2d::SaveDocument save("consumer", 1u);
    const bool saveValueSet = save.setInteger("score", 7);
    std::ostringstream saveOutput;
    const bool saveWritten = l2d::SaveGameSerializer::save(saveOutput, save);
    l2d::SaveDocument saveLoaded;
    std::istringstream saveInput(saveOutput.str());
    const bool saveRead = l2d::SaveGameSerializer::load(saveInput, saveLoaded);

    l2d::UiCanvas2D ui;
    l2d::UiButton2D uiButton;
    uiButton.id = "consumer-button";
    uiButton.position = {0.f, 0.f};
    uiButton.size = {64.f, 32.f};
    const bool uiConfigured = ui.addButton(uiButton);
    l2d::PointerState uiPointer;
    uiPointer.screenPosition = {16, 16};
    ui.update(uiPointer);
    const bool uiHit = ui.hoveredButton() == std::optional<std::string>("consumer-button");

    l2d::AudioPlayOptions2D audioOptions;
    audioOptions.bus = l2d::AudioBus2D::Ui;
    const bool audioConfigured = l2d::AudioSystem::isValidPlayOptions(audioOptions);

    l2d::Profiler diagnostics;
    const bool diagnosticFrameStarted = diagnostics.beginFrame();
    const bool diagnosticRecorded = diagnostics.record("consumer", 0.25);
    const bool diagnosticFrameEnded = diagnostics.endFrame();
    l2d::DiagnosticCounters diagnosticCounters;
    diagnosticCounters.set(l2d::DiagnosticCounter::ActiveEntities, 1u);
    l2d::recordPhysicsQueries(diagnosticCounters, 2u);
    l2d::recordSaveWriteDiagnostics(diagnosticCounters, saveOutput.str().size());
    l2d::recordSaveReadDiagnostics(diagnosticCounters, saveOutput.str().size());
    const l2d::DiagnosticSnapshot diagnosticSnapshot =
        l2d::captureDiagnosticSnapshot(diagnostics, diagnosticCounters);
    const bool diagnosticsConfigured =
        diagnosticFrameStarted && diagnosticRecorded && diagnosticFrameEnded &&
        diagnosticSnapshot.frameIndex == 1u && diagnosticSnapshot.timings.size() == 1u;

    l2d::DeterministicHasher64 replayHasher;
    replayHasher.appendString("consumer-state");
    replayHasher.appendUInt64(diagnosticSnapshot.frameIndex);
    l2d::ReplayTrace replayTrace;
    const bool replayRecorded = replayTrace.record(0u, {}, replayHasher.value());
    const bool replayConfigured =
        replayRecorded && l2d::compareReplayTraces(replayTrace, replayTrace).equivalent();

    l2d::CircleCollider2D collider(2.f);
    l2d::CapsuleCollider2D capsule(2.f, 8.f);
    l2d::ConvexPolygonCollider2D polygon;
    l2d::PhysicsQueryFilter2D queryFilter;
    l2d::CharacterMotorConfig2D motorConfig;
    const l2d::CharacterMotorConfig2D topDownMotorConfig = l2d::topDownCharacterMotorConfig2D();
    l2d::TopDownControllerConfig2D topDownConfig;
    l2d::GridStepControllerConfig2D gridConfig;
    const l2d::CharacterMotorConfig2D platformerMotorConfig =
        l2d::platformerCharacterMotorConfig2D(2u);
    l2d::PlatformerControllerConfig2D platformerConfig;
    l2d::NavigationGridConfig2D navigationConfig;
    navigationConfig.size = {2u, 1u};
    l2d::NavigationGrid2D navigationGrid(navigationConfig);
    const l2d::NavigationPath2D navigationPath =
        l2d::AStarPathfinder2D{}.findPath(navigationGrid, {0, 0}, {1, 0});
    l2d::recordNavigationPathDiagnostics(navigationPath, diagnosticCounters);
    l2d::PathFollowerConfig2D followerConfig;
    l2d::DistanceJoint2D joint(42u, 3.f);
    const l2d::OrthogonalProjection2D projection;
    const l2d::RenderContext2D renderContext{1.f, &projection, l2d::RenderPass2D::World};
    l2d::RenderOrder2D renderOrder(l2d::RenderDepthMode2D::ProjectedY);

    l2d::InputSnapshot inputSnapshot;
    l2d::InputMap inputMap(inputSnapshot);
    const bool inputConfigured =
        inputMap.bindAxis2D("move", l2d::InputCode::keyboard(sf::Keyboard::Scancode::A),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::D),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::W),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::S));

    return l2d::VersionString == "1.1.0" && position == sf::Vector2f{6.f, 8.f} && frameAdded &&
                   tileAdded && tileDataImported && tileColliders.empty() && isometricPick &&
                   codecRegistered && levelSaved && resourceRootAdded && saveValueSet &&
                   saveWritten && saveRead && saveLoaded == save && uiConfigured && uiHit &&
                   audioConfigured && diagnosticsConfigured && replayConfigured &&
                   diagnosticCounters.value(l2d::DiagnosticCounter::PhysicsQueries) == 2u &&
                   diagnosticCounters.value(l2d::DiagnosticCounter::NavigationExpansions) ==
                       navigationPath.visitedNodes &&
                   inputConfigured && collider.id() != l2d::InvalidColliderId &&
                   capsule.height() == 8.f && polygon.vertices().size() == 3u &&
                   queryFilter.categoryMask != 0u &&
                   l2d::CharacterMotor2D::isValidConfig(motorConfig) &&
                   l2d::CharacterMotor2D::isValidConfig(topDownMotorConfig) &&
                   l2d::TopDownController2D::isValidConfig(topDownConfig) &&
                   l2d::GridStepController2D::isValidConfig(gridConfig) &&
                   l2d::CharacterMotor2D::isValidConfig(platformerMotorConfig) &&
                   l2d::PlatformerController2D::isValidConfig(platformerConfig) &&
                   navigationPath.succeeded() &&
                   l2d::PathFollower2D::isValidConfig(followerConfig) &&
                   l2d::gridDirectionFromInput({1.f, 0.f}) == l2d::GridDirection2D::Right &&
                   joint.id() != l2d::InvalidJointId &&
                   renderContext.worldToRender(position) == position &&
                   renderOrder.depthMode() == l2d::RenderDepthMode2D::ProjectedY
               ? 0
               : 1;
}

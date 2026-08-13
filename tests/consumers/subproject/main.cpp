#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
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
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>
#include <Lorenzo2D/Tilemap/TileMapColliderBuilder2D.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>

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

    return l2d::VersionString == "0.12.0" && position == sf::Vector2f{6.f, 8.f} && frameAdded &&
                   tileAdded && tileDataImported && tileColliders.empty() && codecRegistered &&
                   levelSaved && resourceRootAdded && inputConfigured &&
                   collider.id() != l2d::InvalidColliderId && capsule.height() == 8.f &&
                   polygon.vertices().size() == 3u && queryFilter.categoryMask != 0u &&
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

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
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

    return l2d::VersionString == "0.7.0" && position == sf::Vector2f{6.f, 8.f} && frameAdded &&
                   tileAdded && levelSaved && resourceRootAdded && inputConfigured &&
                   collider.id() != l2d::InvalidColliderId && capsule.height() == 8.f &&
                   polygon.vertices().size() == 3u && queryFilter.categoryMask != 0u &&
                   joint.id() != l2d::InvalidJointId &&
                   renderContext.worldToRender(position) == position &&
                   renderOrder.depthMode() == l2d::RenderDepthMode2D::ProjectedY
               ? 0
               : 1;
}

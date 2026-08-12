#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>
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
    l2d::DistanceJoint2D joint(42u, 3.f);

    l2d::InputSnapshot inputSnapshot;
    l2d::InputMap inputMap(inputSnapshot);
    const bool inputConfigured =
        inputMap.bindAxis2D("move", l2d::InputCode::keyboard(sf::Keyboard::Scancode::A),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::D),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::W),
                            l2d::InputCode::keyboard(sf::Keyboard::Scancode::S));

    return l2d::VersionString == "0.5.0" && position == sf::Vector2f{6.f, 8.f} && frameAdded &&
                   tileAdded && levelSaved && resourceRootAdded && inputConfigured &&
                   collider.id() != l2d::InvalidColliderId && joint.id() != l2d::InvalidJointId
               ? 0
               : 1;
}

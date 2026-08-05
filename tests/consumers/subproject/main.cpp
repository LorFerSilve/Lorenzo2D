#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/ECS/Transform.hpp>
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

    return position == sf::Vector2f{6.f, 8.f} && frameAdded && tileAdded && levelSaved &&
                   resourceRootAdded
               ? 0
               : 1;
}

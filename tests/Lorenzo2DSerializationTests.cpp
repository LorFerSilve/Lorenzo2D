#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;
    using l2d::test::TemporaryFile;

    l2d::Prefab makePlayerPrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "Player One";
        prefab.tag = "player";
        prefab.transform.position = {12.5f, -8.f};
        prefab.transform.rotation = 15.f;
        prefab.transform.scale = {-1.f, 2.f};
        prefab.rectangleRenderer =
            l2d::RectangleRendererPrefab{{32.f, 48.f}, sf::Color(20, 80, 220, 200)};
        prefab.rigidBody = l2d::RigidBodyPrefab{
            l2d::BodyType2D::Kinematic, {4.f, 5.f}, {1.f, 2.f}, 3.f, true, 0.5f};

        l2d::BoxColliderPrefab collider;
        collider.size = {30.f, 44.f};
        collider.properties.offset = {1.f, 2.f};
        collider.properties.material = {0.25f, 0.6f, 0.4f};
        collider.properties.filter = {4u, 9u};
        collider.properties.sensor = true;
        prefab.boxCollider = collider;

        l2d::CircleColliderPrefab circleCollider;
        circleCollider.radius = 8.f;
        circleCollider.properties.offset = {16.f, 24.f};
        circleCollider.properties.filter = {4u, 9u};
        prefab.circleCollider = circleCollider;
        return prefab;
    }

    void testLevelRoundTripsAndInstantiatesEngineComponents()
    {
        l2d::LevelDocument source;
        source.name = "Phase 2 showcase";
        source.objects.push_back(makePlayerPrefab());

        l2d::Prefab decoration;
        decoration.name = "Orb";
        decoration.circleRenderer = l2d::CircleRendererPrefab{12.f, sf::Color(240, 190, 40)};
        source.objects.push_back(decoration);

        std::stringstream serialized;
        L2D_REQUIRE(l2d::LevelSerializer::save(serialized, source));
        L2D_REQUIRE(serialized.str().find("LORENZO2D_LEVEL 1") == 0u);

        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::load(serialized, loaded));
        L2D_REQUIRE(loaded.name == source.name);
        L2D_REQUIRE(loaded.objects.size() == 2u);

        const l2d::Prefab& playerPrefab = loaded.objects[0];
        L2D_REQUIRE(playerPrefab.name == "Player One");
        L2D_REQUIRE(playerPrefab.tag == "player");
        L2D_REQUIRE(playerPrefab.rectangleRenderer.has_value());
        L2D_REQUIRE(playerPrefab.rigidBody.has_value());
        L2D_REQUIRE(playerPrefab.boxCollider.has_value());
        L2D_REQUIRE(playerPrefab.circleCollider.has_value());
        L2D_REQUIRE(playerPrefab.rectangleRenderer->color == sf::Color(20, 80, 220, 200));
        L2D_REQUIRE_APPROX_2D(playerPrefab.transform.position, sf::Vector2f(12.5f, -8.f), 0.0001f);

        l2d::Scene scene("Loaded");
        const std::vector<l2d::GameObjectHandle> objects =
            l2d::LevelSerializer::instantiate(scene, loaded);
        L2D_REQUIRE(objects.size() == 2u);
        L2D_REQUIRE(scene.gameObjectCount() == 2u);

        l2d::GameObject* player = objects[0].get();
        L2D_REQUIRE(player != nullptr);
        L2D_REQUIRE(player->hasTag("player"));
        L2D_REQUIRE(player->getComponent<l2d::RectangleRenderer>() != nullptr);

        const l2d::RigidBody2D* body = player->getComponent<l2d::RigidBody2D>();
        const l2d::BoxCollider2D* collider = player->getComponent<l2d::BoxCollider2D>();
        const l2d::CircleCollider2D* circleCollider = player->getComponent<l2d::CircleCollider2D>();
        L2D_REQUIRE(body != nullptr);
        L2D_REQUIRE(collider != nullptr);
        L2D_REQUIRE(circleCollider != nullptr);
        L2D_REQUIRE_EQUAL(player->getComponents<l2d::Collider2D>().size(), 2);
        L2D_REQUIRE(collider->id() != circleCollider->id());
        L2D_REQUIRE(body->bodyType() == l2d::BodyType2D::Kinematic);
        L2D_REQUIRE_APPROX(body->mass(), 3.f, 0.0001f);
        L2D_REQUIRE(body->useGravity());
        L2D_REQUIRE(collider->isSensor());
        L2D_REQUIRE(collider->filter().categoryBits == 4u);
        L2D_REQUIRE(collider->filter().maskBits == 9u);
        L2D_REQUIRE_APPROX(collider->material().dynamicFriction, 0.4f, 0.0001f);
    }

    void testMalformedAndUnsupportedInputIsTransactional()
    {
        l2d::LevelDocument destination;
        destination.name = "unchanged";
        destination.objects.push_back(makePlayerPrefab());

        std::stringstream unsupported("LORENZO2D_LEVEL 99\nlevel \"bad\"\nobjects 0\n");
        L2D_REQUIRE(!l2d::LevelSerializer::load(unsupported, destination));
        L2D_REQUIRE(destination.name == "unchanged");
        L2D_REQUIRE(destination.objects.size() == 1u);

        std::stringstream malformed(
            "LORENZO2D_LEVEL 1\nlevel \"bad\"\nobjects 1\nobject\nname \"half\"\n");
        L2D_REQUIRE(!l2d::LevelSerializer::load(malformed, destination));
        L2D_REQUIRE(destination.name == "unchanged");
        L2D_REQUIRE(destination.objects.size() == 1u);
    }

    void testPrefabLibraryValidatesBeforeReplacing()
    {
        l2d::PrefabLibrary library;
        l2d::Prefab valid = makePlayerPrefab();
        L2D_REQUIRE(library.store("player", valid));
        L2D_REQUIRE(library.size() == 1u);

        l2d::Prefab invalid = valid;
        invalid.transform.position.x = std::numeric_limits<float>::quiet_NaN();
        L2D_REQUIRE(!library.store("player", invalid));
        L2D_REQUIRE(library.find("player") != nullptr);
        L2D_REQUIRE(library.find("player")->name == "Player One");

        l2d::Scene scene;
        l2d::GameObject* instance = library.instantiate(scene, "player");
        L2D_REQUIRE(instance != nullptr);
        L2D_REQUIRE(library.instantiate(scene, "missing") == nullptr);
        L2D_REQUIRE(library.remove("player"));
        L2D_REQUIRE(!library.remove("player"));
    }

    void testLevelFileHelpersRoundTrip()
    {
        const TemporaryFile file("lorenzo2d_level", ".l2dlevel");
        l2d::LevelDocument source;
        source.name = "file level";
        source.objects.push_back(makePlayerPrefab());

        L2D_REQUIRE(l2d::LevelSerializer::saveToFile(file.path().string(), source));

        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::loadFromFile(file.path().string(), loaded));
        L2D_REQUIRE(loaded.name == source.name);
        L2D_REQUIRE(loaded.objects.size() == 1u);
    }

    void testCheckedInExampleLevelLoads()
    {
        l2d::LevelDocument level;
        L2D_REQUIRE(l2d::LevelSerializer::loadFromFile("phase2-showcase.l2dlevel", level));
        L2D_REQUIRE(level.name == "Phase 2 showcase");
        L2D_REQUIRE(level.objects.size() == 2u);
        L2D_REQUIRE(level.objects[0].boxCollider.has_value());
        L2D_REQUIRE(level.objects[1].circleCollider.has_value());
    }

    void testInvalidDocumentsDoNotOverwriteFilesOrScenes()
    {
        const TemporaryFile file("lorenzo2d_invalid_level", ".l2dlevel");
        {
            std::ofstream output(file.path());
            output << "keep me";
        }

        l2d::LevelDocument invalid;
        invalid.objects.push_back(makePlayerPrefab());
        invalid.objects[0].transform.position.x = std::numeric_limits<float>::infinity();

        L2D_REQUIRE(!l2d::LevelSerializer::saveToFile(file.path().string(), invalid));

        std::ifstream input(file.path());
        std::string contents;
        std::getline(input, contents);
        L2D_REQUIRE(contents == "keep me");

        l2d::Scene scene;
        bool threw = false;
        try
        {
            (void)l2d::LevelSerializer::instantiate(scene, invalid);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        L2D_REQUIRE(threw);
        L2D_REQUIRE(scene.gameObjectCount() == 0u);
    }
}

int main()
{
    int failures = 0;
    runTest("levels round-trip and instantiate components",
            testLevelRoundTripsAndInstantiatesEngineComponents, failures);
    runTest("malformed levels load transactionally",
            testMalformedAndUnsupportedInputIsTransactional, failures);
    runTest("prefab library validates replacements", testPrefabLibraryValidatesBeforeReplacing,
            failures);
    runTest("level file helpers round-trip", testLevelFileHelpersRoundTrip, failures);
    runTest("checked-in example level loads", testCheckedInExampleLevelLoads, failures);
    runTest("invalid levels preserve files and scenes",
            testInvalidDocumentsDoNotOverwriteFilesOrScenes, failures);

    if (failures != 0)
    {
        std::cerr << failures << " serialization test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D serialization tests passed.\n";
    return 0;
}

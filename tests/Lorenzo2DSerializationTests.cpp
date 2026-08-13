#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Animation/Animator.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <fstream>
#include <memory>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;
    using l2d::test::TemporaryFile;

    class MarkerComponent final : public l2d::Component
    {
      public:
        explicit MarkerComponent(std::string marker) : value(std::move(marker)) {}
        std::string value;
    };

    l2d::Prefab makePlayerPrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "Player One";
        prefab.tag = "player";
        prefab.transform.position = {12.5f, -8.f};
        prefab.transform.rotation = 15.f;
        prefab.transform.scale = {-1.f, 2.f};
        prefab.zOrder = 12;
        prefab.rectangleRenderer =
            l2d::RectangleRendererPrefab{{32.f, 48.f}, sf::Color(20, 80, 220, 200)};
        prefab.rigidBody = l2d::RigidBodyPrefab{
            l2d::BodyType2D::Kinematic, {4.f, 5.f}, {1.f, 2.f}, 3.f, true, 0.5f};
        l2d::CharacterMotorPrefab motor;
        motor.config.skinWidth = 0.025f;
        motor.config.maximumSlopeAngleDegrees = 42.f;
        motor.config.maximumSlideIterations = 6u;
        motor.config.queryFilter.categoryMask = 8u;
        motor.config.oneWayPlatformCategoryMask = 16u;
        prefab.characterMotor = motor;
        l2d::TopDownControllerPrefab topDown;
        topDown.config.maximumSpeed = 240.f;
        topDown.config.acceleration = 1200.f;
        prefab.topDownController = topDown;
        l2d::GridStepControllerPrefab grid;
        grid.config.cellSize = {16.f, 24.f};
        grid.config.gridOrigin = {4.f, 8.f};
        grid.config.stepDuration = 0.2f;
        grid.config.axisPriority = l2d::GridAxisPriority2D::Horizontal;
        prefab.gridStepController = grid;
        l2d::PlatformerControllerPrefab platformer;
        platformer.config.maximumRunSpeed = 280.f;
        platformer.config.jumpSpeed = 700.f;
        platformer.config.coyoteTime = 0.14f;
        prefab.platformerController = platformer;

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

        l2d::CapsuleColliderPrefab capsuleCollider;
        capsuleCollider.radius = 7.f;
        capsuleCollider.height = 30.f;
        capsuleCollider.properties.offset = {15.f, 22.f};
        capsuleCollider.properties.material = {0.1f, 0.5f, 0.3f};
        prefab.capsuleCollider = capsuleCollider;

        l2d::ConvexPolygonColliderPrefab polygonCollider;
        polygonCollider.vertices = {{-10.f, 10.f}, {0.f, -10.f}, {10.f, 10.f}};
        polygonCollider.properties.offset = {15.f, 20.f};
        polygonCollider.properties.sensor = true;
        prefab.convexPolygonCollider = polygonCollider;
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
        L2D_REQUIRE(serialized.str().find("\"format\": \"Lorenzo2DLevel\"") != std::string::npos);
        L2D_REQUIRE(serialized.str().find("\"version\": 7") != std::string::npos);

        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::load(serialized, loaded));
        L2D_REQUIRE(loaded.name == source.name);
        L2D_REQUIRE(loaded.objects.size() == 2u);

        const l2d::Prefab& playerPrefab = loaded.objects[0];
        L2D_REQUIRE(playerPrefab.name == "Player One");
        L2D_REQUIRE(playerPrefab.tag == "player");
        L2D_REQUIRE(playerPrefab.zOrder == 12);
        L2D_REQUIRE(playerPrefab.rectangleRenderer.has_value());
        L2D_REQUIRE(playerPrefab.rigidBody.has_value());
        L2D_REQUIRE(playerPrefab.characterMotor.has_value());
        L2D_REQUIRE_APPROX(playerPrefab.characterMotor->config.skinWidth, 0.025f, 0.0001f);
        L2D_REQUIRE_EQUAL(playerPrefab.characterMotor->config.maximumSlideIterations, 6u);
        L2D_REQUIRE_EQUAL(playerPrefab.characterMotor->config.oneWayPlatformCategoryMask, 16u);
        L2D_REQUIRE(playerPrefab.topDownController.has_value());
        L2D_REQUIRE_APPROX(playerPrefab.topDownController->config.maximumSpeed, 240.f, 0.0001f);
        L2D_REQUIRE(playerPrefab.gridStepController.has_value());
        L2D_REQUIRE_APPROX_2D(playerPrefab.gridStepController->config.cellSize,
                              sf::Vector2f(16.f, 24.f), 0.0001f);
        L2D_REQUIRE(playerPrefab.gridStepController->config.axisPriority ==
                    l2d::GridAxisPriority2D::Horizontal);
        L2D_REQUIRE(playerPrefab.platformerController.has_value());
        L2D_REQUIRE_APPROX(playerPrefab.platformerController->config.jumpSpeed, 700.f, 0.0001f);
        L2D_REQUIRE(playerPrefab.boxCollider.has_value());
        L2D_REQUIRE(playerPrefab.circleCollider.has_value());
        L2D_REQUIRE(playerPrefab.capsuleCollider.has_value());
        L2D_REQUIRE(playerPrefab.convexPolygonCollider.has_value());
        L2D_REQUIRE_EQUAL(playerPrefab.convexPolygonCollider->vertices.size(), 3u);
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
        L2D_REQUIRE(player->zOrder() == 12);
        L2D_REQUIRE(player->getComponent<l2d::RectangleRenderer>() != nullptr);

        const l2d::RigidBody2D* body = player->getComponent<l2d::RigidBody2D>();
        const l2d::CharacterMotor2D* motor = player->getComponent<l2d::CharacterMotor2D>();
        const l2d::TopDownController2D* topDown = player->getComponent<l2d::TopDownController2D>();
        const l2d::GridStepController2D* grid = player->getComponent<l2d::GridStepController2D>();
        const l2d::PlatformerController2D* platformer =
            player->getComponent<l2d::PlatformerController2D>();
        const l2d::BoxCollider2D* collider = player->getComponent<l2d::BoxCollider2D>();
        const l2d::CircleCollider2D* circleCollider = player->getComponent<l2d::CircleCollider2D>();
        const l2d::CapsuleCollider2D* capsuleCollider =
            player->getComponent<l2d::CapsuleCollider2D>();
        const l2d::ConvexPolygonCollider2D* polygonCollider =
            player->getComponent<l2d::ConvexPolygonCollider2D>();
        L2D_REQUIRE(body != nullptr);
        L2D_REQUIRE(motor != nullptr);
        L2D_REQUIRE(topDown != nullptr);
        L2D_REQUIRE(grid != nullptr);
        L2D_REQUIRE(platformer != nullptr);
        L2D_REQUIRE_APPROX(topDown->config().maximumSpeed, 240.f, 0.0001f);
        L2D_REQUIRE_APPROX_2D(grid->config().gridOrigin, sf::Vector2f(4.f, 8.f), 0.0001f);
        L2D_REQUIRE_APPROX(platformer->config().maximumRunSpeed, 280.f, 0.0001f);
        L2D_REQUIRE_APPROX(motor->config().maximumSlopeAngleDegrees, 42.f, 0.0001f);
        L2D_REQUIRE(collider != nullptr);
        L2D_REQUIRE(circleCollider != nullptr);
        L2D_REQUIRE(capsuleCollider != nullptr);
        L2D_REQUIRE(polygonCollider != nullptr);
        L2D_REQUIRE_EQUAL(player->getComponents<l2d::Collider2D>().size(), 4u);
        L2D_REQUIRE(collider->id() != circleCollider->id());
        L2D_REQUIRE(body->bodyType() == l2d::BodyType2D::Kinematic);
        L2D_REQUIRE_APPROX(body->mass(), 3.f, 0.0001f);
        L2D_REQUIRE(body->useGravity());
        L2D_REQUIRE(collider->isSensor());
        L2D_REQUIRE(collider->filter().categoryBits == 4u);
        L2D_REQUIRE(collider->filter().maskBits == 9u);
        L2D_REQUIRE_APPROX(collider->material().dynamicFriction, 0.4f, 0.0001f);
        L2D_REQUIRE_APPROX(capsuleCollider->height(), 30.f, 0.0001f);
        L2D_REQUIRE_EQUAL(polygonCollider->vertices().size(), 3u);
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

        l2d::Prefab invalidPolygon = valid;
        invalidPolygon.convexPolygonCollider->vertices = {
            {0.f, 0.f}, {10.f, 0.f}, {5.f, 2.f}, {10.f, 10.f}, {0.f, 10.f}};
        L2D_REQUIRE(!library.store("player", invalidPolygon));

        l2d::Prefab invalidCapsule = valid;
        invalidCapsule.capsuleCollider->height = invalidCapsule.capsuleCollider->radius;
        L2D_REQUIRE(!library.store("player", invalidCapsule));

        l2d::Prefab invalidDynamicMotor = valid;
        invalidDynamicMotor.rigidBody->bodyType = l2d::BodyType2D::Dynamic;
        L2D_REQUIRE(!library.store("player", invalidDynamicMotor));

        l2d::Prefab invalidStaticMotor = valid;
        invalidStaticMotor.rigidBody->bodyType = l2d::BodyType2D::Static;
        L2D_REQUIRE(!library.store("player", invalidStaticMotor));

        l2d::Prefab missingControllerMotor = valid;
        missingControllerMotor.characterMotor.reset();
        L2D_REQUIRE(!library.store("player", missingControllerMotor));

        l2d::Prefab invalidTopDown = valid;
        invalidTopDown.topDownController->config.maximumSpeed = 0.f;
        L2D_REQUIRE(!library.store("player", invalidTopDown));

        l2d::Prefab invalidGrid = valid;
        invalidGrid.gridStepController->config.cellSize.y = 0.f;
        L2D_REQUIRE(!library.store("player", invalidGrid));

        l2d::Prefab invalidPlatformer = valid;
        invalidPlatformer.platformerController->config.gravity = -1.f;
        L2D_REQUIRE(!library.store("player", invalidPlatformer));

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
        L2D_REQUIRE(level.objects[0].zOrder == 0);
        L2D_REQUIRE(level.objects[1].zOrder == 0);
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

    void testSpriteAnimatorAssetsAndCustomCodecsRoundTrip()
    {
        l2d::Prefab actor;
        actor.name = "Sprite actor";
        l2d::SpriteRendererPrefab sprite;
        sprite.texture = "hero";
        sprite.textureRect = {{0, 0}, {16, 24}};
        sprite.size = {32.f, 48.f};
        sprite.origin = {8.f, 24.f};
        sprite.flipX = true;
        sprite.renderOrder.layer = 3;
        sprite.renderOrder.depth = 4.f;
        sprite.renderOrder.order = 5;
        sprite.renderOrder.depthMode =
            static_cast<std::uint8_t>(l2d::RenderDepthMode2D::ProjectedY);
        actor.spriteRenderer = sprite;
        actor.animator = l2d::AnimatorPrefab{{"idle"}, "idle", 1.5f, true};
        actor.customComponents.push_back({"Marker", 1u, true, "{\"value\":\"blue\"}"});
        actor.customComponents.push_back({"OptionalFuture", 7u, false, "{}"});

        l2d::LevelDocument source;
        source.name = "Asset level";
        source.objects.push_back(actor);
        std::stringstream serialized;
        L2D_REQUIRE(l2d::LevelSerializer::saveJson(serialized, source));

        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::loadJson(serialized, loaded));
        L2D_REQUIRE(loaded.objects[0].spriteRenderer.has_value());
        L2D_REQUIRE(loaded.objects[0].animator.has_value());
        L2D_REQUIRE_EQUAL(loaded.objects[0].customComponents.size(), 2u);

        l2d::AssetManager assets;
        L2D_REQUIRE(
            assets.storeTexture("hero", l2d::TextureHandle(std::make_shared<sf::Texture>())));
        auto clip = std::make_shared<l2d::AnimationClip>("idle");
        L2D_REQUIRE(clip->addFrame({{0, 0}, {16, 24}}));
        L2D_REQUIRE(assets.storeAnimationClip("idle", l2d::AnimationClipHandle(clip)));

        l2d::ComponentCodecRegistry codecs;
        L2D_REQUIRE(codecs.registerCodec(
            "Marker", 1u,
            [](const l2d::GameObject& object) -> std::optional<std::string>
            {
                const MarkerComponent* marker = object.getComponent<MarkerComponent>();
                return marker == nullptr ? std::nullopt : std::optional<std::string>(marker->value);
            },
            [](l2d::GameObject& object, const std::string& data)
            {
                object.addComponent<MarkerComponent>(data);
                return true;
            }));

        l2d::Scene scene;
        const auto handles = l2d::LevelSerializer::instantiate(scene, loaded, assets, &codecs);
        L2D_REQUIRE_EQUAL(handles.size(), 1u);
        l2d::GameObject* object = handles[0].get();
        L2D_REQUIRE(object != nullptr);
        L2D_REQUIRE(object->getComponent<l2d::SpriteRenderer>() != nullptr);
        L2D_REQUIRE(object->getComponent<l2d::Animator>() != nullptr);
        L2D_REQUIRE(object->getComponent<l2d::RenderOrder2D>() != nullptr);
        L2D_REQUIRE(object->getComponent<MarkerComponent>() != nullptr);
        L2D_REQUIRE(object->getComponent<MarkerComponent>()->value == "{\"value\":\"blue\"}");

        l2d::LevelDocument missingRequired = loaded;
        missingRequired.objects[0].customComponents[0].version = 99u;
        l2d::Scene rejectedScene;
        bool rejected = false;
        try
        {
            (void)l2d::LevelSerializer::instantiate(rejectedScene, missingRequired, assets,
                                                    &codecs);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        L2D_REQUIRE(rejected);
        L2D_REQUIRE_EQUAL(rejectedScene.gameObjectCount(), 0u);

        l2d::LevelDocument unchanged = loaded;
        std::stringstream duplicate(
            R"({"format":"Lorenzo2DLevel","version":4,"name":"bad","objects":[{"name":"x","tag":"","active":true,"zOrder":0,"transform":{"position":[0,0],"rotation":0,"scale":[1,1]},"components":[{"type":"CircleRenderer","version":1,"required":true,"data":{"radius":1,"color":[255,255,255,255]}},{"type":"CircleRenderer","version":1,"required":true,"data":{"radius":2,"color":[255,255,255,255]}}]}]})");
        L2D_REQUIRE(!l2d::LevelSerializer::loadJson(duplicate, unchanged));
        L2D_REQUIRE(unchanged.name == loaded.name);

        std::stringstream wrongTypes(
            R"({"format":"Lorenzo2DLevel","version":4,"name":{},"objects":[]})");
        L2D_REQUIRE(!l2d::LevelSerializer::loadJson(wrongTypes, unchanged));
        L2D_REQUIRE(unchanged.name == loaded.name);

        l2d::LevelDocument missingAsset = loaded;
        l2d::AssetManager emptyAssets;
        l2d::Scene missingAssetScene;
        rejected = false;
        try
        {
            (void)l2d::LevelSerializer::instantiate(missingAssetScene, missingAsset, emptyAssets,
                                                    &codecs);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        L2D_REQUIRE(rejected);
        L2D_REQUIRE_EQUAL(missingAssetScene.gameObjectCount(), 0u);
    }

    void testJsonVersionFourRemainsReadable()
    {
        std::stringstream versionFour(
            R"({"format":"Lorenzo2DLevel","version":4,"name":"Phase 4","objects":[{"name":"Legacy JSON object","tag":"","active":true,"zOrder":0,"transform":{"position":[3,4],"rotation":0,"scale":[1,1]},"components":[{"type":"BoxCollider2D","version":1,"required":true,"data":{"size":[8,10],"properties":{"offset":[0,0],"restitution":0,"staticFriction":0.5,"dynamicFriction":0.3,"category":1,"mask":4294967295,"sensor":false}}}]}]})");
        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::loadJson(versionFour, loaded));
        L2D_REQUIRE_EQUAL(loaded.objects.size(), 1u);
        L2D_REQUIRE(loaded.objects[0].boxCollider.has_value());
        L2D_REQUIRE(!loaded.objects[0].characterMotor.has_value());

        std::stringstream upgraded;
        L2D_REQUIRE(l2d::LevelSerializer::saveJson(upgraded, loaded));
        L2D_REQUIRE(upgraded.str().find("\"version\": 7") != std::string::npos);
    }

    void testJsonVersionFiveRemainsReadable()
    {
        std::stringstream versionFive(
            R"({"format":"Lorenzo2DLevel","version":5,"name":"Phase 5","objects":[{"name":"Legacy motor","tag":"","active":true,"zOrder":0,"transform":{"position":[0,0],"rotation":0,"scale":[1,1]},"components":[{"type":"CharacterMotor2D","version":1,"required":true,"data":{"skinWidth":0.01,"groundProbeDistance":0.1,"maximumSlopeAngleDegrees":50,"minimumMoveDistance":0.00001,"maximumMoveDistance":100000,"maximumPlatformDisplacement":10000,"maximumSlideIterations":4,"maximumRecoveryIterations":4,"upDirection":[0,-1],"categoryMask":4294967295,"includeSensors":false,"snapToGround":true,"inheritPlatformTranslation":true}},{"type":"BoxCollider2D","version":1,"required":true,"data":{"size":[8,10],"properties":{"offset":[0,0],"restitution":0,"staticFriction":0.5,"dynamicFriction":0.3,"category":1,"mask":4294967295,"sensor":false}}}]}]})");
        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::loadJson(versionFive, loaded));
        L2D_REQUIRE_EQUAL(loaded.objects.size(), 1u);
        L2D_REQUIRE(loaded.objects[0].characterMotor.has_value());
        L2D_REQUIRE(!loaded.objects[0].topDownController.has_value());
        L2D_REQUIRE(!loaded.objects[0].gridStepController.has_value());
        L2D_REQUIRE(!loaded.objects[0].platformerController.has_value());

        std::stringstream upgraded;
        L2D_REQUIRE(l2d::LevelSerializer::saveJson(upgraded, loaded));
        L2D_REQUIRE(upgraded.str().find("\"version\": 7") != std::string::npos);
    }

    void testJsonVersionSixRemainsReadable()
    {
        std::stringstream versionSix(
            R"({"format":"Lorenzo2DLevel","version":6,"name":"Phase 6","objects":[]})");
        l2d::LevelDocument loaded;
        L2D_REQUIRE(l2d::LevelSerializer::loadJson(versionSix, loaded));
        L2D_REQUIRE_EQUAL(loaded.objects.size(), 0u);
        std::stringstream upgraded;
        L2D_REQUIRE(l2d::LevelSerializer::saveJson(upgraded, loaded));
        L2D_REQUIRE(upgraded.str().find("\"version\": 7") != std::string::npos);
    }

    void testInvalidControllerEnumIsRejectedTransactionally()
    {
        l2d::LevelDocument destination;
        destination.name = "unchanged";
        std::stringstream invalid(
            R"({"format":"Lorenzo2DLevel","version":6,"name":"bad","objects":[{"name":"Grid","tag":"","active":true,"zOrder":0,"transform":{"position":[0,0],"rotation":0,"scale":[1,1]},"components":[{"type":"CharacterMotor2D","version":1,"required":true,"data":{"upDirection":[0,-1]}},{"type":"GridStepController2D","version":1,"required":true,"data":{"cellSize":[32,32],"gridOrigin":[0,0],"axisPriority":256}},{"type":"BoxCollider2D","version":1,"required":true,"data":{"size":[8,10],"properties":{"offset":[0,0],"sensor":false}}}]}]})");
        L2D_REQUIRE(!l2d::LevelSerializer::loadJson(invalid, destination));
        L2D_REQUIRE(destination.name == "unchanged");
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
    runTest("sprite animator assets and custom codecs round-trip",
            testSpriteAnimatorAssetsAndCustomCodecsRoundTrip, failures);
    runTest("JSON level version 4 remains readable", testJsonVersionFourRemainsReadable, failures);
    runTest("JSON level version 5 remains readable", testJsonVersionFiveRemainsReadable, failures);
    runTest("JSON level version 6 remains readable", testJsonVersionSixRemainsReadable, failures);
    runTest("invalid controller enum is rejected transactionally",
            testInvalidControllerEnumIsRejectedTransactionally, failures);

    if (failures != 0)
    {
        std::cerr << failures << " serialization test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D serialization tests passed.\n";
    return 0;
}

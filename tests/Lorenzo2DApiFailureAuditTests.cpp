#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Core/ActionMap.hpp>
#include <Lorenzo2D/Core/InputContextStack.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "TestSupport.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    using l2d::test::runTest;

    class InvalidProjection final : public l2d::CoordinateProjection2D
    {
      public:
        sf::Vector2f worldToRender(sf::Vector2f) const override
        {
            return invalidVector();
        }

        sf::Vector2f renderToWorld(sf::Vector2f) const override
        {
            return invalidVector();
        }

        float depthFor(sf::Vector2f) const override
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

      private:
        static sf::Vector2f invalidVector()
        {
            const float nan = std::numeric_limits<float>::quiet_NaN();
            return {nan, nan};
        }
    };

    class AuditMarker final : public l2d::Component
    {
    };

    void testInputSnapshotBorrowingRejectsTemporaryConstruction()
    {
        static_assert(!std::is_constructible<l2d::InputMap, l2d::InputSnapshot&&>::value,
                      "InputMap must reject temporary snapshots");
        static_assert(!std::is_constructible<l2d::InputMap, const l2d::InputSnapshot&&>::value,
                      "InputMap must reject const temporary snapshots");
        static_assert(!std::is_constructible<l2d::InputContextStack, l2d::InputSnapshot&&>::value,
                      "InputContextStack must reject temporary snapshots");
        static_assert(
            !std::is_constructible<l2d::InputContextStack, const l2d::InputSnapshot&&>::value,
            "InputContextStack must reject const temporary snapshots");

        l2d::InputSnapshot snapshot;

        l2d::InputMap map;
        L2D_REQUIRE(!map.hasSnapshot());
        map.setSnapshot(snapshot);
        L2D_REQUIRE(map.hasSnapshot());
        map.clearSnapshot();
        L2D_REQUIRE(!map.hasSnapshot());

        l2d::InputContextStack stack;
        l2d::InputMap& context = stack.createContext("gameplay");
        L2D_REQUIRE(!stack.hasSnapshot());
        L2D_REQUIRE(!context.hasSnapshot());

        stack.setSnapshot(snapshot);
        L2D_REQUIRE(stack.hasSnapshot());
        L2D_REQUIRE(context.hasSnapshot());

        stack.clearSnapshot();
        L2D_REQUIRE(!stack.hasSnapshot());
        L2D_REQUIRE(!context.hasSnapshot());
    }

    void testCheckedLegacyActionMapReportsFailure()
    {
        l2d::ActionMap actions;

        L2D_REQUIRE(!actions.tryBindAction("", l2d::Key::D));
        L2D_REQUIRE(!actions.tryBindAction("invalid", l2d::Key::Unknown));

        L2D_REQUIRE(actions.tryBindAction("move-right", l2d::Key::D));
        L2D_REQUIRE(actions.tryClearAction("move-right"));
        L2D_REQUIRE(!actions.tryClearAction("move-right"));

        // Legacy adapters remain callable and retain their void contract.
        actions.bindAction("compatibility", l2d::Key::D);
        actions.clearAction("compatibility");
    }

    void testCheckedRenderContextSurfacesProjectionFailure()
    {
        InvalidProjection projection;
        const l2d::RenderContext2D context{1.f, &projection, l2d::RenderPass2D::World};
        const sf::Vector2f position{12.f, 34.f};

        L2D_REQUIRE(!context.tryWorldToRender(position).has_value());
        L2D_REQUIRE(!context.tryRenderToWorld(position).has_value());
        L2D_REQUIRE(!context.tryDepthFor(position).has_value());

        // Compatibility adapters retain the pre-audit fallback behavior.
        L2D_REQUIRE_EQUAL(context.worldToRender(position), position);
        L2D_REQUIRE_EQUAL(context.renderToWorld(position), position);
        L2D_REQUIRE_EQUAL(context.depthFor(position), 0.f);

        const l2d::RenderContext2D identity{1.f, nullptr, l2d::RenderPass2D::World};
        const auto identityProjected = identity.tryWorldToRender(position);
        L2D_REQUIRE(identityProjected.has_value());
        L2D_REQUIRE_EQUAL(*identityProjected, position);

        const float nan = std::numeric_limits<float>::quiet_NaN();
        L2D_REQUIRE(!identity.tryWorldToRender({nan, 0.f}).has_value());
        L2D_REQUIRE(!identity.tryDepthFor({0.f, nan}).has_value());
    }

    void testCheckedCameraMutationsReportRejection()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::Camera2D camera({100.f, 100.f});
        const sf::Vector2f initialCenter = camera.center();

        L2D_REQUIRE(!camera.trySetCenter({nan, 0.f}));
        L2D_REQUIRE_EQUAL(camera.center(), initialCenter);

        L2D_REQUIRE(camera.trySetCenter({20.f, 30.f}));
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(20.f, 30.f));

        L2D_REQUIRE(!camera.tryMove({infinity, 0.f}));
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(20.f, 30.f));
        L2D_REQUIRE(camera.tryMove({5.f, -5.f}));
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(25.f, 25.f));

        L2D_REQUIRE(!camera.trySetBounds({0.f, nan}, {100.f, 100.f}));
        L2D_REQUIRE(!camera.hasBounds());
        L2D_REQUIRE(camera.trySetBounds({100.f, 80.f}, {0.f, 0.f}));
        L2D_REQUIRE(camera.hasBounds());

        camera.clearBounds();
        L2D_REQUIRE(!camera.tryFollow({50.f, 50.f}, 0.f));
        L2D_REQUIRE(camera.tryFollow({50.f, 50.f}, 1.f));
    }

    l2d::Prefab assetBackedPrefab()
    {
        l2d::Prefab prefab;
        prefab.name = "asset-backed";
        l2d::SpriteRendererPrefab sprite;
        sprite.texture = "missing-texture";
        sprite.textureRect = sf::IntRect({0, 0}, {16, 16});
        prefab.spriteRenderer = sprite;
        return prefab;
    }

    void testLevelInstantiationRollsBackPartialBatch()
    {
        l2d::Scene scene;
        scene.createGameObject("pre-existing");

        l2d::LevelDocument level;
        l2d::Prefab first;
        first.name = "first";
        level.objects.push_back(first);
        level.objects.push_back(assetBackedPrefab());

        bool rejected = false;
        try
        {
            (void)l2d::LevelSerializer::instantiate(scene, level);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }

        L2D_REQUIRE(rejected);
        L2D_REQUIRE_EQUAL(scene.gameObjectCount(), 1u);
        L2D_REQUIRE(scene.findGameObjectByName("pre-existing") != nullptr);
        L2D_REQUIRE(scene.findGameObjectByName("first") == nullptr);
    }

    void testLevelRollbackPreservesExistingDestroyQueue()
    {
        l2d::Scene scene;
        l2d::GameObject& preQueued = scene.createGameObject("pre-queued");
        preQueued.destroy();

        L2D_REQUIRE_EQUAL(scene.gameObjectCount(), 1u);
        L2D_REQUIRE_EQUAL(scene.destroyQueuedGameObjectCount(), 1u);

        l2d::ComponentCodecRegistry codecs;
        L2D_REQUIRE(codecs.registerCodec(
            "audit.failure", 1u,
            [](const l2d::GameObject&) { return std::optional<std::string>("{}"); },
            [](l2d::GameObject& object, const std::string&)
            {
                object.addComponent<AuditMarker>();
                return false;
            }));

        l2d::Prefab prefab;
        prefab.name = "codec-failure";
        prefab.customComponents.push_back({"audit.failure", 1u, true, "{}"});

        l2d::LevelDocument level;
        level.objects.push_back(prefab);

        l2d::AssetManager assets;
        bool rejected = false;
        try
        {
            (void)l2d::LevelSerializer::instantiate(scene, level, assets, &codecs);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }

        L2D_REQUIRE(rejected);
        L2D_REQUIRE_EQUAL(scene.gameObjectCount(), 1u);
        L2D_REQUIRE_EQUAL(scene.destroyQueuedGameObjectCount(), 1u);
        L2D_REQUIRE(scene.findGameObjectByName("codec-failure") == nullptr);

        scene.destroyQueuedGameObjects();
        L2D_REQUIRE_EQUAL(scene.gameObjectCount(), 0u);
    }
}

int main()
{
    int failures = 0;

    runTest("input snapshot borrowing rejects temporaries",
            testInputSnapshotBorrowingRejectsTemporaryConstruction, failures);
    runTest("checked legacy action map reports failure", testCheckedLegacyActionMapReportsFailure,
            failures);
    runTest("checked render context surfaces projection failure",
            testCheckedRenderContextSurfacesProjectionFailure, failures);
    runTest("checked camera mutations report rejection", testCheckedCameraMutationsReportRejection,
            failures);
    runTest("level instantiation rolls back partial batch",
            testLevelInstantiationRollsBackPartialBatch, failures);
    runTest("level rollback preserves existing destroy queue",
            testLevelRollbackPreservesExistingDestroyQueue, failures);

    return failures == 0 ? 0 : 1;
}

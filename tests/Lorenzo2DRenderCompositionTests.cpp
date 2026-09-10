#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/RenderComposition2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "TestSupport.hpp"

#include <iostream>
#include <limits>
#include <string>

namespace
{
    using l2d::test::runTest;

    void testLayerRangeContract()
    {
        const l2d::RenderLayerRange2D all;
        L2D_REQUIRE(all.isValid());
        L2D_REQUIRE(all.contains(std::numeric_limits<std::int32_t>::min()));
        L2D_REQUIRE(all.contains(0));
        L2D_REQUIRE(all.contains(std::numeric_limits<std::int32_t>::max()));

        const l2d::RenderLayerRange2D bounded{-3, 7};
        L2D_REQUIRE(bounded.isValid());
        L2D_REQUIRE(bounded.contains(-3));
        L2D_REQUIRE(bounded.contains(7));
        L2D_REQUIRE(!bounded.contains(-4));
        L2D_REQUIRE(!bounded.contains(8));

        const l2d::RenderLayerRange2D invalid{2, 1};
        L2D_REQUIRE(!invalid.isValid());
        L2D_REQUIRE(!invalid.contains(1));
        L2D_REQUIRE(!invalid.contains(2));
    }

    void testCompositionValidationAndOrdering()
    {
        l2d::Camera2D worldCamera({640.f, 360.f});
        worldCamera.setCenter({320.f, 180.f});
        l2d::Camera2D minimapCamera({200.f, 200.f});
        minimapCamera.setCenter({50.f, 75.f});

        l2d::RenderCompositionEntry2D world;
        world.name = "world";
        world.camera = &worldCamera;
        world.pass = l2d::RenderPass2D::World;
        world.layers = {-10, 20};
        L2D_REQUIRE(l2d::RenderComposition2D::isValidEntry(world));

        l2d::RenderCompositionEntry2D minimap;
        minimap.name = "minimap";
        minimap.camera = &minimapCamera;
        minimap.pass = l2d::RenderPass2D::World;
        minimap.layers = {0, 5};
        minimap.viewport = sf::FloatRect({0.75f, 0.f}, {0.25f, 0.25f});
        L2D_REQUIRE(l2d::RenderComposition2D::isValidEntry(minimap));

        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(world));
        L2D_REQUIRE(composition.addEntry(minimap));
        L2D_REQUIRE_EQUAL(composition.entryCount(), 2u);
        L2D_REQUIRE(composition.entry(0u) != nullptr);
        L2D_REQUIRE_EQUAL(composition.entry(0u)->name, std::string("world"));
        L2D_REQUIRE_EQUAL(composition.entry(1u)->name, std::string("minimap"));

        l2d::RenderCompositionEntry2D duplicate = minimap;
        duplicate.camera = &worldCamera;
        L2D_REQUIRE(!composition.addEntry(duplicate));
        L2D_REQUIRE_EQUAL(composition.entryCount(), 2u);

        L2D_REQUIRE(composition.moveEntry(1u, 0u));
        L2D_REQUIRE_EQUAL(composition.entry(0u)->name, std::string("minimap"));
        L2D_REQUIRE_EQUAL(composition.entry(1u)->name, std::string("world"));
        L2D_REQUIRE(!composition.moveEntry(2u, 0u));

        l2d::RenderCompositionFrame2D frame;
        frame.interpolationAlpha = 0.25f;
        L2D_REQUIRE(l2d::RenderComposition2D::isValidFrame(frame));
        const auto context = composition.makeContext(0u, frame);
        L2D_REQUIRE(context.has_value());
        L2D_REQUIRE_APPROX(context->interpolationAlpha, 0.25f, 0.0001f);
        L2D_REQUIRE(context->pass == l2d::RenderPass2D::World);
        L2D_REQUIRE_EQUAL(context->layers.minimum, 0);
        L2D_REQUIRE_EQUAL(context->layers.maximum, 5);

        const auto view = composition.makeView(0u);
        L2D_REQUIRE(view.has_value());
        L2D_REQUIRE_APPROX_2D(view->getCenter(), minimapCamera.view().getCenter(), 0.0001f);
        L2D_REQUIRE_APPROX_2D(view->getSize(), minimapCamera.view().getSize(), 0.0001f);
        L2D_REQUIRE_APPROX_2D(view->getViewport().position, sf::Vector2f(0.75f, 0.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(view->getViewport().size, sf::Vector2f(0.25f, 0.25f), 0.0001f);

        L2D_REQUIRE(composition.setEntryEnabled(0u, false));
        L2D_REQUIRE(!composition.makeContext(0u, frame).has_value());
        L2D_REQUIRE(!composition.makeView(0u).has_value());
        L2D_REQUIRE(composition.setEntryEnabled(0u, true));

        frame.interpolationAlpha = 1.5f;
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidFrame(frame));
        L2D_REQUIRE(!composition.makeContext(0u, frame).has_value());

        l2d::RenderCompositionEntry2D invalid = world;
        invalid.name.clear();
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidEntry(invalid));
        invalid = world;
        invalid.camera = nullptr;
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidEntry(invalid));
        invalid = world;
        invalid.pass = l2d::RenderPass2D::Count;
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidEntry(invalid));
        invalid = world;
        invalid.layers = {2, 1};
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidEntry(invalid));
        invalid = world;
        invalid.viewport = sf::FloatRect({0.9f, 0.f}, {0.2f, 1.f});
        L2D_REQUIRE(!l2d::RenderComposition2D::isValidEntry(invalid));

        L2D_REQUIRE(composition.removeEntry(1u));
        L2D_REQUIRE_EQUAL(composition.entryCount(), 1u);
        L2D_REQUIRE(!composition.removeEntry(1u));
        L2D_REQUIRE(composition.clear());
        L2D_REQUIRE_EQUAL(composition.entryCount(), 0u);
    }

    void testCompositionCapacityIsTransactional()
    {
        l2d::Camera2D camera;
        l2d::RenderComposition2D composition;

        for (std::size_t index = 0u; index < l2d::RenderComposition2D::MaximumEntryCount; ++index)
        {
            l2d::RenderCompositionEntry2D entry;
            entry.name = "entry-" + std::to_string(index);
            entry.camera = &camera;
            L2D_REQUIRE(composition.addEntry(std::move(entry)));
        }

        L2D_REQUIRE_EQUAL(composition.entryCount(), l2d::RenderComposition2D::MaximumEntryCount);

        l2d::RenderCompositionEntry2D overflow;
        overflow.name = "overflow";
        overflow.camera = &camera;
        L2D_REQUIRE(!composition.addEntry(std::move(overflow)));
        L2D_REQUIRE_EQUAL(composition.entryCount(), l2d::RenderComposition2D::MaximumEntryCount);
    }

    void testRenderQueueLayerFiltering()
    {
        l2d::Scene scene("composition-layers");
        scene.createGameObject("default-layer");

        l2d::GameObject& lower = scene.createGameObject("lower");
        lower.addComponent<l2d::RenderOrder2D>().setLayer(-2);

        l2d::GameObject& upper = scene.createGameObject("upper");
        upper.addComponent<l2d::RenderOrder2D>().setLayer(2);

        l2d::GameObject& ui = scene.createGameObject("ui");
        l2d::RenderOrder2D& uiOrder = ui.addComponent<l2d::RenderOrder2D>();
        uiOrder.setPass(l2d::RenderPass2D::UI);
        uiOrder.setLayer(1);

        l2d::RenderContext2D context;
        context.pass = l2d::RenderPass2D::World;
        context.layers = {-2, 2};

        l2d::RenderQueue2D queue;
        queue.build(scene, context);
        L2D_REQUIRE_EQUAL(queue.size(), 3u);
        L2D_REQUIRE_EQUAL(queue.entries()[0].sortKey.layer, -2);
        L2D_REQUIRE_EQUAL(queue.entries()[1].sortKey.layer, 0);
        L2D_REQUIRE_EQUAL(queue.entries()[2].sortKey.layer, 2);

        context.layers = {2, 2};
        queue.build(scene, context);
        L2D_REQUIRE_EQUAL(queue.size(), 1u);
        L2D_REQUIRE_EQUAL(queue.entries()[0].sortKey.layer, 2);

        context.layers = {5, 4};
        queue.build(scene, context);
        L2D_REQUIRE(queue.empty());
    }
}

int main()
{
    int failures = 0;
    runTest("render layer range contract", testLayerRangeContract, failures);
    runTest("composition validation and ordering", testCompositionValidationAndOrdering, failures);
    runTest("composition capacity is transactional", testCompositionCapacityIsTransactional,
            failures);
    runTest("render queue layer filtering", testRenderQueueLayerFiltering, failures);

    if (failures != 0)
    {
        std::cerr << failures << " render composition test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D render composition tests passed.\n";
    return 0;
}

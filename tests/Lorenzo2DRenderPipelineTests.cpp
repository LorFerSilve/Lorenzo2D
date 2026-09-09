#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RenderPipeline2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/VideoMode.hpp>

#include "TestSupport.hpp"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using l2d::test::runTest;

    l2d::RenderPipelinePass2D backbufferPass(std::string name)
    {
        l2d::RenderPipelinePass2D pass;
        pass.name = std::move(name);
        pass.target = l2d::RenderPipelinePassTarget2D::Backbuffer;
        return pass;
    }

    class LegacyRenderCounter final : public l2d::Component
    {
      public:
        void onRender(sf::RenderWindow&, const l2d::RenderContext2D& context) override
        {
            ++calls;
            lastPass = context.pass;
        }

        int calls = 0;
        l2d::RenderPass2D lastPass = l2d::RenderPass2D::Count;
    };

    void testPipelineValidationAndLimitsAreCpuOnly()
    {
        static_assert(!std::is_copy_constructible_v<l2d::RenderPipeline2D>);
        static_assert(!std::is_copy_assignable_v<l2d::RenderPipeline2D>);
        static_assert(!std::is_move_constructible_v<l2d::RenderPipeline2D>);
        static_assert(!std::is_move_assignable_v<l2d::RenderPipeline2D>);

        L2D_REQUIRE(l2d::RenderPipeline2D::isValidFrame({1.f, nullptr}));
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidFrame(
            {std::numeric_limits<float>::quiet_NaN(), nullptr}));
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidFrame({-0.01f, nullptr}));
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidFrame({1.01f, nullptr}));

        l2d::RenderPipelinePass2D pass = backbufferPass("main");
        L2D_REQUIRE(l2d::RenderPipeline2D::isValidPass(pass));

        pass.name.clear();
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        pass = backbufferPass(std::string(l2d::RenderPipeline2D::MaximumPassNameBytes + 1u, 'x'));
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        pass = backbufferPass("invalid-pass");
        pass.contextPass = l2d::RenderPass2D::Count;
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        pass = backbufferPass("invalid-target");
        pass.target = static_cast<l2d::RenderPipelinePassTarget2D>(255);
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        pass = backbufferPass("backbuffer-present");
        pass.present.enabled = true;
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        auto surface = std::make_shared<l2d::RenderSurface2D>();
        pass = backbufferPass("surface-output");
        pass.target = l2d::RenderPipelinePassTarget2D::Surface;
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        pass.surface = surface;
        L2D_REQUIRE(l2d::RenderPipeline2D::isValidPass(pass));

        pass.inputs.push_back(surface);
        L2D_REQUIRE(!l2d::RenderPipeline2D::isValidPass(pass));

        l2d::RenderPipeline2D pipeline;
        for (std::size_t index = 0u; index < l2d::RenderPipeline2D::MaximumPassCount; ++index)
        {
            L2D_REQUIRE(pipeline.addPass(backbufferPass("pass-" + std::to_string(index)),
                                         [](const l2d::RenderPipelineExecution2D&)
                                         { return true; }));
        }

        L2D_REQUIRE_EQUAL(pipeline.passCount(), l2d::RenderPipeline2D::MaximumPassCount);
        L2D_REQUIRE(!pipeline.addPass(backbufferPass("overflow"),
                                      [](const l2d::RenderPipelineExecution2D&) { return true; }));
        L2D_REQUIRE(!pipeline.addPass(backbufferPass("pass-0"),
                                      [](const l2d::RenderPipelineExecution2D&) { return true; }));
        L2D_REQUIRE(pipeline.clear());
        L2D_REQUIRE_EQUAL(pipeline.passCount(), 0u);
    }

    void testPipelineOrderDisableAndMoveAreDeterministic()
    {
        l2d::RenderSurface2D backbuffer;
        L2D_REQUIRE(backbuffer.create({{2u, 2u}, false, false}));

        l2d::RenderPipeline2D pipeline;
        std::vector<int> order;

        L2D_REQUIRE(pipeline.addPass(backbufferPass("one"),
                                     [&order](const l2d::RenderPipelineExecution2D&)
                                     {
                                         order.push_back(1);
                                         return true;
                                     }));
        L2D_REQUIRE(pipeline.addPass(backbufferPass("two"),
                                     [&order](const l2d::RenderPipelineExecution2D&)
                                     {
                                         order.push_back(2);
                                         return true;
                                     }));
        L2D_REQUIRE(pipeline.addPass(backbufferPass("three"),
                                     [&order](const l2d::RenderPipelineExecution2D&)
                                     {
                                         order.push_back(3);
                                         return true;
                                     }));

        L2D_REQUIRE(pipeline.movePass(2u, 0u));
        L2D_REQUIRE_EQUAL(pipeline.pass(0u)->name, std::string("three"));
        L2D_REQUIRE_EQUAL(pipeline.pass(1u)->name, std::string("one"));
        L2D_REQUIRE_EQUAL(pipeline.pass(2u)->name, std::string("two"));

        L2D_REQUIRE(pipeline.setPassEnabled(1u, false));
        const l2d::RenderPipelineResult2D result = pipeline.execute(*backbuffer.target());

        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedPasses, 2u);
        L2D_REQUIRE(!result.failedPass.has_value());
        L2D_REQUIRE_EQUAL(order.size(), 2u);
        L2D_REQUIRE_EQUAL(order[0], 3);
        L2D_REQUIRE_EQUAL(order[1], 2);
    }

    void testPassViewStateIsRestored()
    {
        l2d::RenderSurface2D backbuffer;
        L2D_REQUIRE(backbuffer.create({{8u, 8u}, false, false}));

        const sf::View initialView = backbuffer.target()->getView();
        bool secondSawInitialView = false;

        l2d::RenderPipeline2D pipeline;
        L2D_REQUIRE(pipeline.addPass(backbufferPass("change-view"),
                                     [](const l2d::RenderPipelineExecution2D& execution)
                                     {
                                         sf::View changed = execution.target.getView();
                                         changed.setCenter({123.f, 456.f});
                                         execution.target.setView(changed);
                                         return true;
                                     }));
        L2D_REQUIRE(pipeline.addPass(
            backbufferPass("verify-view"),
            [&initialView, &secondSawInitialView](const l2d::RenderPipelineExecution2D& execution)
            {
                secondSawInitialView =
                    execution.target.getView().getCenter() == initialView.getCenter() &&
                    execution.target.getView().getSize() == initialView.getSize();
                return true;
            }));

        const l2d::RenderPipelineResult2D result = pipeline.execute(*backbuffer.target());
        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE(secondSawInitialView);
        L2D_REQUIRE_EQUAL(backbuffer.target()->getView().getCenter(), initialView.getCenter());
        L2D_REQUIRE_EQUAL(backbuffer.target()->getView().getSize(), initialView.getSize());
    }

    void testPreflightRejectsUnavailableOutputBeforeDrawing()
    {
        l2d::RenderSurface2D backbuffer;
        L2D_REQUIRE(backbuffer.create({{2u, 2u}, false, false}));

        bool firstExecuted = false;
        l2d::RenderPipeline2D pipeline;

        L2D_REQUIRE(pipeline.addPass(backbufferPass("first"),
                                     [&firstExecuted](const l2d::RenderPipelineExecution2D&)
                                     {
                                         firstExecuted = true;
                                         return true;
                                     }));

        auto unavailable = std::make_shared<l2d::RenderSurface2D>();
        l2d::RenderPipelinePass2D second = backbufferPass("unavailable-output");
        second.target = l2d::RenderPipelinePassTarget2D::Surface;
        second.surface = unavailable;
        L2D_REQUIRE(pipeline.addPass(std::move(second),
                                     [](const l2d::RenderPipelineExecution2D&) { return true; }));

        const l2d::RenderPipelineResult2D result = pipeline.execute(*backbuffer.target());

        L2D_REQUIRE(!result.succeeded());
        L2D_REQUIRE(result.failure == l2d::RenderPipelineFailure2D::SurfaceUnavailable);
        L2D_REQUIRE(result.failedPass == std::optional<std::size_t>(1u));
        L2D_REQUIRE_EQUAL(result.completedPasses, 0u);
        L2D_REQUIRE(!firstExecuted);
    }

    void testSurfaceInputsPublishAndPresentInOrder()
    {
        auto first = std::make_shared<l2d::RenderSurface2D>();
        auto second = std::make_shared<l2d::RenderSurface2D>();
        l2d::RenderSurface2D backbuffer;

        L2D_REQUIRE(first->create({{2u, 2u}, false, false}));
        L2D_REQUIRE(second->create({{2u, 2u}, false, false}));
        L2D_REQUIRE(backbuffer.create({{4u, 4u}, false, false}));
        L2D_REQUIRE(backbuffer.clear(sf::Color::Black));

        l2d::RenderPipeline2D pipeline;

        l2d::RenderPipelinePass2D firstPass = backbufferPass("produce-red");
        firstPass.target = l2d::RenderPipelinePassTarget2D::Surface;
        firstPass.surface = first;
        firstPass.clear = {true, sf::Color::Red};
        L2D_REQUIRE(pipeline.addPass(std::move(firstPass),
                                     [](const l2d::RenderPipelineExecution2D&) { return true; }));

        l2d::RenderPipelinePass2D secondPass = backbufferPass("copy-and-present");
        secondPass.target = l2d::RenderPipelinePassTarget2D::Surface;
        secondPass.surface = second;
        secondPass.inputs.push_back(first);
        secondPass.clear = {true, sf::Color::Black};
        secondPass.present.enabled = true;
        secondPass.present.presentation.position = {1.f, 1.f};
        secondPass.present.presentation.size = sf::Vector2f{2.f, 2.f};

        L2D_REQUIRE(pipeline.addPass(std::move(secondPass),
                                     [](const l2d::RenderPipelineExecution2D& execution)
                                     {
                                         return execution.inputs.size() == 1u &&
                                                execution.inputs[0]->present(execution.target);
                                     }));

        const l2d::RenderPipelineResult2D result = pipeline.execute(*backbuffer.target());
        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedPasses, 2u);
        L2D_REQUIRE_EQUAL(first->contentGeneration(), 1u);
        L2D_REQUIRE_EQUAL(second->contentGeneration(), 1u);

        L2D_REQUIRE(backbuffer.display());
        const sf::Image image = backbuffer.texture()->copyToImage();
        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color::Black);
        L2D_REQUIRE_EQUAL(image.getPixel({1u, 1u}), sf::Color::Red);
        L2D_REQUIRE_EQUAL(image.getPixel({2u, 2u}), sf::Color::Red);
        L2D_REQUIRE_EQUAL(image.getPixel({3u, 3u}), sf::Color::Black);
    }

    void testExternalInputRequiresPublishedContent()
    {
        auto input = std::make_shared<l2d::RenderSurface2D>();
        l2d::RenderSurface2D backbuffer;
        L2D_REQUIRE(input->create({{2u, 2u}, false, false}));
        L2D_REQUIRE(backbuffer.create({{2u, 2u}, false, false}));

        bool executed = false;
        l2d::RenderPipelinePass2D pass = backbufferPass("consume");
        pass.inputs.push_back(input);

        l2d::RenderPipeline2D pipeline;
        L2D_REQUIRE(pipeline.addPass(std::move(pass),
                                     [&executed](const l2d::RenderPipelineExecution2D&)
                                     {
                                         executed = true;
                                         return true;
                                     }));

        const l2d::RenderPipelineResult2D unpublished = pipeline.execute(*backbuffer.target());
        L2D_REQUIRE(unpublished.failure == l2d::RenderPipelineFailure2D::InputUnavailable);
        L2D_REQUIRE(!executed);

        L2D_REQUIRE(input->clear(sf::Color::Green));
        L2D_REQUIRE(input->display());

        const l2d::RenderPipelineResult2D published = pipeline.execute(*backbuffer.target());
        L2D_REQUIRE(published.succeeded());
        L2D_REQUIRE(executed);
    }

    void testCallbackFailureAndReentrancyAreBounded()
    {
        l2d::RenderSurface2D backbuffer;
        L2D_REQUIRE(backbuffer.create({{2u, 2u}, false, false}));

        l2d::RenderPipeline2D pipeline;
        l2d::RenderPipelineResult2D nested;
        bool mutationRejected = false;
        bool laterExecuted = false;

        L2D_REQUIRE(pipeline.addPass(
            backbufferPass("failing"),
            [&pipeline, &nested, &mutationRejected](const l2d::RenderPipelineExecution2D& execution)
            {
                nested = pipeline.execute(execution.target);
                mutationRejected =
                    !pipeline.addPass(backbufferPass("illegal-during-execute"),
                                      [](const l2d::RenderPipelineExecution2D&) { return true; }) &&
                    !pipeline.clear();
                return false;
            }));

        L2D_REQUIRE(pipeline.addPass(backbufferPass("later"),
                                     [&laterExecuted](const l2d::RenderPipelineExecution2D&)
                                     {
                                         laterExecuted = true;
                                         return true;
                                     }));

        const l2d::RenderPipelineResult2D result = pipeline.execute(*backbuffer.target());

        L2D_REQUIRE(result.failure == l2d::RenderPipelineFailure2D::CallbackFailed);
        L2D_REQUIRE(result.failedPass == std::optional<std::size_t>(0u));
        L2D_REQUIRE_EQUAL(result.completedPasses, 0u);
        L2D_REQUIRE(nested.failure == l2d::RenderPipelineFailure2D::ReentrantExecution);
        L2D_REQUIRE(mutationRejected);
        L2D_REQUIRE(!laterExecuted);
        L2D_REQUIRE(!pipeline.executing());
    }

    void testLegacySceneBridgeRequiresWindowAndDispatches()
    {
        l2d::RenderSurface2D genericBackbuffer;
        L2D_REQUIRE(genericBackbuffer.create({{4u, 4u}, false, false}));

        l2d::RenderPipeline2D pipeline;
        l2d::RenderPipelinePass2D legacy = backbufferPass("legacy-world");
        legacy.contextPass = l2d::RenderPass2D::World;
        legacy.clear = {true, sf::Color::Black};
        L2D_REQUIRE(pipeline.addLegacyScenePass(std::move(legacy)));
        L2D_REQUIRE(pipeline.isLegacyScenePass(0u));

        const l2d::RenderPipelineResult2D generic = pipeline.execute(*genericBackbuffer.target());
        L2D_REQUIRE(generic.failure == l2d::RenderPipelineFailure2D::LegacySceneRequired);
        L2D_REQUIRE(generic.failedPass == std::optional<std::size_t>(0u));
        L2D_REQUIRE_EQUAL(generic.completedPasses, 0u);

        l2d::Scene scene("pipeline-legacy");
        l2d::GameObject& object = scene.createGameObject("counter");
        LegacyRenderCounter& counter = object.addComponent<LegacyRenderCounter>();

        sf::RenderWindow window(sf::VideoMode({8u, 8u}), "Lorenzo2D render pipeline test");
        window.setVisible(false);
        L2D_REQUIRE(window.isOpen());

        const l2d::RenderPipelineResult2D windowResult = pipeline.execute(window, scene);
        L2D_REQUIRE(windowResult.succeeded());
        L2D_REQUIRE_EQUAL(windowResult.completedPasses, 1u);
        L2D_REQUIRE_EQUAL(counter.calls, 1);
        L2D_REQUIRE(counter.lastPass == l2d::RenderPass2D::World);

        window.close();
    }

    void testFailureNamesAreStable()
    {
        L2D_REQUIRE_EQUAL(l2d::renderPipelineFailureName(l2d::RenderPipelineFailure2D::None),
                          std::string_view("none"));
        L2D_REQUIRE_EQUAL(
            l2d::renderPipelineFailureName(l2d::RenderPipelineFailure2D::LegacySceneRequired),
            std::string_view("legacy-scene-required"));
        L2D_REQUIRE_EQUAL(
            l2d::renderPipelineFailureName(static_cast<l2d::RenderPipelineFailure2D>(255)),
            std::string_view("unknown"));
    }
}

int main()
{
    int failures = 0;

    runTest("pipeline validation and limits are CPU-only",
            testPipelineValidationAndLimitsAreCpuOnly, failures);
    runTest("pipeline order disable and move are deterministic",
            testPipelineOrderDisableAndMoveAreDeterministic, failures);
    runTest("pass view state is restored", testPassViewStateIsRestored, failures);
    runTest("preflight rejects unavailable output before drawing",
            testPreflightRejectsUnavailableOutputBeforeDrawing, failures);
    runTest("surface inputs publish and present in order",
            testSurfaceInputsPublishAndPresentInOrder, failures);
    runTest("external input requires published content", testExternalInputRequiresPublishedContent,
            failures);
    runTest("callback failure and reentrancy are bounded",
            testCallbackFailureAndReentrancyAreBounded, failures);
    runTest("legacy scene bridge requires window and dispatches",
            testLegacySceneBridgeRequiresWindowAndDispatches, failures);
    runTest("failure names are stable", testFailureNamesAreStable, failures);

    return failures == 0 ? 0 : 1;
}

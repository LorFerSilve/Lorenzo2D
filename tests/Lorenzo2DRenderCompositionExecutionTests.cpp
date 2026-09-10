#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/RenderComposition2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/VideoMode.hpp>

#include "TestSupport.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using l2d::test::runTest;

    bool sameView(const sf::View& left, const sf::View& right)
    {
        return left.getCenter() == right.getCenter() && left.getSize() == right.getSize() &&
               left.getViewport() == right.getViewport();
    }

    l2d::RenderCompositionEntry2D entryFor(std::string name, const l2d::Camera2D& camera)
    {
        l2d::RenderCompositionEntry2D entry;
        entry.name = std::move(name);
        entry.camera = &camera;
        return entry;
    }

    struct LegacyObservation
    {
        std::string label;
        l2d::RenderPass2D pass = l2d::RenderPass2D::Count;
        l2d::RenderLayerRange2D layers;
        sf::Vector2f center;
        sf::FloatRect viewport;
    };

    class LegacyProbe final : public l2d::Component
    {
      public:
        LegacyProbe(std::vector<LegacyObservation>& observations, std::string label)
            : m_observations(observations), m_label(std::move(label))
        {
        }

        void onRender(sf::RenderWindow& window, const l2d::RenderContext2D& context) override
        {
            m_observations.push_back(
                {m_label, context.pass, context.layers, window.getView().getCenter(),
                 window.getView().getViewport()});
        }

      private:
        std::vector<LegacyObservation>& m_observations;
        std::string m_label;
    };

    void testGenericExecutionUsesOrderedCameraSnapshots()
    {
        l2d::RenderSurface2D target;
        L2D_REQUIRE(target.create({{64u, 32u}, false, false}));

        sf::View incomingView = target.target()->getView();
        incomingView.setCenter({11.f, 13.f});
        incomingView.setSize({29.f, 31.f});
        incomingView.setViewport(sf::FloatRect({0.1f, 0.1f}, {0.8f, 0.8f}));
        target.target()->setView(incomingView);

        l2d::Camera2D firstCamera({200.f, 100.f});
        firstCamera.setCenter({100.f, 50.f});
        l2d::Camera2D secondCamera({80.f, 60.f});
        secondCamera.setCenter({300.f, 400.f});

        l2d::RenderCompositionEntry2D first = entryFor("first", firstCamera);
        first.layers = {-3, 0};
        first.viewport = sf::FloatRect({0.f, 0.f}, {0.5f, 1.f});

        l2d::RenderCompositionEntry2D disabled = entryFor("disabled", firstCamera);
        disabled.enabled = false;

        l2d::RenderCompositionEntry2D second = entryFor("second", secondCamera);
        second.pass = l2d::RenderPass2D::UI;
        second.layers = {4, 8};
        second.viewport = sf::FloatRect({0.5f, 0.f}, {0.5f, 1.f});

        sf::View expectedFirst = firstCamera.view();
        expectedFirst.setViewport(first.viewport);
        sf::View expectedSecond = secondCamera.view();
        expectedSecond.setViewport(second.viewport);

        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(first));
        L2D_REQUIRE(composition.addEntry(disabled));
        L2D_REQUIRE(composition.addEntry(second));

        l2d::RenderCompositionFrame2D frame;
        frame.interpolationAlpha = 0.25f;

        std::vector<std::size_t> indices;
        std::vector<std::string> names;
        bool firstViewMatched = false;
        bool secondViewMatchedSnapshot = false;
        bool contextMatched = true;

        const l2d::RenderCompositionResult2D result = composition.execute(
            *target.target(),
            [&](const l2d::RenderCompositionExecution2D& execution)
            {
                indices.push_back(execution.entryIndex);
                names.push_back(execution.entry.name);

                if (execution.entryIndex == 0u)
                {
                    firstViewMatched = sameView(execution.target.getView(), expectedFirst);
                    contextMatched =
                        contextMatched && execution.context.pass == l2d::RenderPass2D::World &&
                        execution.context.layers.minimum == -3 &&
                        execution.context.layers.maximum == 0 &&
                        execution.context.interpolationAlpha == 0.25f;

                    // Later entries use the view snapshot captured before the
                    // first callback rather than this mid-run camera mutation.
                    secondCamera.setCenter({999.f, 999.f});
                }
                else if (execution.entryIndex == 2u)
                {
                    secondViewMatchedSnapshot = sameView(execution.target.getView(), expectedSecond);
                    contextMatched =
                        contextMatched && execution.context.pass == l2d::RenderPass2D::UI &&
                        execution.context.layers.minimum == 4 &&
                        execution.context.layers.maximum == 8 &&
                        execution.context.interpolationAlpha == 0.25f;
                }
                else
                {
                    contextMatched = false;
                }

                return true;
            },
            frame);

        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedEntries, 2u);
        L2D_REQUIRE(!result.failedEntry.has_value());
        L2D_REQUIRE_EQUAL(indices.size(), 2u);
        L2D_REQUIRE_EQUAL(indices[0], 0u);
        L2D_REQUIRE_EQUAL(indices[1], 2u);
        L2D_REQUIRE_EQUAL(names[0], std::string("first"));
        L2D_REQUIRE_EQUAL(names[1], std::string("second"));
        L2D_REQUIRE(firstViewMatched);
        L2D_REQUIRE(secondViewMatchedSnapshot);
        L2D_REQUIRE(contextMatched);
        L2D_REQUIRE(sameView(target.target()->getView(), incomingView));
        L2D_REQUIRE(!composition.executing());
    }

    void testCallbackFailureReentrancyAndMutationAreBounded()
    {
        l2d::RenderSurface2D target;
        L2D_REQUIRE(target.create({{32u, 32u}, false, false}));
        const sf::View incomingView = target.target()->getView();

        l2d::Camera2D camera({32.f, 32.f});
        l2d::RenderCompositionEntry2D first = entryFor("first", camera);
        l2d::RenderCompositionEntry2D second = entryFor("second", camera);

        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(first));
        L2D_REQUIRE(composition.addEntry(second));

        l2d::RenderCompositionResult2D nested;
        bool mutationRejected = false;
        bool laterExecuted = false;

        const l2d::RenderCompositionResult2D result = composition.execute(
            *target.target(),
            [&](const l2d::RenderCompositionExecution2D& execution)
            {
                if (execution.entryIndex != 0u)
                {
                    laterExecuted = true;
                    return true;
                }

                nested = composition.execute(
                    execution.target,
                    [](const l2d::RenderCompositionExecution2D&) { return true; });

                l2d::RenderCompositionEntry2D illegal = entryFor("illegal", camera);
                mutationRejected = !composition.addEntry(std::move(illegal)) &&
                                   !composition.setEntryEnabled(1u, false) &&
                                   !composition.moveEntry(1u, 0u) &&
                                   !composition.removeEntry(1u) && !composition.clear();

                sf::View changed = execution.target.getView();
                changed.setCenter({123.f, 456.f});
                execution.target.setView(changed);
                return false;
            });

        L2D_REQUIRE(result.failure == l2d::RenderCompositionFailure2D::CallbackFailed);
        L2D_REQUIRE(result.failedEntry == std::optional<std::size_t>(0u));
        L2D_REQUIRE_EQUAL(result.completedEntries, 0u);
        L2D_REQUIRE(nested.failure == l2d::RenderCompositionFailure2D::ReentrantExecution);
        L2D_REQUIRE(mutationRejected);
        L2D_REQUIRE(!laterExecuted);
        L2D_REQUIRE(!composition.executing());
        L2D_REQUIRE(sameView(target.target()->getView(), incomingView));
        L2D_REQUIRE(composition.setEntryEnabled(1u, false));
        L2D_REQUIRE(composition.setEntryEnabled(1u, true));
    }

    void testInvalidExecutionInputsHaveNoSideEffects()
    {
        l2d::RenderSurface2D target;
        L2D_REQUIRE(target.create({{16u, 16u}, false, false}));
        const sf::View incomingView = target.target()->getView();

        l2d::Camera2D camera({16.f, 16.f});
        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(entryFor("main", camera)));

        const l2d::RenderCompositionResult2D missingCallback = composition.execute(
            *target.target(), l2d::RenderCompositionCallback2D{});
        L2D_REQUIRE(missingCallback.failure == l2d::RenderCompositionFailure2D::InvalidCallback);
        L2D_REQUIRE_EQUAL(missingCallback.completedEntries, 0u);
        L2D_REQUIRE(!missingCallback.failedEntry.has_value());
        L2D_REQUIRE(sameView(target.target()->getView(), incomingView));

        l2d::RenderCompositionFrame2D invalidFrame;
        invalidFrame.interpolationAlpha = std::numeric_limits<float>::quiet_NaN();
        bool callbackRan = false;
        const l2d::RenderCompositionResult2D invalid = composition.execute(
            *target.target(),
            [&callbackRan](const l2d::RenderCompositionExecution2D&)
            {
                callbackRan = true;
                return true;
            },
            invalidFrame);

        L2D_REQUIRE(invalid.failure == l2d::RenderCompositionFailure2D::InvalidFrame);
        L2D_REQUIRE_EQUAL(invalid.completedEntries, 0u);
        L2D_REQUIRE(!callbackRan);
        L2D_REQUIRE(sameView(target.target()->getView(), incomingView));
        L2D_REQUIRE(!composition.executing());
    }

    void testExceptionUnwindingRestoresExecutionStateAndView()
    {
        l2d::RenderSurface2D target;
        L2D_REQUIRE(target.create({{16u, 16u}, false, false}));
        const sf::View incomingView = target.target()->getView();

        l2d::Camera2D camera({16.f, 16.f});
        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(entryFor("throwing", camera)));

        bool threw = false;
        try
        {
            (void)composition.execute(
                *target.target(),
                [](const l2d::RenderCompositionExecution2D& execution) -> bool
                {
                    sf::View changed = execution.target.getView();
                    changed.setCenter({77.f, 88.f});
                    execution.target.setView(changed);
                    throw std::runtime_error("composition callback failure");
                });
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        L2D_REQUIRE(threw);
        L2D_REQUIRE(!composition.executing());
        L2D_REQUIRE(sameView(target.target()->getView(), incomingView));
    }

    void testLegacySceneBridgeFiltersLayersAndUsesEntryViews()
    {
        std::vector<LegacyObservation> observations;
        l2d::Scene scene("composition-legacy");

        l2d::GameObject& low = scene.createGameObject("low");
        low.addComponent<l2d::RenderOrder2D>().setLayer(0);
        low.addComponent<LegacyProbe>(observations, "low");

        l2d::GameObject& high = scene.createGameObject("high");
        high.addComponent<l2d::RenderOrder2D>().setLayer(5);
        high.addComponent<LegacyProbe>(observations, "high");

        l2d::GameObject& ui = scene.createGameObject("ui");
        l2d::RenderOrder2D& uiOrder = ui.addComponent<l2d::RenderOrder2D>();
        uiOrder.setPass(l2d::RenderPass2D::UI);
        uiOrder.setLayer(0);
        ui.addComponent<LegacyProbe>(observations, "ui");

        l2d::Camera2D lowCamera({40.f, 40.f});
        lowCamera.setCenter({20.f, 20.f});
        l2d::Camera2D highCamera({20.f, 20.f});
        highCamera.setCenter({100.f, 50.f});

        l2d::RenderCompositionEntry2D lowEntry = entryFor("low-view", lowCamera);
        lowEntry.layers = {0, 0};
        lowEntry.viewport = sf::FloatRect({0.f, 0.f}, {0.5f, 1.f});

        l2d::RenderCompositionEntry2D highEntry = entryFor("high-view", highCamera);
        highEntry.layers = {5, 5};
        highEntry.viewport = sf::FloatRect({0.5f, 0.f}, {0.5f, 1.f});

        l2d::RenderComposition2D composition;
        L2D_REQUIRE(composition.addEntry(lowEntry));
        L2D_REQUIRE(composition.addEntry(highEntry));

        sf::RenderWindow window(sf::VideoMode({80u, 40u}), "Lorenzo2D composition execution test");
        window.setVisible(false);
        L2D_REQUIRE(window.isOpen());

        sf::View incomingView = window.getView();
        incomingView.setCenter({7.f, 9.f});
        incomingView.setSize({17.f, 19.f});
        window.setView(incomingView);

        const l2d::RenderCompositionResult2D result = composition.execute(window, scene);

        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedEntries, 2u);
        L2D_REQUIRE_EQUAL(observations.size(), 2u);
        L2D_REQUIRE_EQUAL(observations[0].label, std::string("low"));
        L2D_REQUIRE_EQUAL(observations[1].label, std::string("high"));
        L2D_REQUIRE(observations[0].pass == l2d::RenderPass2D::World);
        L2D_REQUIRE(observations[1].pass == l2d::RenderPass2D::World);
        L2D_REQUIRE_EQUAL(observations[0].layers.minimum, 0);
        L2D_REQUIRE_EQUAL(observations[0].layers.maximum, 0);
        L2D_REQUIRE_EQUAL(observations[1].layers.minimum, 5);
        L2D_REQUIRE_EQUAL(observations[1].layers.maximum, 5);
        L2D_REQUIRE_APPROX_2D(observations[0].center, lowCamera.view().getCenter(), 0.0001f);
        L2D_REQUIRE_APPROX_2D(observations[1].center, highCamera.view().getCenter(), 0.0001f);
        L2D_REQUIRE_APPROX_2D(observations[0].viewport.position, sf::Vector2f(0.f, 0.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(observations[0].viewport.size, sf::Vector2f(0.5f, 1.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(observations[1].viewport.position, sf::Vector2f(0.5f, 0.f), 0.0001f);
        L2D_REQUIRE_APPROX_2D(observations[1].viewport.size, sf::Vector2f(0.5f, 1.f), 0.0001f);
        L2D_REQUIRE(sameView(window.getView(), incomingView));

        window.close();
    }

    void testFailureNamesAreStable()
    {
        L2D_REQUIRE_EQUAL(
            l2d::renderCompositionFailureName(l2d::RenderCompositionFailure2D::None),
            std::string_view("none"));
        L2D_REQUIRE_EQUAL(
            l2d::renderCompositionFailureName(l2d::RenderCompositionFailure2D::CallbackFailed),
            std::string_view("callback-failed"));
        L2D_REQUIRE_EQUAL(
            l2d::renderCompositionFailureName(static_cast<l2d::RenderCompositionFailure2D>(255)),
            std::string_view("unknown"));
    }
}

int main()
{
    int failures = 0;

    runTest("generic execution uses ordered camera snapshots",
            testGenericExecutionUsesOrderedCameraSnapshots, failures);
    runTest("callback failure reentrancy and mutation are bounded",
            testCallbackFailureReentrancyAndMutationAreBounded, failures);
    runTest("invalid execution inputs have no side effects",
            testInvalidExecutionInputsHaveNoSideEffects, failures);
    runTest("exception unwinding restores execution state and view",
            testExceptionUnwindingRestoresExecutionStateAndView, failures);
    runTest("legacy scene bridge filters layers and uses entry views",
            testLegacySceneBridgeFiltersLayersAndUsesEntryViews, failures);
    runTest("composition execution failure names are stable", testFailureNamesAreStable, failures);

    return failures == 0 ? 0 : 1;
}

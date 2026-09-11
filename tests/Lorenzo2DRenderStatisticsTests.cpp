#include "TestSupport.hpp"

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/RenderComposition2D.hpp>
#include <Lorenzo2D/Renderer/RenderPipeline2D.hpp>
#include <Lorenzo2D/Renderer/RenderStatistics2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>
#include <Lorenzo2D/Renderer/Shader2D.hpp>
#include <Lorenzo2D/Renderer/ShaderPostProcessChain2D.hpp>
#include <Lorenzo2D/Renderer/SpriteBatch2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace
{
    using l2d::test::runTest;

    constexpr const char* SimpleFragmentSource = R"(
void main()
{
    gl_FragColor = gl_Color;
}
)";

    l2d::TextureHandle makeTexture()
    {
        const sf::Image image({2u, 2u}, sf::Color::White);
        auto texture = std::make_shared<sf::Texture>();
        if (!texture->loadFromImage(image))
            throw std::runtime_error("unable to create render statistics test texture");
        return l2d::TextureHandle(std::move(texture));
    }

    void requireStats(const l2d::RenderStatistics2D& stats, std::size_t draws,
                      std::size_t primitives, std::size_t batches, std::size_t materialSwitches,
                      std::size_t shaderSwitches, std::size_t renderedItems,
                      std::size_t culledItems)
    {
        L2D_REQUIRE_EQUAL(stats.drawCallCount, draws);
        L2D_REQUIRE_EQUAL(stats.submittedPrimitiveCount, primitives);
        L2D_REQUIRE_EQUAL(stats.batchCount, batches);
        L2D_REQUIRE_EQUAL(stats.materialSwitchCount, materialSwitches);
        L2D_REQUIRE_EQUAL(stats.shaderSwitchCount, shaderSwitches);
        L2D_REQUIRE_EQUAL(stats.renderedItemCount, renderedItems);
        L2D_REQUIRE_EQUAL(stats.culledItemCount, culledItems);
    }

    void testRecorderOrderingResetAndSaturation()
    {
        auto first = std::make_shared<l2d::Material2D>();
        auto second = std::make_shared<l2d::Material2D>();
        l2d::RenderStatisticsRecorder2D recorder;

        recorder.recordDraw({2u, 1u, 1u, first});
        recorder.recordDraw({3u, 2u, 1u, first});
        recorder.recordRepeatedDraws(2u, {4u, 3u, 2u, second});
        recorder.recordCulledItems(5u);
        requireStats(recorder.statistics(), 4u, 9u, 4u, 1u, 0u, 6u, 5u);

        recorder.reset();
        requireStats(recorder.statistics(), 0u, 0u, 0u, 0u, 0u, 0u, 0u);

        const std::size_t maximum = std::numeric_limits<std::size_t>::max();
        recorder.recordRepeatedDraws(maximum, {maximum, maximum, maximum, nullptr});
        recorder.recordDraw({1u, 1u, 1u, nullptr});
        recorder.recordCulledItems(maximum);
        recorder.recordCulledItems(1u);
        requireStats(recorder.statistics(), maximum, maximum, maximum, 0u, 0u, maximum, maximum);
    }

    void testDestroyedStateIdentity()
    {
        alignas(l2d::Material2D) unsigned char storage[sizeof(l2d::Material2D)];
        l2d::RenderStatisticsRecorder2D recorder;

        auto* firstObject = new (storage) l2d::Material2D();
        l2d::Material2DHandle first(firstObject, [](l2d::Material2D*) {});
        recorder.recordDraw({2u, 1u, 1u, first});
        first.reset();
        firstObject->~Material2D();

        auto* secondObject = new (storage) l2d::Material2D();
        l2d::Material2DHandle second(secondObject, [](l2d::Material2D*) {});
        recorder.recordDraw({2u, 1u, 1u, second});

        requireStats(recorder.statistics(), 2u, 4u, 2u, 1u, 0u, 2u, 0u);

        second.reset();
        secondObject->~Material2D();
    }

    void testLegacyRenderMethodOverloads()
    {
        using SurfacePresent =
            bool (l2d::RenderSurface2D::*)(sf::RenderTarget&, const l2d::RenderSurfacePresent2D&)
                const;
        using BatchDraw =
            l2d::SpriteBatchDrawResult2D (l2d::SpriteBatch2D::*)(sf::RenderTarget&) const;
        using PostProcessApply = l2d::ShaderPostProcessResult2D (l2d::ShaderPostProcessChain2D::*)(
            const l2d::RenderSurface2D&, sf::RenderTarget&);

        const SurfacePresent surfacePresent =
            static_cast<SurfacePresent>(&l2d::RenderSurface2D::present);
        const BatchDraw batchDraw = static_cast<BatchDraw>(&l2d::SpriteBatch2D::draw);
        const PostProcessApply postProcessApply =
            static_cast<PostProcessApply>(&l2d::ShaderPostProcessChain2D::apply);

        L2D_REQUIRE(surfacePresent != nullptr);
        L2D_REQUIRE(batchDraw != nullptr);
        L2D_REQUIRE(postProcessApply != nullptr);
    }

    void testShaderSwitchIdentity()
    {
        sf::RenderTexture context({2u, 2u});
        (void)context;

        if (!l2d::Shader2D::isSupported())
        {
            std::cout
                << "[SKIP] render statistics shader identity: shader runtime is unavailable\n";
            return;
        }

        auto firstShader = std::make_shared<l2d::Shader2D>();
        auto secondShader = std::make_shared<l2d::Shader2D>();
        L2D_REQUIRE(firstShader->loadFragmentSource(SimpleFragmentSource));
        L2D_REQUIRE(secondShader->loadFragmentSource(SimpleFragmentSource));

        auto firstMaterial = std::make_shared<l2d::Material2D>();
        auto sameShaderMaterial = std::make_shared<l2d::Material2D>();
        auto secondMaterial = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(firstMaterial->setShader(firstShader));
        L2D_REQUIRE(sameShaderMaterial->setShader(firstShader));
        L2D_REQUIRE(secondMaterial->setShader(secondShader));

        l2d::RenderStatisticsRecorder2D recorder;
        recorder.recordDraw({2u, 1u, 1u, firstMaterial});
        recorder.recordDraw({2u, 1u, 1u, sameShaderMaterial});
        recorder.recordDraw({2u, 1u, 1u, secondMaterial});
        recorder.recordDraw({2u, 1u, 1u, nullptr});
        requireStats(recorder.statistics(), 4u, 8u, 4u, 3u, 2u, 4u, 0u);
    }

    void testSpriteBatchStatistics()
    {
        const l2d::TextureHandle texture = makeTexture();
        l2d::SpriteBatch2D batch;

        l2d::SpriteBatchSubmission2D first;
        first.texture = texture;
        L2D_REQUIRE(batch.submit(first).succeeded());

        l2d::SpriteBatchSubmission2D second;
        second.texture = texture;
        second.position = {2.f, 0.f};
        L2D_REQUIRE(batch.submit(second).succeeded());
        L2D_REQUIRE_EQUAL(batch.batchCount(), 1u);

        sf::RenderTexture target({8u, 4u});
        l2d::RenderStatisticsRecorder2D recorder;
        const l2d::SpriteBatchDrawResult2D result = batch.draw(target, &recorder);

        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.drawCallCount, 1u);
        L2D_REQUIRE_EQUAL(result.renderedSpriteCount, 2u);
        requireStats(recorder.statistics(), 1u, 4u, 1u, 0u, 0u, 2u, 0u);
    }

    void testSpriteRendererStatistics()
    {
        const l2d::TextureHandle texture = makeTexture();
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("sprite");
        object.addComponent<l2d::SpriteRenderer>(texture);

        sf::RenderWindow window(sf::VideoMode({16u, 16u}), "Lorenzo2D render statistics test");
        window.setVisible(false);
        L2D_REQUIRE(window.isOpen());

        l2d::RenderStatisticsRecorder2D recorder;
        const l2d::RenderContext2D context{1.f, nullptr, l2d::RenderPass2D::World, {}, &recorder};
        scene.render(window, context);

        requireStats(recorder.statistics(), 1u, 2u, 1u, 0u, 0u, 1u, 0u);
        window.close();
    }

    void testSurfaceAndPostProcessStatistics()
    {
        l2d::RenderSurface2D source;
        L2D_REQUIRE(source.create({{4u, 4u}, false, false}));
        L2D_REQUIRE(source.clear(sf::Color::White));
        L2D_REQUIRE(source.display());

        sf::RenderTexture target({4u, 4u});
        l2d::RenderStatisticsRecorder2D recorder;
        L2D_REQUIRE(source.present(target, {}, &recorder));

        l2d::ShaderPostProcessChain2D postProcess;
        const l2d::ShaderPostProcessResult2D result = postProcess.apply(source, target, &recorder);
        L2D_REQUIRE(result.succeeded());

        requireStats(recorder.statistics(), 2u, 4u, 2u, 0u, 0u, 2u, 0u);
    }

    void testPipelineAndCompositionPropagation()
    {
        sf::RenderTexture target({8u, 8u});
        l2d::RenderStatisticsRecorder2D recorder;

        l2d::RenderPipeline2D pipeline;
        l2d::RenderPipelinePass2D pass;
        pass.name = "world";
        bool pipelineSawRecorder = false;
        L2D_REQUIRE(
            pipeline.addPass(pass,
                             [&pipelineSawRecorder](const l2d::RenderPipelineExecution2D& execution)
                             {
                                 pipelineSawRecorder = execution.context.statistics != nullptr;
                                 execution.context.statistics->recordDraw({3u, 2u, 1u, nullptr});
                                 return true;
                             }));

        l2d::RenderPipelineFrame2D pipelineFrame;
        pipelineFrame.statistics = &recorder;
        L2D_REQUIRE(pipeline.execute(target, pipelineFrame).succeeded());
        L2D_REQUIRE(pipelineSawRecorder);

        l2d::Camera2D camera({8.f, 8.f});
        l2d::RenderComposition2D composition;
        l2d::RenderCompositionEntry2D entry;
        entry.name = "main";
        entry.camera = &camera;
        L2D_REQUIRE(composition.addEntry(entry));

        bool compositionSawRecorder = false;
        l2d::RenderCompositionFrame2D compositionFrame;
        compositionFrame.statistics = &recorder;
        L2D_REQUIRE(
            composition
                .execute(
                    target,
                    [&compositionSawRecorder](const l2d::RenderCompositionExecution2D& execution)
                    {
                        compositionSawRecorder = execution.context.statistics != nullptr;
                        execution.context.statistics->recordDraw({2u, 1u, 1u, nullptr});
                        return true;
                    },
                    compositionFrame)
                .succeeded());
        L2D_REQUIRE(compositionSawRecorder);
        requireStats(recorder.statistics(), 2u, 5u, 2u, 0u, 0u, 3u, 0u);
    }

    void testTilemapAndDiagnosticAdapters()
    {
        l2d::TileMapRenderStats tilemap;
        tilemap.drawCallCount = 2u;
        tilemap.submittedTileCount = 3u;
        tilemap.submittedVertexCount = 18u;
        tilemap.culledChunkCount = 4u;

        auto material = std::make_shared<l2d::Material2D>();
        l2d::RenderStatisticsRecorder2D recorder;
        recorder.recordDraw({2u, 1u, 1u, material});
        l2d::recordTileMapRenderStatistics(tilemap, recorder);
        requireStats(recorder.statistics(), 3u, 8u, 3u, 1u, 0u, 4u, 4u);

        l2d::DiagnosticCounters counters;
        l2d::accumulateRenderStatisticsDiagnostics(recorder.statistics(), counters);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::DrawCalls), 3u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::RenderedItems), 4u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::SubmittedPrimitives), 8u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::RenderBatches), 3u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::MaterialSwitches), 1u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::ShaderSwitches), 0u);
        L2D_REQUIRE_EQUAL(counters.value(l2d::DiagnosticCounter::CulledItems), 4u);
    }
}

int main()
{
    int failures = 0;
    runTest("render statistics recorder ordering", testRecorderOrderingResetAndSaturation,
            failures);
    runTest("render statistics destroyed state identity", testDestroyedStateIdentity, failures);
    runTest("render statistics legacy overloads", testLegacyRenderMethodOverloads, failures);
    runTest("render statistics shader identity", testShaderSwitchIdentity, failures);
    runTest("render statistics sprite batch", testSpriteBatchStatistics, failures);
    runTest("render statistics SpriteRenderer", testSpriteRendererStatistics, failures);
    runTest("render statistics surface and post-process", testSurfaceAndPostProcessStatistics,
            failures);
    runTest("render statistics pipeline and composition", testPipelineAndCompositionPropagation,
            failures);
    runTest("render statistics tilemap diagnostics", testTilemapAndDiagnosticAdapters, failures);
    return failures == 0 ? 0 : 1;
}

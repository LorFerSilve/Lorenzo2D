#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>
#include <Lorenzo2D/Renderer/Shader2D.hpp>
#include <Lorenzo2D/Renderer/ShaderPostProcessChain2D.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>

#include "TestSupport.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    using l2d::test::runTest;

    constexpr const char* TransformFragmentSource = R"(
uniform sampler2D source_texture;
uniform float scale_value;
uniform float bias_value;

void main()
{
    vec4 sampled = texture2D(source_texture, gl_TexCoord[0].xy);
    sampled.r = clamp(sampled.r * scale_value + bias_value, 0.0, 1.0);
    gl_FragColor = gl_Color * sampled;
}
)";

    template <typename Function>
    void runShaderRuntimeTest(const char* name, Function&& function, int& failures)
    {
        if (!l2d::Shader2D::isSupported())
        {
            std::cout << "[SKIP] " << name << ": shader runtime is unavailable on this runner\n";
            return;
        }

        runTest(name, std::forward<Function>(function), failures);
    }

    l2d::Shader2DHandle makeShader()
    {
        auto shader = std::make_shared<l2d::Shader2D>();
        L2D_REQUIRE(shader->loadFragmentSource(TransformFragmentSource));
        L2D_REQUIRE(
            shader->setUniformLayout({{"source_texture", l2d::ShaderUniformType2D::Texture, true},
                                      {"scale_value", l2d::ShaderUniformType2D::Float, true},
                                      {"bias_value", l2d::ShaderUniformType2D::Float, true}}));
        return shader;
    }

    l2d::Material2DHandle makeMaterial(const l2d::Shader2DHandle& shader, float scale, float bias)
    {
        auto material = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(material->setShader(shader));
        L2D_REQUIRE(material->setCurrentTexture("source_texture"));
        L2D_REQUIRE(material->setFloat("scale_value", scale));
        L2D_REQUIRE(material->setFloat("bias_value", bias));
        L2D_REQUIRE(material->setBlendMode(l2d::MaterialBlendMode2D::None));
        L2D_REQUIRE(material->isComplete());
        return material;
    }

    void createPublishedSurface(l2d::RenderSurface2D& surface, sf::Vector2u size, sf::Color color,
                                bool smooth = false)
    {
        L2D_REQUIRE(surface.create({size, smooth, false}));
        L2D_REQUIRE(surface.clear(color));
        L2D_REQUIRE(surface.display());
    }

    sf::Color publishedPixel(l2d::RenderSurface2D& surface, sf::Vector2u pixel = {0u, 0u})
    {
        L2D_REQUIRE(surface.display());
        return surface.texture()->copyToImage().getPixel(pixel);
    }

    bool nearChannel(std::uint8_t value, int expected, int tolerance = 2)
    {
        return std::abs(static_cast<int>(value) - expected) <= tolerance;
    }

    void testConfigurationIsBoundedAndCpuOnly()
    {
        static_assert(!std::is_copy_constructible_v<l2d::ShaderPostProcessChain2D>);
        static_assert(!std::is_copy_assignable_v<l2d::ShaderPostProcessChain2D>);
        static_assert(!std::is_move_constructible_v<l2d::ShaderPostProcessChain2D>);
        static_assert(!std::is_move_assignable_v<l2d::ShaderPostProcessChain2D>);

        l2d::ShaderPostProcessPass2D pass;
        L2D_REQUIRE(!l2d::ShaderPostProcessChain2D::isValidPass(pass));

        pass.name = "cpu-only";
        pass.material = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(l2d::ShaderPostProcessChain2D::isValidPass(pass));

        pass.name.clear();
        L2D_REQUIRE(!l2d::ShaderPostProcessChain2D::isValidPass(pass));
        pass.name.assign(l2d::ShaderPostProcessChain2D::MaximumPassNameBytes + 1u, 'x');
        L2D_REQUIRE(!l2d::ShaderPostProcessChain2D::isValidPass(pass));

        l2d::ShaderPostProcessChain2D chain;
        for (std::size_t index = 0u; index < l2d::ShaderPostProcessChain2D::MaximumPassCount;
             ++index)
        {
            l2d::ShaderPostProcessPass2D configured;
            configured.name = "pass-" + std::to_string(index);
            configured.material = std::make_shared<l2d::Material2D>();
            L2D_REQUIRE(chain.addPass(std::move(configured)));
        }

        L2D_REQUIRE_EQUAL(chain.passCount(), l2d::ShaderPostProcessChain2D::MaximumPassCount);

        l2d::ShaderPostProcessPass2D overflow;
        overflow.name = "overflow";
        overflow.material = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(!chain.addPass(std::move(overflow)));

        L2D_REQUIRE(chain.movePass(chain.passCount() - 1u, 0u));
        L2D_REQUIRE(chain.setPassEnabled(0u, false));
        L2D_REQUIRE(chain.pass(0u) != nullptr);
        L2D_REQUIRE(!chain.pass(0u)->enabled);
        L2D_REQUIRE(chain.removePass(0u));
        L2D_REQUIRE_EQUAL(chain.passCount(), l2d::ShaderPostProcessChain2D::MaximumPassCount - 1u);

        chain.clear();
        L2D_REQUIRE_EQUAL(chain.passCount(), 0u);
        L2D_REQUIRE(!chain.workspaceReady());
        L2D_REQUIRE_EQUAL(chain.workspaceSize(), sf::Vector2u{});
    }

    void testPreflightRejectsUnpublishedAndUnavailableShaderWithoutDrawing()
    {
        l2d::RenderSurface2D source;
        l2d::RenderSurface2D destination;
        L2D_REQUIRE(source.create({{2u, 2u}, false, false}));
        createPublishedSurface(destination, {2u, 2u}, sf::Color::Green);

        auto material = std::make_shared<l2d::Material2D>();
        l2d::ShaderPostProcessChain2D chain;
        L2D_REQUIRE(chain.addPass({"unavailable-shader", material}));

        const l2d::ShaderPostProcessResult2D unpublished =
            chain.apply(source, *destination.target());
        L2D_REQUIRE(unpublished.failure == l2d::ShaderPostProcessFailure2D::SourceUnpublished);
        L2D_REQUIRE(!unpublished.failedPass.has_value());
        L2D_REQUIRE_EQUAL(unpublished.completedPasses, 0u);

        L2D_REQUIRE(source.clear(sf::Color::Red));
        L2D_REQUIRE(source.display());

        const l2d::ShaderPostProcessResult2D unavailable =
            chain.apply(source, *destination.target());
        L2D_REQUIRE(unavailable.failure == l2d::ShaderPostProcessFailure2D::ShaderUnavailable);
        L2D_REQUIRE(unavailable.failedPass == std::optional<std::size_t>(0u));
        L2D_REQUIRE_EQUAL(unavailable.completedPasses, 0u);

        const sf::Color destinationColor = publishedPixel(destination);
        L2D_REQUIRE_EQUAL(destinationColor, sf::Color::Green);
    }

    void testDisabledChainCopiesSourceAndRestoresDestinationView()
    {
        l2d::RenderSurface2D source;
        l2d::RenderSurface2D destination;
        createPublishedSurface(source, {2u, 2u}, sf::Color(23, 91, 177, 129));
        createPublishedSurface(destination, {4u, 4u}, sf::Color::Black);

        sf::View custom = destination.target()->getView();
        custom.setCenter({17.f, 29.f});
        destination.target()->setView(custom);

        l2d::ShaderPostProcessChain2D chain;
        const l2d::ShaderPostProcessResult2D result = chain.apply(source, *destination.target());

        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedPasses, 0u);
        L2D_REQUIRE_EQUAL(destination.target()->getView().getCenter(), custom.getCenter());

        L2D_REQUIRE(destination.display());
        const sf::Image image = destination.texture()->copyToImage();
        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color(23, 91, 177, 129));
        L2D_REQUIRE_EQUAL(image.getPixel({3u, 3u}), sf::Color(23, 91, 177, 129));
    }

    void testSinglePassAppliesShaderFullscreen()
    {
        const l2d::Shader2DHandle shader = makeShader();
        const l2d::Material2DHandle material = makeMaterial(shader, 0.5f, 0.25f);

        l2d::RenderSurface2D source;
        l2d::RenderSurface2D destination;
        createPublishedSurface(source, {2u, 2u}, sf::Color(64, 100, 150, 255));
        createPublishedSurface(destination, {4u, 4u}, sf::Color::Magenta);

        l2d::ShaderPostProcessChain2D chain;
        L2D_REQUIRE(chain.addPass({"transform", material, sf::Color::Black, true, true}));

        const l2d::ShaderPostProcessResult2D result = chain.apply(source, *destination.target());
        L2D_REQUIRE(result.succeeded());
        L2D_REQUIRE_EQUAL(result.completedPasses, 1u);
        L2D_REQUIRE(!chain.workspaceReady());

        const sf::Color color = publishedPixel(destination, {3u, 3u});
        L2D_REQUIRE(nearChannel(color.r, 96));
        L2D_REQUIRE(nearChannel(color.g, 100));
        L2D_REQUIRE(nearChannel(color.b, 150));
        L2D_REQUIRE(nearChannel(color.a, 255));
    }

    void testMultiPassOrderUsesReusablePingPongWorkspace()
    {
        const l2d::Shader2DHandle shader = makeShader();
        const l2d::Material2DHandle half = makeMaterial(shader, 0.5f, 0.f);
        const l2d::Material2DHandle addQuarter = makeMaterial(shader, 1.f, 0.25f);
        const l2d::Material2DHandle identity = makeMaterial(shader, 1.f, 0.f);

        l2d::RenderSurface2D source;
        l2d::RenderSurface2D destination;
        createPublishedSurface(source, {3u, 2u}, sf::Color(64, 80, 120, 255), true);
        createPublishedSurface(destination, {3u, 2u}, sf::Color::Black);

        l2d::ShaderPostProcessChain2D chain;
        L2D_REQUIRE(chain.addPass({"half", half}));
        L2D_REQUIRE(chain.addPass({"add-quarter", addQuarter}));
        L2D_REQUIRE(chain.addPass({"identity", identity}));

        const l2d::ShaderPostProcessResult2D first = chain.apply(source, *destination.target());
        L2D_REQUIRE(first.succeeded());
        L2D_REQUIRE_EQUAL(first.completedPasses, 3u);
        L2D_REQUIRE(chain.workspaceReady());
        L2D_REQUIRE_EQUAL(chain.workspaceSize(), sf::Vector2u(3u, 2u));
        L2D_REQUIRE(nearChannel(publishedPixel(destination).r, 96));

        L2D_REQUIRE(chain.movePass(1u, 0u));
        const l2d::ShaderPostProcessResult2D reordered = chain.apply(source, *destination.target());
        L2D_REQUIRE(reordered.succeeded());
        L2D_REQUIRE(nearChannel(publishedPixel(destination).r, 64));

        L2D_REQUIRE(source.create({{4u, 1u}, false, false}));
        L2D_REQUIRE(source.clear(sf::Color(64, 80, 120, 255)));
        L2D_REQUIRE(source.display());
        L2D_REQUIRE(chain.apply(source, *destination.target()).succeeded());
        L2D_REQUIRE_EQUAL(chain.workspaceSize(), sf::Vector2u(4u, 1u));

        chain.resetWorkspace();
        L2D_REQUIRE(!chain.workspaceReady());
        L2D_REQUIRE_EQUAL(chain.workspaceSize(), sf::Vector2u{});
    }

    void testFeedbackAndFailureNamesAreStable()
    {
        l2d::RenderSurface2D source;
        createPublishedSurface(source, {2u, 2u}, sf::Color::Blue);

        l2d::ShaderPostProcessChain2D chain;
        const l2d::ShaderPostProcessResult2D feedback = chain.apply(source, *source.target());
        L2D_REQUIRE(feedback.failure == l2d::ShaderPostProcessFailure2D::FeedbackLoop);

        L2D_REQUIRE_EQUAL(l2d::shaderPostProcessFailureName(l2d::ShaderPostProcessFailure2D::None),
                          std::string_view("none"));
        L2D_REQUIRE_EQUAL(l2d::shaderPostProcessFailureName(
                              l2d::ShaderPostProcessFailure2D::WorkspacePublishFailed),
                          std::string_view("workspace-publish-failed"));
        L2D_REQUIRE_EQUAL(
            l2d::shaderPostProcessFailureName(static_cast<l2d::ShaderPostProcessFailure2D>(255)),
            std::string_view("unknown"));
    }
}

int main()
{
    int failures = 0;

    runTest("shader post-process configuration is bounded and CPU-only",
            testConfigurationIsBoundedAndCpuOnly, failures);
    runTest("post-process preflight rejects unpublished source and unavailable shader",
            testPreflightRejectsUnpublishedAndUnavailableShaderWithoutDrawing, failures);
    runTest("disabled post-process chain copies source and restores destination view",
            testDisabledChainCopiesSourceAndRestoresDestinationView, failures);
    runShaderRuntimeTest("single shader post-process pass applies fullscreen",
                         testSinglePassAppliesShaderFullscreen, failures);
    runShaderRuntimeTest("multi-pass post-process order uses reusable ping-pong workspace",
                         testMultiPassOrderUsesReusablePingPongWorkspace, failures);
    runTest("post-process feedback and failure names are stable",
            testFeedbackAndFailureNamesAreStable, failures);

    return failures == 0 ? 0 : 1;
}

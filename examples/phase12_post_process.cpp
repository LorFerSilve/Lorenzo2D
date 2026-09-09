#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/RenderPipeline2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>
#include <Lorenzo2D/Renderer/Shader2D.hpp>
#include <Lorenzo2D/Renderer/ShaderPostProcessChain2D.hpp>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <memory>
#include <stdexcept>

namespace
{
    constexpr const char* FragmentSource = R"(
uniform sampler2D source_texture;
uniform float desaturate;
uniform vec4 tint;

void main()
{
    vec4 source = texture2D(source_texture, gl_TexCoord[0].xy);
    float luminance = dot(source.rgb, vec3(0.299, 0.587, 0.114));
    vec3 graded = mix(source.rgb, vec3(luminance), desaturate) * tint.rgb;
    gl_FragColor = vec4(graded, source.a * tint.a) * gl_Color;
}
)";
}

class Phase12PostProcessExample final : public l2d::Application
{
  public:
    Phase12PostProcessExample()
        : l2d::Application(800, 450, "Lorenzo2D Phase 12 shader post-processing")
    {
        if (!l2d::Shader2D::isSupported())
            throw std::runtime_error("This example requires shader support");

        if (!m_sceneSurface->create({{800u, 450u}, true, false}))
            throw std::runtime_error("Could not allocate the scene render surface");

        auto shader = std::make_shared<l2d::Shader2D>();
        if (!shader->loadFragmentSource(FragmentSource) ||
            !shader->setUniformLayout(
                {{"source_texture", l2d::ShaderUniformType2D::Texture, true},
                 {"desaturate", l2d::ShaderUniformType2D::Float, true},
                 {"tint", l2d::ShaderUniformType2D::Color, true}}))
        {
            throw std::runtime_error("Could not create the post-process shader");
        }

        auto material = std::make_shared<l2d::Material2D>();
        if (!material->setShader(shader) || !material->setCurrentTexture("source_texture") ||
            !material->setFloat("desaturate", 0.35f) ||
            !material->setColor("tint", sf::Color(220, 235, 255)) ||
            !material->setBlendMode(l2d::MaterialBlendMode2D::None))
        {
            throw std::runtime_error("Could not configure the post-process material");
        }

        l2d::ShaderPostProcessPass2D postPass;
        postPass.name = "cool-desaturate";
        postPass.material = std::move(material);
        postPass.clearColor = sf::Color::Black;
        if (!m_postProcess.addPass(std::move(postPass)))
            throw std::runtime_error("Could not configure the post-process chain");

        l2d::RenderPipelinePass2D scenePass;
        scenePass.name = "scene-offscreen";
        scenePass.target = l2d::RenderPipelinePassTarget2D::Surface;
        scenePass.surface = m_sceneSurface;
        scenePass.clear = {true, sf::Color(18, 24, 42)};
        if (!m_pipeline.addPass(std::move(scenePass),
                                [](const l2d::RenderPipelineExecution2D& execution)
                                {
                                    sf::RectangleShape panel({520.f, 250.f});
                                    panel.setPosition({140.f, 100.f});
                                    panel.setFillColor(sf::Color(245, 175, 70));
                                    execution.target.draw(panel);

                                    sf::CircleShape circle(70.f);
                                    circle.setPosition({330.f, 155.f});
                                    circle.setFillColor(sf::Color(70, 155, 255));
                                    execution.target.draw(circle);
                                    return true;
                                }))
        {
            throw std::runtime_error("Could not configure the off-screen render pass");
        }

        l2d::RenderPipelinePass2D postProcessPass;
        postProcessPass.name = "shader-post-process";
        postProcessPass.inputs.push_back(m_sceneSurface);
        postProcessPass.clear = {true, sf::Color::Black};
        if (!m_pipeline.addPass(
                std::move(postProcessPass),
                [this](const l2d::RenderPipelineExecution2D& execution)
                {
                    if (execution.inputs.size() != 1u) return false;
                    return m_postProcess.apply(*execution.inputs.front(), execution.target).succeeded();
                }))
        {
            throw std::runtime_error("Could not configure the post-process render pass");
        }
    }

  private:
    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        const l2d::RenderPipelineResult2D result =
            m_pipeline.execute(window, {interpolationAlpha, nullptr});
        if (!result.succeeded()) window.clear(sf::Color::Red);
    }

    std::shared_ptr<l2d::RenderSurface2D> m_sceneSurface =
        std::make_shared<l2d::RenderSurface2D>();
    l2d::ShaderPostProcessChain2D m_postProcess;
    l2d::RenderPipeline2D m_pipeline;
};

int main()
{
    Phase12PostProcessExample application;
    application.run();
    return 0;
}

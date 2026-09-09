#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/Shader2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "TestSupport.hpp"

#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using l2d::test::runTest;

    constexpr const char* FragmentSource = R"(
uniform float alpha_value;
uniform int mode_value;
uniform bool enabled_value;
uniform vec2 offset_value;
uniform vec3 direction_value;
uniform vec4 tint_value;
uniform sampler2D texture_value;

void main()
{
    vec4 sampled = texture2D(texture_value, gl_TexCoord[0].xy);
    float enabled = enabled_value ? 1.0 : 0.0;
    float mode = float(mode_value) * 0.0;
    vec3 direction = direction_value * 0.0;
    vec2 offset = offset_value * 0.0;
    gl_FragColor = gl_Color * sampled * tint_value *
                   vec4(direction + vec3(offset, mode), alpha_value * enabled);
}
)";

    constexpr const char* VertexSource = R"(
void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
}
)";

    std::vector<l2d::ShaderUniformSpec2D> completeLayout()
    {
        return {
            {"alpha_value", l2d::ShaderUniformType2D::Float, true},
            {"mode_value", l2d::ShaderUniformType2D::Integer, false},
            {"enabled_value", l2d::ShaderUniformType2D::Boolean, false},
            {"offset_value", l2d::ShaderUniformType2D::Vector2, false},
            {"direction_value", l2d::ShaderUniformType2D::Vector3, false},
            {"tint_value", l2d::ShaderUniformType2D::Color, true},
            {"texture_value", l2d::ShaderUniformType2D::Texture, true},
        };
    }

    void testShaderSourceLoadingIsBoundedAndTransactional()
    {
        L2D_REQUIRE(l2d::Shader2D::isSupported());

        l2d::Shader2D shader;
        L2D_REQUIRE(!shader.loaded());
        L2D_REQUIRE(!shader.loadFragmentSource(""));
        L2D_REQUIRE(
            !shader.loadFragmentSource(std::string(l2d::Shader2D::MaximumSourceBytes + 1u, 'x')));

        L2D_REQUIRE(shader.loadFragmentSource(FragmentSource));
        L2D_REQUIRE(shader.loaded());
        const unsigned int firstHandle = shader.nativeHandle();
        L2D_REQUIRE(firstHandle != 0u);

        L2D_REQUIRE(!shader.loadFragmentSource("this is not valid GLSL"));
        L2D_REQUIRE(shader.loaded());
        L2D_REQUIRE_EQUAL(shader.nativeHandle(), firstHandle);

        L2D_REQUIRE(shader.loadVertexFragmentSource(VertexSource, FragmentSource));
        L2D_REQUIRE(shader.nativeHandle() != 0u);
    }

    void testShaderUniformLayoutValidationIsTransactional()
    {
        l2d::Shader2D shader;
        L2D_REQUIRE(shader.setUniformLayout(completeLayout()));
        L2D_REQUIRE_EQUAL(shader.uniformCount(), 7u);
        L2D_REQUIRE(shader.uniform("alpha_value") != nullptr);
        L2D_REQUIRE(shader.uniform("alpha_value")->required);
        L2D_REQUIRE(shader.uniform("missing") == nullptr);

        auto duplicate = completeLayout();
        duplicate.push_back({"alpha_value", l2d::ShaderUniformType2D::Float, false});
        L2D_REQUIRE(!shader.setUniformLayout(std::move(duplicate)));
        L2D_REQUIRE_EQUAL(shader.uniformCount(), 7u);

        auto invalidName = completeLayout();
        invalidName[0].name = "bad uniform";
        L2D_REQUIRE(!shader.setUniformLayout(std::move(invalidName)));
        L2D_REQUIRE_EQUAL(shader.uniformCount(), 7u);

        auto invalidType = completeLayout();
        invalidType[0].type = static_cast<l2d::ShaderUniformType2D>(255);
        L2D_REQUIRE(!shader.setUniformLayout(std::move(invalidType)));
        L2D_REQUIRE_EQUAL(shader.uniformCount(), 7u);

        std::vector<l2d::ShaderUniformSpec2D> tooMany;
        tooMany.reserve(l2d::Shader2D::MaximumUniformCount + 1u);
        for (std::size_t index = 0u; index <= l2d::Shader2D::MaximumUniformCount; ++index)
        {
            tooMany.push_back(
                {"uniform_" + std::to_string(index), l2d::ShaderUniformType2D::Float, false});
        }

        L2D_REQUIRE(!shader.setUniformLayout(std::move(tooMany)));
        L2D_REQUIRE_EQUAL(shader.uniformCount(), 7u);
    }

    void testMaterialTypedUniformsAndCompleteness()
    {
        auto shader = std::make_shared<l2d::Shader2D>();
        L2D_REQUIRE(shader->loadFragmentSource(FragmentSource));
        L2D_REQUIRE(shader->setUniformLayout(completeLayout()));

        l2d::Material2D material;
        L2D_REQUIRE(material.setShader(shader));
        L2D_REQUIRE(!material.isComplete());

        L2D_REQUIRE(material.setFloat("alpha_value", 0.75f));
        L2D_REQUIRE(!material.setFloat("alpha_value", std::numeric_limits<float>::quiet_NaN()));
        L2D_REQUIRE(!material.setInteger("alpha_value", 4));
        L2D_REQUIRE(material.setInteger("mode_value", 2));
        L2D_REQUIRE(material.setBoolean("enabled_value", true));
        L2D_REQUIRE(material.setVector2("offset_value", {2.f, 3.f}));
        L2D_REQUIRE(
            !material.setVector2("offset_value", {std::numeric_limits<float>::infinity(), 0.f}));
        L2D_REQUIRE(material.setVector3("direction_value", {1.f, 2.f, 3.f}));
        L2D_REQUIRE(material.setColor("tint_value", sf::Color(200, 160, 120, 255)));
        L2D_REQUIRE(material.setCurrentTexture("texture_value"));
        L2D_REQUIRE(material.isComplete());
        L2D_REQUIRE_EQUAL(material.uniformCount(), 7u);
        L2D_REQUIRE(material.uniformType("texture_value") ==
                    std::optional<l2d::ShaderUniformType2D>(l2d::ShaderUniformType2D::Texture));

        const l2d::TextureHandle invalidTexture;
        L2D_REQUIRE(!material.setTexture("texture_value", invalidTexture));
        L2D_REQUIRE(material.isComplete());

        L2D_REQUIRE(material.setBlendMode(l2d::MaterialBlendMode2D::Add));
        L2D_REQUIRE(!material.setBlendMode(static_cast<l2d::MaterialBlendMode2D>(255)));
        L2D_REQUIRE(material.blendMode() == l2d::MaterialBlendMode2D::Add);

        sf::RenderStates states;
        L2D_REQUIRE(material.apply(states));
        L2D_REQUIRE(states.shader != nullptr);
        L2D_REQUIRE(states.blendMode == sf::BlendAdd);

        L2D_REQUIRE(material.removeUniform("alpha_value"));
        L2D_REQUIRE(!material.isComplete());
        L2D_REQUIRE(!material.apply(states));
        L2D_REQUIRE(material.setFloat("alpha_value", 1.f));
        L2D_REQUIRE(material.isComplete());
    }

    void testShaderRebindingRejectsIncompatibleMaterialState()
    {
        auto firstShader = std::make_shared<l2d::Shader2D>();
        L2D_REQUIRE(firstShader->loadFragmentSource(FragmentSource));
        L2D_REQUIRE(firstShader->setUniformLayout(completeLayout()));

        l2d::Material2D material;
        L2D_REQUIRE(material.setShader(firstShader));
        L2D_REQUIRE(material.setFloat("alpha_value", 0.5f));

        auto incompatibleShader = std::make_shared<l2d::Shader2D>();
        L2D_REQUIRE(incompatibleShader->loadFragmentSource(FragmentSource));
        L2D_REQUIRE(incompatibleShader->setUniformLayout(
            {{"alpha_value", l2d::ShaderUniformType2D::Integer, false}}));

        L2D_REQUIRE(!material.setShader(incompatibleShader));
        L2D_REQUIRE(material.shader() == firstShader);
        L2D_REQUIRE(material.uniformType("alpha_value") ==
                    std::optional<l2d::ShaderUniformType2D>(l2d::ShaderUniformType2D::Float));

        material.clearShader();
        L2D_REQUIRE(material.shader() == nullptr);
        L2D_REQUIRE_EQUAL(material.uniformCount(), 0u);
        L2D_REQUIRE(material.isComplete());

        sf::RenderStates states;
        L2D_REQUIRE(material.setBlendMode(l2d::MaterialBlendMode2D::Multiply));
        L2D_REQUIRE(material.apply(states));
        L2D_REQUIRE(states.shader == nullptr);
        L2D_REQUIRE(states.blendMode == sf::BlendMultiply);
    }

    void testSpriteRendererMaterialBindingIsOptional()
    {
        const l2d::TextureHandle texture(std::make_shared<sf::Texture>());
        l2d::SpriteRenderer sprite(texture);
        L2D_REQUIRE(sprite.material() == nullptr);

        auto material = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(material->setBlendMode(l2d::MaterialBlendMode2D::Add));

        sprite.setMaterial(material);
        L2D_REQUIRE(sprite.material() == material);

        sprite.clearMaterial();
        L2D_REQUIRE(sprite.material() == nullptr);
    }
}

int main()
{
    int failures = 0;

    runTest("shader loading is bounded and transactional",
            testShaderSourceLoadingIsBoundedAndTransactional, failures);
    runTest("shader uniform layout validation is transactional",
            testShaderUniformLayoutValidationIsTransactional, failures);
    runTest("material typed uniforms and completeness", testMaterialTypedUniformsAndCompleteness,
            failures);
    runTest("shader rebinding rejects incompatible material state",
            testShaderRebindingRejectsIncompatibleMaterialState, failures);
    runTest("sprite material binding is optional", testSpriteRendererMaterialBindingIsOptional,
            failures);

    return failures == 0 ? 0 : 1;
}

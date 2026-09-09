#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Renderer/Shader2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace sf
{
    struct RenderStates;
}

namespace l2d
{
    enum class MaterialBlendMode2D : std::uint8_t
    {
        Alpha,
        Add,
        Multiply,
        None
    };

    struct MaterialCurrentTexture2D
    {
    };

    using MaterialUniformValue2D = std::variant<float, int, bool, sf::Vector2f, sf::Vector3f,
                                                sf::Color, TextureHandle, MaterialCurrentTexture2D>;

    // Explicit per-draw shader state. Material2D owns sampler texture leases and
    // validates every typed setter against the Shader2D uniform layout.
    //
    // Material mutation/application is a single render-thread contract. Sharing
    // one Shader2D between materials is supported because each apply() rewrites
    // the complete material-owned uniform state before the draw.
    class Material2D
    {
      public:
        [[nodiscard]] bool setShader(Shader2DHandle shader);
        void clearShader();

        [[nodiscard]] Shader2DHandle shader() const noexcept;

        [[nodiscard]] bool setBlendMode(MaterialBlendMode2D blendMode) noexcept;
        [[nodiscard]] MaterialBlendMode2D blendMode() const noexcept;

        [[nodiscard]] bool setFloat(std::string name, float value);
        [[nodiscard]] bool setInteger(std::string name, int value);
        [[nodiscard]] bool setBoolean(std::string name, bool value);
        [[nodiscard]] bool setVector2(std::string name, sf::Vector2f value);
        [[nodiscard]] bool setVector3(std::string name, sf::Vector3f value);
        [[nodiscard]] bool setColor(std::string name, sf::Color value);
        [[nodiscard]] bool setTexture(std::string name, TextureHandle texture);
        [[nodiscard]] bool setCurrentTexture(std::string name);

        [[nodiscard]] bool removeUniform(std::string_view name);
        void clearUniforms();

        [[nodiscard]] std::size_t uniformCount() const noexcept;
        [[nodiscard]] const MaterialUniformValue2D* uniform(std::string_view name) const noexcept;
        [[nodiscard]] std::optional<ShaderUniformType2D> uniformType(
            std::string_view name) const noexcept;

        // Complete means every required shader declaration has a compatible
        // material value and every stored value still matches the current layout.
        // Omitted optional declarations receive canonical defaults at apply().
        [[nodiscard]] bool isComplete() const noexcept;

        // Transactionally updates RenderStates only after validation succeeds.
        // Shader uniform state itself is then written immediately for the
        // upcoming draw.
        [[nodiscard]] bool apply(sf::RenderStates& states) const;

      private:
        [[nodiscard]] bool setUniform(std::string name, ShaderUniformType2D type,
                                      MaterialUniformValue2D value);
        [[nodiscard]] bool uniformMatchesLayout(std::string_view name,
                                                ShaderUniformType2D type) const noexcept;
        [[nodiscard]] static std::optional<ShaderUniformType2D> typeOf(
            const MaterialUniformValue2D& value) noexcept;
        [[nodiscard]] static bool isFinite(sf::Vector2f value) noexcept;
        [[nodiscard]] static bool isFinite(sf::Vector3f value) noexcept;

        Shader2DHandle m_shader;
        MaterialBlendMode2D m_blendMode = MaterialBlendMode2D::Alpha;
        std::map<std::string, MaterialUniformValue2D, std::less<>> m_uniforms;
    };

    using Material2DHandle = std::shared_ptr<Material2D>;
}

#pragma once

#include <SFML/Graphics/Shader.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace l2d
{
    enum class ShaderUniformType2D : std::uint8_t
    {
        Float,
        Integer,
        Boolean,
        Vector2,
        Vector3,
        Color,
        Texture
    };

    struct ShaderUniformSpec2D
    {
        std::string name;
        ShaderUniformType2D type = ShaderUniformType2D::Float;
        bool required = false;
    };

    class Material2D;

    // Owns one compiled SFML shader program plus an explicit material-facing
    // uniform layout. Compilation is transactional: a failed reload keeps the
    // previously compiled program alive.
    //
    // Shader compilation and material application are graphics-context work and
    // must occur on the owning render thread/context unless the application
    // provides an equivalent explicit context discipline.
    class Shader2D
    {
      public:
        static constexpr std::size_t MaximumSourceBytes = 1024u * 1024u;
        static constexpr std::size_t MaximumUniformCount = 64u;
        static constexpr std::size_t MaximumUniformNameBytes = 128u;

        Shader2D() = default;

        Shader2D(const Shader2D&) = delete;
        Shader2D& operator=(const Shader2D&) = delete;
        Shader2D(Shader2D&&) noexcept = default;
        Shader2D& operator=(Shader2D&&) noexcept = default;

        [[nodiscard]] static bool isSupported() noexcept;

        [[nodiscard]] bool loadFragmentSource(std::string_view fragmentSource);
        [[nodiscard]] bool loadVertexFragmentSource(std::string_view vertexSource,
                                                    std::string_view fragmentSource);

        // Replaces the material-facing layout only when every declaration is
        // valid, unique, and within the bounded public envelope.
        [[nodiscard]] bool setUniformLayout(std::vector<ShaderUniformSpec2D> layout);
        void clearUniformLayout();

        [[nodiscard]] bool loaded() const noexcept;
        [[nodiscard]] unsigned int nativeHandle() const noexcept;
        [[nodiscard]] std::size_t uniformCount() const noexcept;
        [[nodiscard]] const ShaderUniformSpec2D* uniform(std::string_view name) const noexcept;
        [[nodiscard]] const std::vector<ShaderUniformSpec2D>& uniformLayout() const noexcept;

        [[nodiscard]] static bool isValidUniformName(std::string_view name) noexcept;
        [[nodiscard]] static bool isValidUniformType(ShaderUniformType2D type) noexcept;

      private:
        [[nodiscard]] static bool isValidSource(std::string_view source) noexcept;

        sf::Shader m_shader;
        std::vector<ShaderUniformSpec2D> m_uniformLayout;
        bool m_loaded = false;

        friend class Material2D;
    };

    using Shader2DHandle = std::shared_ptr<Shader2D>;
}

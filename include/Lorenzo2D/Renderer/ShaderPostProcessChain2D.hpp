#pragma once

#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sf
{
    class RenderTarget;
    class Texture;
}

namespace l2d
{
    enum class ShaderPostProcessFailure2D : std::uint8_t
    {
        None,
        InvalidPass,
        SourceUnavailable,
        SourceUnpublished,
        DestinationUnavailable,
        FeedbackLoop,
        ShaderUnavailable,
        MaterialIncomplete,
        WorkspaceAllocationFailed,
        WorkspaceClearFailed,
        MaterialApplyFailed,
        WorkspacePublishFailed
    };

    [[nodiscard]] std::string_view shaderPostProcessFailureName(
        ShaderPostProcessFailure2D failure) noexcept;

    struct ShaderPostProcessPass2D
    {
        std::string name;
        Material2DHandle material;
        sf::Color clearColor = sf::Color::Transparent;
        bool clearOutput = true;
        bool enabled = true;
    };

    struct ShaderPostProcessResult2D
    {
        ShaderPostProcessFailure2D failure = ShaderPostProcessFailure2D::None;
        std::size_t completedPasses = 0u;
        std::optional<std::size_t> failedPass;

        [[nodiscard]] bool succeeded() const noexcept;
    };

    // Ordered full-screen shader chain for a published RenderSurface2D input.
    //
    // Passes execute strictly in insertion order. Multi-pass chains allocate up
    // to two internal ping-pong RenderSurface2D workspaces lazily and reuse them
    // while the source size/filtering configuration remains compatible.
    // apply() may therefore allocate graphics resources on first use or resize;
    // resetWorkspace() releases those retained resources explicitly.
    //
    // The source texture is bound as the current draw texture. A pass material
    // can expose that input through Material2D::setCurrentTexture(), while other
    // texture uniforms remain available for LUT/noise/auxiliary inputs.
    //
    // Configuration is CPU-only. Shader compilation, workspace allocation, and
    // apply() are graphics-context work and follow the owning render-thread
    // discipline. This type is not a concurrent API.
    class ShaderPostProcessChain2D
    {
      public:
        static constexpr std::size_t MaximumPassCount = 16u;
        static constexpr std::size_t MaximumPassNameBytes = 128u;

        ShaderPostProcessChain2D() = default;
        ~ShaderPostProcessChain2D() = default;

        ShaderPostProcessChain2D(const ShaderPostProcessChain2D&) = delete;
        ShaderPostProcessChain2D& operator=(const ShaderPostProcessChain2D&) = delete;
        ShaderPostProcessChain2D(ShaderPostProcessChain2D&&) = delete;
        ShaderPostProcessChain2D& operator=(ShaderPostProcessChain2D&&) = delete;

        [[nodiscard]] static bool isValidPass(const ShaderPostProcessPass2D& pass) noexcept;

        [[nodiscard]] bool addPass(ShaderPostProcessPass2D pass);
        [[nodiscard]] bool setPass(std::size_t index, ShaderPostProcessPass2D pass);
        [[nodiscard]] bool setPassEnabled(std::size_t index, bool enabled) noexcept;
        [[nodiscard]] bool movePass(std::size_t from, std::size_t to);
        [[nodiscard]] bool removePass(std::size_t index);
        void clear();

        [[nodiscard]] std::size_t passCount() const noexcept;
        [[nodiscard]] const ShaderPostProcessPass2D* pass(std::size_t index) const noexcept;

        [[nodiscard]] bool workspaceReady() const noexcept;
        [[nodiscard]] sf::Vector2u workspaceSize() const noexcept;
        void resetWorkspace();

        // Applies all enabled passes to destination in deterministic insertion
        // order. The source must already have published content via display().
        // The destination view is restored after the call. If no passes are
        // enabled, source is copied to the destination with overwrite blending.
        //
        // destination may itself be the output target of a RenderPipeline2D
        // surface pass; in that case the pipeline remains responsible for the
        // final RenderSurface2D::display() publication step.
        [[nodiscard]] ShaderPostProcessResult2D apply(const RenderSurface2D& source,
                                                      sf::RenderTarget& destination);

      private:
        [[nodiscard]] bool isPassNameAvailable(std::string_view name,
                                               std::optional<std::size_t> skip) const noexcept;
        [[nodiscard]] bool ensureWorkspace(const RenderSurfaceConfig2D& config,
                                           std::size_t requiredCount);
        [[nodiscard]] static bool workspaceMatches(const RenderSurface2D& surface,
                                                   const RenderSurfaceConfig2D& config) noexcept;
        [[nodiscard]] static bool drawFullscreen(const sf::Texture& texture,
                                                 sf::RenderTarget& destination,
                                                 const Material2DHandle& material);

        std::vector<ShaderPostProcessPass2D> m_passes;
        std::array<std::unique_ptr<RenderSurface2D>, 2u> m_workspace;
    };
}

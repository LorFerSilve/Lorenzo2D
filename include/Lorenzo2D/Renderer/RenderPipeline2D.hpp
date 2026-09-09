#pragma once

#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>

#include <SFML/Graphics/Color.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sf
{
    class RenderTarget;
    class RenderWindow;
}

namespace l2d
{
    class Scene;

    enum class RenderPipelinePassTarget2D : std::uint8_t
    {
        Backbuffer,
        Surface
    };

    enum class RenderPipelineFailure2D : std::uint8_t
    {
        None,
        ReentrantExecution,
        InvalidFrame,
        InvalidPass,
        LegacySceneRequired,
        SurfaceUnavailable,
        InputUnavailable,
        FeedbackLoop,
        CallbackFailed,
        SurfaceClearFailed,
        SurfacePublishFailed,
        SurfacePresentFailed
    };

    [[nodiscard]] std::string_view renderPipelineFailureName(
        RenderPipelineFailure2D failure) noexcept;

    struct RenderPipelineClear2D
    {
        bool enabled = false;
        sf::Color color = sf::Color::Black;
    };

    struct RenderPipelinePresent2D
    {
        bool enabled = false;
        RenderSurfacePresent2D presentation;
    };

    // One deterministic ordered pipeline pass.
    //
    // inputs are read-only lifetime leases. target selects either the caller
    // backbuffer or one owned RenderSurface2D output. Surface outputs are
    // published automatically after the pass callback completes.
    struct RenderPipelinePass2D
    {
        std::string name;
        RenderPass2D contextPass = RenderPass2D::World;
        RenderPipelinePassTarget2D target = RenderPipelinePassTarget2D::Backbuffer;
        RenderSurface2DHandle surface;
        std::vector<RenderSurface2DConstHandle> inputs;
        RenderPipelineClear2D clear;
        RenderPipelinePresent2D present;
        bool enabled = true;
    };

    // Per-execution presentation state. projection is borrowed only for the
    // duration of execute(); the pipeline never stores it.
    struct RenderPipelineFrame2D
    {
        float interpolationAlpha = 1.f;
        const CoordinateProjection2D* projection = nullptr;
    };

    struct RenderPipelineExecution2D
    {
        sf::RenderTarget& target;
        const RenderContext2D& context;
        const std::vector<RenderSurface2DConstHandle>& inputs;
    };

    using RenderPipelineCallback2D =
        std::function<bool(const RenderPipelineExecution2D& execution)>;

    struct RenderPipelineResult2D
    {
        RenderPipelineFailure2D failure = RenderPipelineFailure2D::None;
        std::size_t completedPasses = 0u;
        std::optional<std::size_t> failedPass;

        [[nodiscard]] bool succeeded() const noexcept;
    };

    // Lightweight ordered pass orchestration. This is deliberately not a
    // dependency-scheduling render graph: insertion order is execution order,
    // declared inputs are validated but never reordered automatically.
    class RenderPipeline2D
    {
      public:
        static constexpr std::size_t MaximumPassCount = 64u;
        static constexpr std::size_t MaximumInputCount = 16u;
        static constexpr std::size_t MaximumPassNameBytes = 128u;

        RenderPipeline2D() = default;
        ~RenderPipeline2D() = default;

        RenderPipeline2D(const RenderPipeline2D&) = delete;
        RenderPipeline2D& operator=(const RenderPipeline2D&) = delete;
        RenderPipeline2D(RenderPipeline2D&&) = delete;
        RenderPipeline2D& operator=(RenderPipeline2D&&) = delete;

        [[nodiscard]] static bool isValidFrame(const RenderPipelineFrame2D& frame) noexcept;
        [[nodiscard]] static bool isValidPass(const RenderPipelinePass2D& pass) noexcept;

        [[nodiscard]] bool addPass(RenderPipelinePass2D pass, RenderPipelineCallback2D callback);
        [[nodiscard]] bool addLegacyScenePass(RenderPipelinePass2D pass);
        [[nodiscard]] bool setPass(std::size_t index, RenderPipelinePass2D pass);
        [[nodiscard]] bool setPassEnabled(std::size_t index, bool enabled) noexcept;
        [[nodiscard]] bool movePass(std::size_t from, std::size_t to);
        [[nodiscard]] bool removePass(std::size_t index);
        [[nodiscard]] bool clear();

        [[nodiscard]] std::size_t passCount() const noexcept;
        [[nodiscard]] const RenderPipelinePass2D* pass(std::size_t index) const noexcept;
        [[nodiscard]] bool isLegacyScenePass(std::size_t index) const noexcept;
        [[nodiscard]] bool executing() const noexcept;

        // Generic execution supports callback passes against any SFML
        // backbuffer target. Pipelines containing a legacy Scene pass fail
        // preflight with LegacySceneRequired before drawing anything.
        [[nodiscard]] RenderPipelineResult2D execute(sf::RenderTarget& backbuffer,
                                                     const RenderPipelineFrame2D& frame = {});

        // Compatibility bridge for the existing Scene/Component
        // RenderWindow virtual contract. The Scene is borrowed only for this
        // call and is never retained by the pipeline.
        [[nodiscard]] RenderPipelineResult2D execute(sf::RenderWindow& window, Scene& legacyScene,
                                                     const RenderPipelineFrame2D& frame = {});

      private:
        struct PassRecord
        {
            RenderPipelinePass2D pass;
            RenderPipelineCallback2D callback;
            bool legacyScene = false;
        };

        [[nodiscard]] bool isPassNameAvailable(std::string_view name,
                                               std::optional<std::size_t> skip) const noexcept;
        [[nodiscard]] static bool isValidLegacyScenePass(const RenderPipelinePass2D& pass) noexcept;
        [[nodiscard]] RenderPipelineResult2D preflight(sf::RenderTarget& backbuffer,
                                                       bool legacySceneAvailable,
                                                       const RenderPipelineFrame2D& frame) const;
        [[nodiscard]] RenderPipelineResult2D executeImpl(sf::RenderTarget& backbuffer,
                                                         sf::RenderWindow* legacyWindow,
                                                         Scene* legacyScene,
                                                         const RenderPipelineFrame2D& frame);

        std::vector<PassRecord> m_passes;
        bool m_executing = false;
    };
}

#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Renderer/Material2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace sf
{
    class RenderTarget;
}

namespace l2d
{
    class RenderStatisticsRecorder2D;

    enum class SpriteBatchFailure2D
    {
        None = 0,
        CapacityExceeded,
        TextureUnavailable,
        TextureRectInvalid,
        TransformInvalid,
        MaterialIncomplete,
        DestinationUnavailable,
        MaterialApplyFailed
    };

    [[nodiscard]] std::string_view spriteBatchFailureName(SpriteBatchFailure2D failure) noexcept;

    // textureRect == {{0, 0}, {0, 0}} selects the complete texture.
    // Explicit atlas rectangles must be positive and remain within the texture.
    struct SpriteBatchSubmission2D
    {
        TextureHandle texture;
        sf::IntRect textureRect;
        sf::Vector2f position;
        sf::Vector2f scale{1.f, 1.f};
        sf::Vector2f origin;
        float rotationDegrees = 0.f;
        sf::Color color = sf::Color::White;
        Material2DHandle material;
    };

    struct SpriteBatchSubmitResult2D
    {
        SpriteBatchFailure2D failure = SpriteBatchFailure2D::None;
        std::optional<std::size_t> batchIndex;

        [[nodiscard]] bool succeeded() const noexcept;
    };

    struct SpriteBatchStats2D
    {
        std::size_t submittedSpriteCount = 0u;
        std::size_t submittedVertexCount = 0u;
        std::size_t batchCount = 0u;
        std::size_t textureSwitchCount = 0u;
        std::size_t materialSwitchCount = 0u;
    };

    struct SpriteBatchDrawResult2D
    {
        SpriteBatchFailure2D failure = SpriteBatchFailure2D::None;
        std::size_t completedBatchCount = 0u;
        std::size_t drawCallCount = 0u;
        std::size_t renderedSpriteCount = 0u;
        std::optional<std::size_t> failedBatch;

        [[nodiscard]] bool succeeded() const noexcept;
    };

    // A bounded ordered submission buffer. Only adjacent submissions with the
    // same texture lease and material handle are coalesced; no sorting occurs.
    //
    // Material state is evaluated at draw() time. Mutating a retained material
    // before draw() affects every queued submission that references that handle.
    // Use distinct Material2D instances when queued items require different
    // material state.
    class SpriteBatch2D
    {
      public:
        static constexpr std::size_t MaximumSubmissionCount = 16384u;
        static constexpr std::size_t VerticesPerSprite = 6u;

        SpriteBatch2D() = default;
        SpriteBatch2D(const SpriteBatch2D&) = delete;
        SpriteBatch2D& operator=(const SpriteBatch2D&) = delete;
        SpriteBatch2D(SpriteBatch2D&&) noexcept = default;
        SpriteBatch2D& operator=(SpriteBatch2D&&) noexcept = default;

        [[nodiscard]] static bool isValidSubmission(
            const SpriteBatchSubmission2D& submission) noexcept;

        [[nodiscard]] SpriteBatchSubmitResult2D submit(SpriteBatchSubmission2D submission);
        void clear() noexcept;

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] std::size_t submissionCount() const noexcept;
        [[nodiscard]] std::size_t batchCount() const noexcept;
        [[nodiscard]] const SpriteBatchStats2D& stats() const noexcept;

        // Draws in the target's current view. On success one draw call is issued
        // per retained batch. Material completeness is preflighted before any
        // draw side effect. The original overload remains source-compatible;
        // the recorder-aware overload records only completed draws and preserves
        // the actual state-switch order.
        [[nodiscard]] SpriteBatchDrawResult2D draw(sf::RenderTarget& destination) const;
        [[nodiscard]] SpriteBatchDrawResult2D draw(sf::RenderTarget& destination,
                                                   RenderStatisticsRecorder2D* statistics) const;

      private:
        struct Batch
        {
            TextureHandle texture;
            Material2DHandle material;
            sf::VertexArray vertices{sf::PrimitiveType::Triangles};
            std::size_t spriteCount = 0u;
        };

        [[nodiscard]] static bool compatible(const Batch& batch,
                                             const SpriteBatchSubmission2D& submission) noexcept;

        std::vector<Batch> m_batches;
        SpriteBatchStats2D m_stats;
    };
}

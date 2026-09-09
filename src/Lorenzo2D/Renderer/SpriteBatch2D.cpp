#include <Lorenzo2D/Renderer/SpriteBatch2D.hpp>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        struct PreparedSubmission2D
        {
            std::array<sf::Vertex, SpriteBatch2D::VerticesPerSprite> vertices;
        };

        bool finite(sf::Vector2f value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool isDefaultTextureRect(const sf::IntRect& rectangle) noexcept
        {
            return rectangle.position == sf::Vector2i{} && rectangle.size == sf::Vector2i{};
        }

        SpriteBatchFailure2D resolveTextureRect(const SpriteBatchSubmission2D& submission,
                                                sf::IntRect& resolved) noexcept
        {
            if (!submission.texture) return SpriteBatchFailure2D::TextureUnavailable;

            const sf::Vector2u textureSize = submission.texture->getSize();
            if (textureSize.x == 0u || textureSize.y == 0u)
                return SpriteBatchFailure2D::TextureUnavailable;

            if (isDefaultTextureRect(submission.textureRect))
            {
                constexpr unsigned int MaximumInt =
                    static_cast<unsigned int>(std::numeric_limits<int>::max());
                if (textureSize.x > MaximumInt || textureSize.y > MaximumInt)
                    return SpriteBatchFailure2D::TextureRectInvalid;

                resolved = {{0, 0},
                            {static_cast<int>(textureSize.x), static_cast<int>(textureSize.y)}};
                return SpriteBatchFailure2D::None;
            }

            const sf::IntRect& rectangle = submission.textureRect;
            if (rectangle.position.x < 0 || rectangle.position.y < 0 || rectangle.size.x <= 0 ||
                rectangle.size.y <= 0)
            {
                return SpriteBatchFailure2D::TextureRectInvalid;
            }

            const std::int64_t right = static_cast<std::int64_t>(rectangle.position.x) +
                                       static_cast<std::int64_t>(rectangle.size.x);
            const std::int64_t bottom = static_cast<std::int64_t>(rectangle.position.y) +
                                        static_cast<std::int64_t>(rectangle.size.y);

            if (right > static_cast<std::int64_t>(textureSize.x) ||
                bottom > static_cast<std::int64_t>(textureSize.y))
            {
                return SpriteBatchFailure2D::TextureRectInvalid;
            }

            resolved = rectangle;
            return SpriteBatchFailure2D::None;
        }

        SpriteBatchFailure2D prepareSubmission(const SpriteBatchSubmission2D& submission,
                                               PreparedSubmission2D* prepared) noexcept
        {
            sf::IntRect rectangle;
            const SpriteBatchFailure2D rectangleFailure = resolveTextureRect(submission, rectangle);
            if (rectangleFailure != SpriteBatchFailure2D::None) return rectangleFailure;

            if (!finite(submission.position) || !finite(submission.scale) ||
                !finite(submission.origin) || !std::isfinite(submission.rotationDegrees))
            {
                return SpriteBatchFailure2D::TransformInvalid;
            }

            if (submission.material && !submission.material->isComplete())
                return SpriteBatchFailure2D::MaterialIncomplete;

            const float width = static_cast<float>(rectangle.size.x);
            const float height = static_cast<float>(rectangle.size.y);
            const std::array<sf::Vector2f, SpriteBatch2D::VerticesPerSprite> localPositions = {
                sf::Vector2f{0.f, 0.f}, sf::Vector2f{0.f, height},   sf::Vector2f{width, height},
                sf::Vector2f{0.f, 0.f}, sf::Vector2f{width, height}, sf::Vector2f{width, 0.f}};

            const float left = static_cast<float>(rectangle.position.x);
            const float top = static_cast<float>(rectangle.position.y);
            const float right = static_cast<float>(rectangle.position.x + rectangle.size.x);
            const float bottom = static_cast<float>(rectangle.position.y + rectangle.size.y);
            const std::array<sf::Vector2f, SpriteBatch2D::VerticesPerSprite> textureCoordinates = {
                sf::Vector2f{left, top}, sf::Vector2f{left, bottom},  sf::Vector2f{right, bottom},
                sf::Vector2f{left, top}, sf::Vector2f{right, bottom}, sf::Vector2f{right, top}};

            const double radians = static_cast<double>(submission.rotationDegrees) * Pi / 180.0;
            const double cosine = std::cos(radians);
            const double sine = std::sin(radians);

            std::array<sf::Vertex, SpriteBatch2D::VerticesPerSprite> vertices;
            for (std::size_t index = 0u; index < vertices.size(); ++index)
            {
                const double localX =
                    (static_cast<double>(localPositions[index].x) - submission.origin.x) *
                    submission.scale.x;
                const double localY =
                    (static_cast<double>(localPositions[index].y) - submission.origin.y) *
                    submission.scale.y;
                const double transformedX =
                    static_cast<double>(submission.position.x) + cosine * localX - sine * localY;
                const double transformedY =
                    static_cast<double>(submission.position.y) + sine * localX + cosine * localY;

                if (!std::isfinite(transformedX) || !std::isfinite(transformedY))
                    return SpriteBatchFailure2D::TransformInvalid;

                const sf::Vector2f transformed{static_cast<float>(transformedX),
                                               static_cast<float>(transformedY)};
                if (!finite(transformed)) return SpriteBatchFailure2D::TransformInvalid;

                vertices[index].position = transformed;
                vertices[index].color = submission.color;
                vertices[index].texCoords = textureCoordinates[index];
            }

            if (prepared != nullptr) prepared->vertices = vertices;

            return SpriteBatchFailure2D::None;
        }

        SpriteBatchDrawResult2D drawFailure(SpriteBatchFailure2D failure,
                                            std::optional<std::size_t> failedBatch = std::nullopt,
                                            std::size_t completedBatchCount = 0u,
                                            std::size_t drawCallCount = 0u,
                                            std::size_t renderedSpriteCount = 0u)
        {
            SpriteBatchDrawResult2D result;
            result.failure = failure;
            result.failedBatch = failedBatch;
            result.completedBatchCount = completedBatchCount;
            result.drawCallCount = drawCallCount;
            result.renderedSpriteCount = renderedSpriteCount;
            return result;
        }
    }

    std::string_view spriteBatchFailureName(SpriteBatchFailure2D failure) noexcept
    {
        switch (failure)
        {
        case SpriteBatchFailure2D::None:
            return "none";
        case SpriteBatchFailure2D::CapacityExceeded:
            return "capacity-exceeded";
        case SpriteBatchFailure2D::TextureUnavailable:
            return "texture-unavailable";
        case SpriteBatchFailure2D::TextureRectInvalid:
            return "texture-rect-invalid";
        case SpriteBatchFailure2D::TransformInvalid:
            return "transform-invalid";
        case SpriteBatchFailure2D::MaterialIncomplete:
            return "material-incomplete";
        case SpriteBatchFailure2D::DestinationUnavailable:
            return "destination-unavailable";
        case SpriteBatchFailure2D::MaterialApplyFailed:
            return "material-apply-failed";
        }

        return "unknown";
    }

    bool SpriteBatchSubmitResult2D::succeeded() const noexcept
    {
        return failure == SpriteBatchFailure2D::None;
    }

    bool SpriteBatchDrawResult2D::succeeded() const noexcept
    {
        return failure == SpriteBatchFailure2D::None;
    }

    bool SpriteBatch2D::isValidSubmission(const SpriteBatchSubmission2D& submission) noexcept
    {
        return prepareSubmission(submission, nullptr) == SpriteBatchFailure2D::None;
    }

    SpriteBatchSubmitResult2D SpriteBatch2D::submit(SpriteBatchSubmission2D submission)
    {
        if (m_stats.submittedSpriteCount >= MaximumSubmissionCount)
            return {SpriteBatchFailure2D::CapacityExceeded, std::nullopt};

        PreparedSubmission2D prepared;
        const SpriteBatchFailure2D failure = prepareSubmission(submission, &prepared);
        if (failure != SpriteBatchFailure2D::None) return {failure, std::nullopt};

        std::size_t batchIndex = 0u;
        if (!m_batches.empty() && compatible(m_batches.back(), submission))
        {
            batchIndex = m_batches.size() - 1u;
        }
        else
        {
            if (!m_batches.empty())
            {
                const Batch& previous = m_batches.back();
                if (previous.texture != submission.texture) ++m_stats.textureSwitchCount;
                if (previous.material.get() != submission.material.get())
                    ++m_stats.materialSwitchCount;
            }

            Batch batch;
            batch.texture = submission.texture;
            batch.material = submission.material;
            m_batches.push_back(std::move(batch));
            batchIndex = m_batches.size() - 1u;
            m_stats.batchCount = m_batches.size();
        }

        Batch& batch = m_batches[batchIndex];
        for (const sf::Vertex& vertex : prepared.vertices)
            batch.vertices.append(vertex);
        ++batch.spriteCount;

        ++m_stats.submittedSpriteCount;
        m_stats.submittedVertexCount += VerticesPerSprite;
        return {SpriteBatchFailure2D::None, batchIndex};
    }

    void SpriteBatch2D::clear() noexcept
    {
        m_batches.clear();
        m_stats = {};
    }

    bool SpriteBatch2D::empty() const noexcept
    {
        return m_stats.submittedSpriteCount == 0u;
    }

    std::size_t SpriteBatch2D::submissionCount() const noexcept
    {
        return m_stats.submittedSpriteCount;
    }

    std::size_t SpriteBatch2D::batchCount() const noexcept
    {
        return m_batches.size();
    }

    const SpriteBatchStats2D& SpriteBatch2D::stats() const noexcept
    {
        return m_stats;
    }

    SpriteBatchDrawResult2D SpriteBatch2D::draw(sf::RenderTarget& destination) const
    {
        if (m_batches.empty()) return {};

        const sf::Vector2u destinationSize = destination.getSize();
        if (destinationSize.x == 0u || destinationSize.y == 0u)
            return drawFailure(SpriteBatchFailure2D::DestinationUnavailable);

        for (std::size_t index = 0u; index < m_batches.size(); ++index)
        {
            const Batch& batch = m_batches[index];
            if (!batch.texture) return drawFailure(SpriteBatchFailure2D::TextureUnavailable, index);
            if (batch.material && !batch.material->isComplete())
                return drawFailure(SpriteBatchFailure2D::MaterialIncomplete, index);
        }

        SpriteBatchDrawResult2D result;
        for (std::size_t index = 0u; index < m_batches.size(); ++index)
        {
            const Batch& batch = m_batches[index];
            sf::RenderStates states;
            states.texture = batch.texture.get();

            if (batch.material && !batch.material->apply(states))
            {
                return drawFailure(SpriteBatchFailure2D::MaterialApplyFailed, index,
                                   result.completedBatchCount, result.drawCallCount,
                                   result.renderedSpriteCount);
            }

            destination.draw(batch.vertices, states);
            ++result.completedBatchCount;
            ++result.drawCallCount;
            result.renderedSpriteCount += batch.spriteCount;
        }

        return result;
    }

    bool SpriteBatch2D::compatible(const Batch& batch,
                                   const SpriteBatchSubmission2D& submission) noexcept
    {
        return batch.texture == submission.texture &&
               batch.material.get() == submission.material.get();
    }
}

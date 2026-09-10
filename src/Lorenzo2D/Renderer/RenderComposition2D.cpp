#include <Lorenzo2D/Renderer/RenderComposition2D.hpp>

#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace l2d
{
    namespace
    {
        RenderCompositionResult2D failureResult(
            RenderCompositionFailure2D failure,
            std::optional<std::size_t> failedEntry = std::nullopt,
            std::size_t completedEntries = 0u)
        {
            RenderCompositionResult2D result;
            result.failure = failure;
            result.failedEntry = failedEntry;
            result.completedEntries = completedEntries;
            return result;
        }

        class ScopedRenderView2D final
        {
          public:
            explicit ScopedRenderView2D(sf::RenderTarget& target)
                : m_target(target), m_view(target.getView())
            {
            }

            ~ScopedRenderView2D()
            {
                m_target.setView(m_view);
            }

            ScopedRenderView2D(const ScopedRenderView2D&) = delete;
            ScopedRenderView2D& operator=(const ScopedRenderView2D&) = delete;

          private:
            sf::RenderTarget& m_target;
            sf::View m_view;
        };

        class ScopedExecutionFlag2D final
        {
          public:
            explicit ScopedExecutionFlag2D(bool& executing) : m_executing(executing)
            {
                m_executing = true;
            }

            ~ScopedExecutionFlag2D()
            {
                m_executing = false;
            }

            ScopedExecutionFlag2D(const ScopedExecutionFlag2D&) = delete;
            ScopedExecutionFlag2D& operator=(const ScopedExecutionFlag2D&) = delete;

          private:
            bool& m_executing;
        };

        struct PreparedCompositionEntry2D
        {
            std::size_t index = 0u;
            RenderContext2D context;
            sf::View view;
        };
    }

    std::string_view renderCompositionFailureName(RenderCompositionFailure2D failure) noexcept
    {
        switch (failure)
        {
        case RenderCompositionFailure2D::None:
            return "none";
        case RenderCompositionFailure2D::ReentrantExecution:
            return "reentrant-execution";
        case RenderCompositionFailure2D::InvalidFrame:
            return "invalid-frame";
        case RenderCompositionFailure2D::InvalidCallback:
            return "invalid-callback";
        case RenderCompositionFailure2D::InvalidEntry:
            return "invalid-entry";
        case RenderCompositionFailure2D::CallbackFailed:
            return "callback-failed";
        }

        return "unknown";
    }

    bool RenderCompositionResult2D::succeeded() const noexcept
    {
        return failure == RenderCompositionFailure2D::None;
    }

    RenderCompositionResult2D RenderComposition2D::execute(
        sf::RenderTarget& target, RenderCompositionCallback2D callback,
        const RenderCompositionFrame2D& frame)
    {
        if (m_executing)
            return failureResult(RenderCompositionFailure2D::ReentrantExecution);
        if (!isValidFrame(frame)) return failureResult(RenderCompositionFailure2D::InvalidFrame);
        if (!callback) return failureResult(RenderCompositionFailure2D::InvalidCallback);

        std::vector<PreparedCompositionEntry2D> prepared;
        prepared.reserve(m_entries.size());

        for (std::size_t index = 0u; index < m_entries.size(); ++index)
        {
            const RenderCompositionEntry2D& entryConfig = m_entries[index];
            if (!entryConfig.enabled) continue;

            if (!isValidEntry(entryConfig))
                return failureResult(RenderCompositionFailure2D::InvalidEntry, index);

            const std::optional<RenderContext2D> context = makeContext(index, frame);
            const std::optional<sf::View> view = makeView(index);
            if (!context || !view)
                return failureResult(RenderCompositionFailure2D::InvalidEntry, index);

            prepared.push_back({index, *context, *view});
        }

        ScopedExecutionFlag2D executionGuard(m_executing);
        ScopedRenderView2D viewGuard(target);
        RenderCompositionResult2D result;

        for (const PreparedCompositionEntry2D& preparedEntry : prepared)
        {
            target.setView(preparedEntry.view);

            const RenderCompositionExecution2D execution{
                target, m_entries[preparedEntry.index], preparedEntry.context, preparedEntry.index};

            if (!callback(execution))
            {
                return failureResult(RenderCompositionFailure2D::CallbackFailed,
                                     preparedEntry.index, result.completedEntries);
            }

            ++result.completedEntries;
        }

        return result;
    }

    RenderCompositionResult2D RenderComposition2D::execute(
        sf::RenderWindow& window, Scene& legacyScene, const RenderCompositionFrame2D& frame)
    {
        RenderCompositionCallback2D callback =
            [&window, &legacyScene](const RenderCompositionExecution2D& execution)
        {
            legacyScene.render(window, execution.context);
            return true;
        };

        return execute(static_cast<sf::RenderTarget&>(window), std::move(callback), frame);
    }
}

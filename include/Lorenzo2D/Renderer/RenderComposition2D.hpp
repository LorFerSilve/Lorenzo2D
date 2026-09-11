#pragma once

#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sf
{
    class RenderTarget;
    class RenderWindow;
}

namespace l2d
{
    class RenderStatisticsRecorder2D;
    class Scene;

    enum class RenderCompositionFailure2D : std::uint8_t
    {
        None,
        ReentrantExecution,
        InvalidFrame,
        InvalidCallback,
        InvalidEntry,
        CallbackFailed
    };

    [[nodiscard]] std::string_view renderCompositionFailureName(
        RenderCompositionFailure2D failure) noexcept;

    struct RenderCompositionEntry2D
    {
        std::string name;
        const Camera2D* camera = nullptr;
        RenderPass2D pass = RenderPass2D::World;
        RenderLayerRange2D layers;
        sf::FloatRect viewport{{0.f, 0.f}, {1.f, 1.f}};
        bool enabled = true;
    };

    struct RenderCompositionFrame2D
    {
        constexpr RenderCompositionFrame2D(
            float interpolationAlphaValue = 1.f,
            const CoordinateProjection2D* projectionValue = nullptr,
            RenderStatisticsRecorder2D* statisticsValue = nullptr) noexcept
            : interpolationAlpha(interpolationAlphaValue), projection(projectionValue),
              statistics(statisticsValue)
        {
        }

        float interpolationAlpha = 1.f;
        const CoordinateProjection2D* projection = nullptr;
        RenderStatisticsRecorder2D* statistics = nullptr;
    };

    struct RenderCompositionExecution2D
    {
        sf::RenderTarget& target;
        const RenderCompositionEntry2D& entry;
        const RenderContext2D& context;
        std::size_t entryIndex = 0u;
    };

    using RenderCompositionCallback2D =
        std::function<bool(const RenderCompositionExecution2D& execution)>;

    struct RenderCompositionResult2D
    {
        RenderCompositionFailure2D failure = RenderCompositionFailure2D::None;
        std::size_t completedEntries = 0u;
        std::optional<std::size_t> failedEntry;

        [[nodiscard]] bool succeeded() const noexcept;
    };

    // Ordered camera/pass/layer/viewport composition plan. Entries borrow
    // Camera2D instances; this class never owns or mutates the referenced
    // cameras. Execution snapshots enabled camera views before the first
    // callback and restores the target's incoming view on every exit path.
    class RenderComposition2D
    {
      public:
        static constexpr std::size_t MaximumEntryCount = 16u;
        static constexpr std::size_t MaximumEntryNameBytes = 128u;

        RenderComposition2D() = default;
        ~RenderComposition2D() = default;

        RenderComposition2D(const RenderComposition2D&) = delete;
        RenderComposition2D& operator=(const RenderComposition2D&) = delete;
        RenderComposition2D(RenderComposition2D&&) = delete;
        RenderComposition2D& operator=(RenderComposition2D&&) = delete;

        [[nodiscard]] static bool isValidFrame(const RenderCompositionFrame2D& frame) noexcept
        {
            return std::isfinite(frame.interpolationAlpha) && frame.interpolationAlpha >= 0.f &&
                   frame.interpolationAlpha <= 1.f;
        }

        [[nodiscard]] static bool isValidEntry(const RenderCompositionEntry2D& entry) noexcept
        {
            return !entry.name.empty() && entry.name.size() <= MaximumEntryNameBytes &&
                   entry.camera != nullptr && isValidRenderPass(entry.pass) &&
                   entry.layers.isValid() && isValidViewport(entry.viewport);
        }

        [[nodiscard]] bool addEntry(RenderCompositionEntry2D entry)
        {
            if (m_executing || m_entries.size() >= MaximumEntryCount || !isValidEntry(entry) ||
                !isEntryNameAvailable(entry.name, std::nullopt))
            {
                return false;
            }

            m_entries.push_back(std::move(entry));
            return true;
        }

        [[nodiscard]] bool setEntry(std::size_t index, RenderCompositionEntry2D entry)
        {
            if (m_executing || index >= m_entries.size() || !isValidEntry(entry) ||
                !isEntryNameAvailable(entry.name, index))
            {
                return false;
            }

            m_entries[index] = std::move(entry);
            return true;
        }

        [[nodiscard]] bool setEntryEnabled(std::size_t index, bool enabled) noexcept
        {
            if (m_executing || index >= m_entries.size()) return false;

            m_entries[index].enabled = enabled;
            return true;
        }

        [[nodiscard]] bool moveEntry(std::size_t from, std::size_t to)
        {
            if (m_executing || from >= m_entries.size() || to >= m_entries.size()) return false;
            if (from == to) return true;

            RenderCompositionEntry2D moved = std::move(m_entries[from]);
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(from));
            m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
            return true;
        }

        [[nodiscard]] bool removeEntry(std::size_t index)
        {
            if (m_executing || index >= m_entries.size()) return false;

            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }

        [[nodiscard]] bool clear()
        {
            if (m_executing) return false;

            m_entries.clear();
            return true;
        }

        [[nodiscard]] std::size_t entryCount() const noexcept
        {
            return m_entries.size();
        }

        [[nodiscard]] const RenderCompositionEntry2D* entry(std::size_t index) const noexcept
        {
            return index < m_entries.size() ? &m_entries[index] : nullptr;
        }

        [[nodiscard]] bool executing() const noexcept
        {
            return m_executing;
        }

        // Disabled or invalid indices return nullopt. The generated context
        // carries the entry's pass/layer filter and statistics recorder while
        // the generated view is a copy of the borrowed camera view with only
        // the viewport overridden.
        [[nodiscard]] std::optional<RenderContext2D> makeContext(
            std::size_t index, const RenderCompositionFrame2D& frame = {}) const noexcept
        {
            if (index >= m_entries.size() || !m_entries[index].enabled || !isValidFrame(frame))
                return std::nullopt;

            RenderContext2D context;
            context.interpolationAlpha = frame.interpolationAlpha;
            context.projection = frame.projection;
            context.pass = m_entries[index].pass;
            context.layers = m_entries[index].layers;
            context.statistics = frame.statistics;
            return context;
        }

        [[nodiscard]] std::optional<sf::View> makeView(std::size_t index) const
        {
            if (index >= m_entries.size() || !m_entries[index].enabled) return std::nullopt;

            sf::View view = m_entries[index].camera->view();
            view.setViewport(m_entries[index].viewport);
            return view;
        }

        // Generic ordered execution against any SFML render target. The
        // callback and its RenderCompositionExecution2D references are valid
        // only for the duration of each callback invocation. Returning false
        // stops execution and reports CallbackFailed.
        [[nodiscard]] RenderCompositionResult2D execute(sf::RenderTarget& target,
                                                        RenderCompositionCallback2D callback,
                                                        const RenderCompositionFrame2D& frame = {});

        // Compatibility bridge for the existing Scene/Component RenderWindow
        // virtual contract. The Scene and window are borrowed only for this
        // call and are never retained by the composition.
        [[nodiscard]] RenderCompositionResult2D execute(sf::RenderWindow& window,
                                                        Scene& legacyScene,
                                                        const RenderCompositionFrame2D& frame = {});

      private:
        [[nodiscard]] static bool isValidRenderPass(RenderPass2D pass) noexcept
        {
            switch (pass)
            {
            case RenderPass2D::World:
            case RenderPass2D::PhysicsDebug:
            case RenderPass2D::UI:
                return true;
            case RenderPass2D::Count:
                return false;
            }

            return false;
        }

        [[nodiscard]] static bool isValidViewport(const sf::FloatRect& viewport) noexcept
        {
            const float left = viewport.position.x;
            const float top = viewport.position.y;
            const float width = viewport.size.x;
            const float height = viewport.size.y;

            if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(width) ||
                !std::isfinite(height))
            {
                return false;
            }

            if (left < 0.f || top < 0.f || width <= 0.f || height <= 0.f) return false;

            const double right = static_cast<double>(left) + static_cast<double>(width);
            const double bottom = static_cast<double>(top) + static_cast<double>(height);
            return right <= 1.0 && bottom <= 1.0;
        }

        [[nodiscard]] bool isEntryNameAvailable(std::string_view name,
                                                std::optional<std::size_t> skip) const noexcept
        {
            for (std::size_t index = 0u; index < m_entries.size(); ++index)
            {
                if (skip && index == *skip) continue;
                if (m_entries[index].name == name) return false;
            }

            return true;
        }

        std::vector<RenderCompositionEntry2D> m_entries;
        bool m_executing = false;
    };
}

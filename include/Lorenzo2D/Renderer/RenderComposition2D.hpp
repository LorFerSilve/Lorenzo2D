#pragma once

#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace l2d
{
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
        float interpolationAlpha = 1.f;
        const CoordinateProjection2D* projection = nullptr;
    };

    // Ordered, CPU-side composition plan. Entries borrow Camera2D instances;
    // this class never owns or mutates the referenced cameras. Rendering the
    // plan is intentionally deferred to the next Phase 12 slice.
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
            if (m_entries.size() >= MaximumEntryCount || !isValidEntry(entry) ||
                !isEntryNameAvailable(entry.name, std::nullopt))
            {
                return false;
            }

            m_entries.push_back(std::move(entry));
            return true;
        }

        [[nodiscard]] bool setEntry(std::size_t index, RenderCompositionEntry2D entry)
        {
            if (index >= m_entries.size() || !isValidEntry(entry) ||
                !isEntryNameAvailable(entry.name, index))
            {
                return false;
            }

            m_entries[index] = std::move(entry);
            return true;
        }

        [[nodiscard]] bool setEntryEnabled(std::size_t index, bool enabled) noexcept
        {
            if (index >= m_entries.size()) return false;

            m_entries[index].enabled = enabled;
            return true;
        }

        [[nodiscard]] bool moveEntry(std::size_t from, std::size_t to)
        {
            if (from >= m_entries.size() || to >= m_entries.size()) return false;
            if (from == to) return true;

            RenderCompositionEntry2D moved = std::move(m_entries[from]);
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(from));
            m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
            return true;
        }

        [[nodiscard]] bool removeEntry(std::size_t index)
        {
            if (index >= m_entries.size()) return false;

            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }

        [[nodiscard]] bool clear()
        {
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

        // Disabled or invalid indices return nullopt. The generated context
        // carries the entry's pass/layer filter while the generated view is a
        // copy of the borrowed camera view with only the viewport overridden.
        [[nodiscard]] std::optional<RenderContext2D>
        makeContext(std::size_t index, const RenderCompositionFrame2D& frame = {}) const noexcept
        {
            if (index >= m_entries.size() || !m_entries[index].enabled || !isValidFrame(frame))
                return std::nullopt;

            RenderContext2D context;
            context.interpolationAlpha = frame.interpolationAlpha;
            context.projection = frame.projection;
            context.pass = m_entries[index].pass;
            context.layers = m_entries[index].layers;
            return context;
        }

        [[nodiscard]] std::optional<sf::View> makeView(std::size_t index) const
        {
            if (index >= m_entries.size() || !m_entries[index].enabled) return std::nullopt;

            sf::View view = m_entries[index].camera->view();
            view.setViewport(m_entries[index].viewport);
            return view;
        }

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
    };
}

#pragma once

#include <Lorenzo2D/Renderer/RenderContext2D.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/View.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace l2d
{
    class Camera2D;

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

        [[nodiscard]] static bool isValidFrame(const RenderCompositionFrame2D& frame) noexcept;
        [[nodiscard]] static bool isValidEntry(const RenderCompositionEntry2D& entry) noexcept;

        [[nodiscard]] bool addEntry(RenderCompositionEntry2D entry);
        [[nodiscard]] bool setEntry(std::size_t index, RenderCompositionEntry2D entry);
        [[nodiscard]] bool setEntryEnabled(std::size_t index, bool enabled) noexcept;
        [[nodiscard]] bool moveEntry(std::size_t from, std::size_t to);
        [[nodiscard]] bool removeEntry(std::size_t index);
        [[nodiscard]] bool clear();

        [[nodiscard]] std::size_t entryCount() const noexcept;
        [[nodiscard]] const RenderCompositionEntry2D* entry(std::size_t index) const noexcept;

        // Disabled or invalid indices return nullopt. The generated context
        // carries the entry's pass/layer filter while the generated view is a
        // copy of the borrowed camera view with only the viewport overridden.
        [[nodiscard]] std::optional<RenderContext2D>
        makeContext(std::size_t index, const RenderCompositionFrame2D& frame = {}) const noexcept;
        [[nodiscard]] std::optional<sf::View> makeView(std::size_t index) const;

      private:
        [[nodiscard]] bool isEntryNameAvailable(std::string_view name,
                                                std::optional<std::size_t> skip) const noexcept;

        std::vector<RenderCompositionEntry2D> m_entries;
    };
}

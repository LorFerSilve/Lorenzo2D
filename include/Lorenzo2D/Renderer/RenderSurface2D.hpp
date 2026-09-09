#pragma once

#include <Lorenzo2D/Renderer/Material2D.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace sf
{
    class RenderTarget;
    class RenderTexture;
    class Texture;
}

namespace l2d
{
    struct RenderSurfaceConfig2D
    {
        sf::Vector2u size{0u, 0u};
        bool smooth = false;
        bool repeated = false;
    };

    struct RenderSurfacePresent2D
    {
        sf::Vector2f position{0.f, 0.f};
        std::optional<sf::Vector2f> size;
        sf::Color color = sf::Color::White;
        Material2DHandle material;
    };

    // Owned off-screen 2D render target with texture output.
    //
    // Construction is CPU-only. create()/reset()/drawing/display/present are
    // graphics-resource operations and must follow the owning render
    // thread/context discipline.
    //
    // Pointers returned by target()/texture() are borrowed and are invalidated
    // by the next successful create() or reset(). The owning object itself has
    // stable identity: copying and moving are disabled so relocation cannot
    // silently invalidate borrowed pointers or generation observers.
    class RenderSurface2D
    {
      public:
        static constexpr unsigned int MaximumDimension = 8192u;
        static constexpr std::uint64_t MaximumPixels = 33554432ull;

        RenderSurface2D();
        ~RenderSurface2D();

        RenderSurface2D(const RenderSurface2D&) = delete;
        RenderSurface2D& operator=(const RenderSurface2D&) = delete;
        RenderSurface2D(RenderSurface2D&&) = delete;
        RenderSurface2D& operator=(RenderSurface2D&&) = delete;

        [[nodiscard]] static bool isValidSize(sf::Vector2u size) noexcept;
        [[nodiscard]] static bool isValidConfig(const RenderSurfaceConfig2D& config) noexcept;
        [[nodiscard]] static bool isValidPresent(const RenderSurfacePresent2D& present) noexcept;

        // Transactional replacement: validation or backend creation failure
        // preserves the currently allocated surface and its generation.
        [[nodiscard]] bool create(const RenderSurfaceConfig2D& config);
        void reset();

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] const RenderSurfaceConfig2D& config() const noexcept;
        [[nodiscard]] sf::Vector2u size() const noexcept;
        [[nodiscard]] std::uint64_t allocationGeneration() const noexcept;
        [[nodiscard]] std::uint64_t contentGeneration() const noexcept;

        [[nodiscard]] sf::RenderTarget* target() noexcept;
        [[nodiscard]] const sf::RenderTarget* target() const noexcept;
        [[nodiscard]] const sf::Texture* texture() const noexcept;

        [[nodiscard]] bool clear(sf::Color color = sf::Color::Black);
        [[nodiscard]] bool display();

        // Draw the current surface texture into another render target.
        // position/size use the destination target's current-view coordinates;
        // natural pixel size is used when present.size is absent.
        // Self-presentation is rejected to avoid read/write feedback on the
        // same render texture.
        [[nodiscard]] bool present(sf::RenderTarget& destination,
                                   const RenderSurfacePresent2D& present = {}) const;

      private:
        static void advanceGeneration(std::uint64_t& generation) noexcept;

        std::unique_ptr<sf::RenderTexture> m_surface;
        RenderSurfaceConfig2D m_config;
        std::uint64_t m_allocationGeneration = 0u;
        std::uint64_t m_contentGeneration = 0u;
    };

    using RenderSurface2DHandle = std::shared_ptr<RenderSurface2D>;
    using RenderSurface2DConstHandle = std::shared_ptr<const RenderSurface2D>;
}

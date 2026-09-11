#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>

#include <Lorenzo2D/Renderer/RenderStatistics2D.hpp>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Exception.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        bool isFinite(sf::Vector2f value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
    }

    RenderSurface2D::RenderSurface2D() = default;
    RenderSurface2D::~RenderSurface2D() = default;

    bool RenderSurface2D::isValidSize(sf::Vector2u size) noexcept
    {
        if (size.x == 0u || size.y == 0u) return false;
        if (size.x > MaximumDimension || size.y > MaximumDimension) return false;

        const std::uint64_t pixels =
            static_cast<std::uint64_t>(size.x) * static_cast<std::uint64_t>(size.y);
        return pixels <= MaximumPixels;
    }

    bool RenderSurface2D::isValidConfig(const RenderSurfaceConfig2D& config) noexcept
    {
        return isValidSize(config.size);
    }

    bool RenderSurface2D::isValidPresent(const RenderSurfacePresent2D& present) noexcept
    {
        if (!isFinite(present.position)) return false;
        if (!present.size) return true;

        return isFinite(*present.size) && present.size->x > 0.f && present.size->y > 0.f;
    }

    bool RenderSurface2D::create(const RenderSurfaceConfig2D& config)
    {
        if (!isValidConfig(config)) return false;

        std::unique_ptr<sf::RenderTexture> candidate;

        try
        {
            candidate = std::make_unique<sf::RenderTexture>(config.size);
        }
        catch (const sf::Exception&)
        {
            return false;
        }

        candidate->setSmooth(config.smooth);
        candidate->setRepeated(config.repeated);

        m_surface = std::move(candidate);
        m_config = config;
        advanceGeneration(m_allocationGeneration);
        m_contentGeneration = 0u;
        return true;
    }

    void RenderSurface2D::reset()
    {
        if (!m_surface) return;

        m_surface.reset();
        m_config = {};
        advanceGeneration(m_allocationGeneration);
        m_contentGeneration = 0u;
    }

    bool RenderSurface2D::ready() const noexcept
    {
        return m_surface != nullptr;
    }

    const RenderSurfaceConfig2D& RenderSurface2D::config() const noexcept
    {
        return m_config;
    }

    sf::Vector2u RenderSurface2D::size() const noexcept
    {
        return m_surface ? m_surface->getSize() : sf::Vector2u{};
    }

    std::uint64_t RenderSurface2D::allocationGeneration() const noexcept
    {
        return m_allocationGeneration;
    }

    std::uint64_t RenderSurface2D::contentGeneration() const noexcept
    {
        return m_contentGeneration;
    }

    sf::RenderTarget* RenderSurface2D::target() noexcept
    {
        return m_surface.get();
    }

    const sf::RenderTarget* RenderSurface2D::target() const noexcept
    {
        return m_surface.get();
    }

    const sf::Texture* RenderSurface2D::texture() const noexcept
    {
        return m_surface ? &m_surface->getTexture() : nullptr;
    }

    bool RenderSurface2D::clear(sf::Color color)
    {
        if (!m_surface) return false;
        m_surface->clear(color);
        return true;
    }

    bool RenderSurface2D::display()
    {
        if (!m_surface) return false;

        m_surface->display();
        advanceGeneration(m_contentGeneration);
        return true;
    }

    bool RenderSurface2D::present(sf::RenderTarget& destination,
                                  const RenderSurfacePresent2D& present) const
    {
        return this->present(destination, present, nullptr);
    }

    bool RenderSurface2D::present(sf::RenderTarget& destination,
                                  const RenderSurfacePresent2D& present,
                                  RenderStatisticsRecorder2D* statistics) const
    {
        if (!m_surface || !isValidPresent(present)) return false;
        if (&destination == static_cast<const sf::RenderTarget*>(m_surface.get())) return false;

        sf::Sprite sprite(m_surface->getTexture());
        sprite.setPosition(present.position);
        sprite.setColor(present.color);

        if (present.size)
        {
            const sf::Vector2u sourceSize = m_surface->getSize();
            sprite.setScale({present.size->x / static_cast<float>(sourceSize.x),
                             present.size->y / static_cast<float>(sourceSize.y)});
        }

        sf::RenderStates states;
        if (present.material && !present.material->apply(states)) return false;

        destination.draw(sprite, states);
        if (statistics != nullptr) statistics->recordDraw({2u, 1u, 1u, present.material});
        return true;
    }

    void RenderSurface2D::advanceGeneration(std::uint64_t& generation) noexcept
    {
        if (generation != std::numeric_limits<std::uint64_t>::max()) ++generation;
    }
}

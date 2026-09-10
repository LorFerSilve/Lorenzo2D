#include <Lorenzo2D/Renderer/RenderComposition2D.hpp>

#include <Lorenzo2D/Renderer/Camera2D.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace l2d
{
    namespace
    {
        bool isValidRenderPass(RenderPass2D pass) noexcept
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

        bool isValidViewport(const sf::FloatRect& viewport) noexcept
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
    }

    bool RenderComposition2D::isValidFrame(const RenderCompositionFrame2D& frame) noexcept
    {
        return std::isfinite(frame.interpolationAlpha) && frame.interpolationAlpha >= 0.f &&
               frame.interpolationAlpha <= 1.f;
    }

    bool RenderComposition2D::isValidEntry(const RenderCompositionEntry2D& entry) noexcept
    {
        return !entry.name.empty() && entry.name.size() <= MaximumEntryNameBytes &&
               entry.camera != nullptr && isValidRenderPass(entry.pass) && entry.layers.isValid() &&
               isValidViewport(entry.viewport);
    }

    bool RenderComposition2D::addEntry(RenderCompositionEntry2D entry)
    {
        if (m_entries.size() >= MaximumEntryCount || !isValidEntry(entry) ||
            !isEntryNameAvailable(entry.name, std::nullopt))
        {
            return false;
        }

        m_entries.push_back(std::move(entry));
        return true;
    }

    bool RenderComposition2D::setEntry(std::size_t index, RenderCompositionEntry2D entry)
    {
        if (index >= m_entries.size() || !isValidEntry(entry) ||
            !isEntryNameAvailable(entry.name, index))
        {
            return false;
        }

        m_entries[index] = std::move(entry);
        return true;
    }

    bool RenderComposition2D::setEntryEnabled(std::size_t index, bool enabled) noexcept
    {
        if (index >= m_entries.size()) return false;

        m_entries[index].enabled = enabled;
        return true;
    }

    bool RenderComposition2D::moveEntry(std::size_t from, std::size_t to)
    {
        if (from >= m_entries.size() || to >= m_entries.size()) return false;
        if (from == to) return true;

        RenderCompositionEntry2D moved = std::move(m_entries[from]);
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(from));
        m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
        return true;
    }

    bool RenderComposition2D::removeEntry(std::size_t index)
    {
        if (index >= m_entries.size()) return false;

        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool RenderComposition2D::clear()
    {
        m_entries.clear();
        return true;
    }

    std::size_t RenderComposition2D::entryCount() const noexcept
    {
        return m_entries.size();
    }

    const RenderCompositionEntry2D* RenderComposition2D::entry(std::size_t index) const noexcept
    {
        return index < m_entries.size() ? &m_entries[index] : nullptr;
    }

    std::optional<RenderContext2D>
    RenderComposition2D::makeContext(std::size_t index,
                                     const RenderCompositionFrame2D& frame) const noexcept
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

    std::optional<sf::View> RenderComposition2D::makeView(std::size_t index) const
    {
        if (index >= m_entries.size() || !m_entries[index].enabled) return std::nullopt;

        sf::View view = m_entries[index].camera->view();
        view.setViewport(m_entries[index].viewport);
        return view;
    }

    bool RenderComposition2D::isEntryNameAvailable(std::string_view name,
                                                    std::optional<std::size_t> skip) const noexcept
    {
        for (std::size_t index = 0u; index < m_entries.size(); ++index)
        {
            if (skip && index == *skip) continue;
            if (m_entries[index].name == name) return false;
        }

        return true;
    }
}

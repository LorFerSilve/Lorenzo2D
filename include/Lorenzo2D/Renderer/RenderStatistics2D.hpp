#pragma once

#include <Lorenzo2D/Renderer/Material2D.hpp>

#include <cstddef>
#include <limits>
#include <memory>

namespace l2d
{
    struct RenderStatistics2D
    {
        std::size_t drawCallCount = 0u;
        std::size_t submittedPrimitiveCount = 0u;
        std::size_t batchCount = 0u;
        std::size_t materialSwitchCount = 0u;
        std::size_t shaderSwitchCount = 0u;
        std::size_t renderedItemCount = 0u;
        std::size_t culledItemCount = 0u;
    };

    struct RenderDrawStatistics2D
    {
        std::size_t submittedPrimitiveCount = 0u;
        std::size_t renderedItemCount = 0u;
        std::size_t batchCount = 1u;
        Material2DHandle material;
    };

    // Ordered frame-local render telemetry. recordDraw() and
    // recordRepeatedDraws() must be called in the same order as the associated
    // draw submissions so material/shader switch counts describe the visible
    // draw stream rather than a post-hoc estimate.
    //
    // Draw records carry a Material2DHandle only for the duration of the call.
    // The recorder stores weak ownership identities, so it neither keeps the
    // material/shader alive nor confuses a later allocation that reuses the
    // same object address with the previously recorded state.
    class RenderStatisticsRecorder2D
    {
      public:
        [[nodiscard]] const RenderStatistics2D& statistics() const noexcept
        {
            return m_statistics;
        }

        void reset() noexcept
        {
            m_statistics = {};
            m_previousMaterial.reset();
            m_previousShader.reset();
            m_hasDrawState = false;
        }

        void recordDraw(const RenderDrawStatistics2D& draw) noexcept
        {
            recordRepeatedDraws(1u, draw);
        }

        // Records several consecutive draw calls that all use the same material
        // state. The numeric fields in draw describe totals across that run.
        void recordRepeatedDraws(std::size_t drawCallCount,
                                 const RenderDrawStatistics2D& draw) noexcept
        {
            if (drawCallCount == 0u) return;

            addSaturated(m_statistics.drawCallCount, drawCallCount);
            addSaturated(m_statistics.submittedPrimitiveCount, draw.submittedPrimitiveCount);
            addSaturated(m_statistics.batchCount, draw.batchCount);
            addSaturated(m_statistics.renderedItemCount, draw.renderedItemCount);
            recordState(draw.material);
        }

        void recordCulledItems(std::size_t count = 1u) noexcept
        {
            addSaturated(m_statistics.culledItemCount, count);
        }

      private:
        template <typename T>
        [[nodiscard]] static bool sameOwner(const std::weak_ptr<T>& left,
                                            const std::weak_ptr<T>& right) noexcept
        {
            return !left.owner_before(right) && !right.owner_before(left);
        }

        void recordState(const Material2DHandle& material) noexcept
        {
            const Shader2DHandle shader = material ? material->shader() : Shader2DHandle{};
            const std::weak_ptr<Material2D> materialIdentity = material;
            const std::weak_ptr<Shader2D> shaderIdentity = shader;

            if (m_hasDrawState)
            {
                if (!sameOwner(materialIdentity, m_previousMaterial))
                    addSaturated(m_statistics.materialSwitchCount, 1u);
                if (!sameOwner(shaderIdentity, m_previousShader))
                    addSaturated(m_statistics.shaderSwitchCount, 1u);
            }

            m_previousMaterial = materialIdentity;
            m_previousShader = shaderIdentity;
            m_hasDrawState = true;
        }

        static void addSaturated(std::size_t& destination, std::size_t amount) noexcept
        {
            const std::size_t maximum = std::numeric_limits<std::size_t>::max();
            destination = amount > maximum - destination ? maximum : destination + amount;
        }

        RenderStatistics2D m_statistics;
        std::weak_ptr<Material2D> m_previousMaterial;
        std::weak_ptr<Shader2D> m_previousShader;
        bool m_hasDrawState = false;
    };
}

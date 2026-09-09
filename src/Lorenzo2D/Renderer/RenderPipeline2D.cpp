#include <Lorenzo2D/Renderer/RenderPipeline2D.hpp>

#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

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

        bool isValidTarget(RenderPipelinePassTarget2D target) noexcept
        {
            switch (target)
            {
            case RenderPipelinePassTarget2D::Backbuffer:
            case RenderPipelinePassTarget2D::Surface:
                return true;
            }

            return false;
        }

        RenderPipelineResult2D failureResult(RenderPipelineFailure2D failure,
                                             std::optional<std::size_t> failedPass = std::nullopt,
                                             std::size_t completedPasses = 0u)
        {
            RenderPipelineResult2D result;
            result.failure = failure;
            result.failedPass = failedPass;
            result.completedPasses = completedPasses;
            return result;
        }
    }

    std::string_view renderPipelineFailureName(RenderPipelineFailure2D failure) noexcept
    {
        switch (failure)
        {
        case RenderPipelineFailure2D::None:
            return "none";
        case RenderPipelineFailure2D::ReentrantExecution:
            return "reentrant-execution";
        case RenderPipelineFailure2D::InvalidFrame:
            return "invalid-frame";
        case RenderPipelineFailure2D::InvalidPass:
            return "invalid-pass";
        case RenderPipelineFailure2D::LegacySceneRequired:
            return "legacy-scene-required";
        case RenderPipelineFailure2D::SurfaceUnavailable:
            return "surface-unavailable";
        case RenderPipelineFailure2D::InputUnavailable:
            return "input-unavailable";
        case RenderPipelineFailure2D::FeedbackLoop:
            return "feedback-loop";
        case RenderPipelineFailure2D::CallbackFailed:
            return "callback-failed";
        case RenderPipelineFailure2D::SurfaceClearFailed:
            return "surface-clear-failed";
        case RenderPipelineFailure2D::SurfacePublishFailed:
            return "surface-publish-failed";
        case RenderPipelineFailure2D::SurfacePresentFailed:
            return "surface-present-failed";
        }

        return "unknown";
    }

    bool RenderPipelineResult2D::succeeded() const noexcept
    {
        return failure == RenderPipelineFailure2D::None;
    }

    bool RenderPipeline2D::isValidFrame(const RenderPipelineFrame2D& frame) noexcept
    {
        return std::isfinite(frame.interpolationAlpha) && frame.interpolationAlpha >= 0.f &&
               frame.interpolationAlpha <= 1.f;
    }

    bool RenderPipeline2D::isValidPass(const RenderPipelinePass2D& pass) noexcept
    {
        if (pass.name.empty() || pass.name.size() > MaximumPassNameBytes ||
            pass.name.find('\0') != std::string::npos)
        {
            return false;
        }

        if (!isValidRenderPass(pass.pass) || !isValidTarget(pass.target) ||
            pass.inputs.size() > MaximumInputCount)
        {
            return false;
        }

        if (pass.target == RenderPipelinePassTarget2D::Backbuffer)
        {
            if (pass.surface != nullptr || pass.present.enabled) return false;
        }
        else
        {
            if (pass.surface == nullptr) return false;

            if (pass.present.enabled &&
                (!RenderSurface2D::isValidPresent(pass.present.presentation) ||
                 (pass.present.presentation.material != nullptr &&
                  !pass.present.presentation.material->isComplete())))
            {
                return false;
            }
        }

        for (std::size_t index = 0u; index < pass.inputs.size(); ++index)
        {
            const RenderSurface2DConstHandle& input = pass.inputs[index];
            if (input == nullptr) return false;
            if (pass.surface != nullptr && input.get() == pass.surface.get()) return false;

            for (std::size_t previous = 0u; previous < index; ++previous)
            {
                if (pass.inputs[previous].get() == input.get()) return false;
            }
        }

        return true;
    }

    bool RenderPipeline2D::addPass(RenderPipelinePass2D pass,
                                   RenderPipelineCallback2D callback)
    {
        if (m_executing || m_passes.size() >= MaximumPassCount || !callback ||
            !isValidPass(pass) || !isPassNameAvailable(pass.name, std::nullopt))
        {
            return false;
        }

        m_passes.push_back({std::move(pass), std::move(callback), false});
        return true;
    }

    bool RenderPipeline2D::addLegacyScenePass(RenderPipelinePass2D pass)
    {
        if (m_executing || m_passes.size() >= MaximumPassCount ||
            !isValidLegacyScenePass(pass) || !isPassNameAvailable(pass.name, std::nullopt))
        {
            return false;
        }

        m_passes.push_back({std::move(pass), {}, true});
        return true;
    }

    bool RenderPipeline2D::setPass(std::size_t index, RenderPipelinePass2D pass)
    {
        if (m_executing || index >= m_passes.size()) return false;

        const bool valid = m_passes[index].legacyScene ? isValidLegacyScenePass(pass)
                                                      : isValidPass(pass);
        if (!valid || !isPassNameAvailable(pass.name, index)) return false;

        m_passes[index].pass = std::move(pass);
        return true;
    }

    bool RenderPipeline2D::setPassEnabled(std::size_t index, bool enabled) noexcept
    {
        if (m_executing || index >= m_passes.size()) return false;

        m_passes[index].pass.enabled = enabled;
        return true;
    }

    bool RenderPipeline2D::movePass(std::size_t from, std::size_t to)
    {
        if (m_executing || from >= m_passes.size() || to >= m_passes.size()) return false;
        if (from == to) return true;

        PassRecord record = std::move(m_passes[from]);
        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(from));
        m_passes.insert(m_passes.begin() + static_cast<std::ptrdiff_t>(to), std::move(record));
        return true;
    }

    bool RenderPipeline2D::removePass(std::size_t index)
    {
        if (m_executing || index >= m_passes.size()) return false;

        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool RenderPipeline2D::clear()
    {
        if (m_executing) return false;

        m_passes.clear();
        return true;
    }

    std::size_t RenderPipeline2D::passCount() const noexcept
    {
        return m_passes.size();
    }

    const RenderPipelinePass2D* RenderPipeline2D::pass(std::size_t index) const noexcept
    {
        return index < m_passes.size() ? &m_passes[index].pass : nullptr;
    }

    bool RenderPipeline2D::isLegacyScenePass(std::size_t index) const noexcept
    {
        return index < m_passes.size() && m_passes[index].legacyScene;
    }

    bool RenderPipeline2D::executing() const noexcept
    {
        return m_executing;
    }

    RenderPipelineResult2D RenderPipeline2D::execute(sf::RenderTarget& backbuffer,
                                                     const RenderPipelineFrame2D& frame)
    {
        return executeImpl(backbuffer, nullptr, nullptr, frame);
    }

    RenderPipelineResult2D RenderPipeline2D::execute(sf::RenderWindow& window, Scene& legacyScene,
                                                     const RenderPipelineFrame2D& frame)
    {
        return executeImpl(window, &window, &legacyScene, frame);
    }

    bool RenderPipeline2D::isPassNameAvailable(std::string_view name,
                                               std::optional<std::size_t> skip) const noexcept
    {
        for (std::size_t index = 0u; index < m_passes.size(); ++index)
        {
            if (skip && *skip == index) continue;
            if (m_passes[index].pass.name == name) return false;
        }

        return true;
    }

    bool RenderPipeline2D::isValidLegacyScenePass(const RenderPipelinePass2D& pass) noexcept
    {
        return isValidPass(pass) && pass.target == RenderPipelinePassTarget2D::Backbuffer &&
               pass.inputs.empty() && !pass.present.enabled;
    }

    RenderPipelineResult2D RenderPipeline2D::preflight(
        sf::RenderTarget& backbuffer, bool legacySceneAvailable,
        const RenderPipelineFrame2D& frame) const
    {
        if (!isValidFrame(frame))
            return failureResult(RenderPipelineFailure2D::InvalidFrame);

        for (std::size_t index = 0u; index < m_passes.size(); ++index)
        {
            const PassRecord& record = m_passes[index];
            const RenderPipelinePass2D& passConfig = record.pass;
            if (!passConfig.enabled) continue;

            const bool valid = record.legacyScene ? isValidLegacyScenePass(passConfig)
                                                  : isValidPass(passConfig);
            if (!valid)
                return failureResult(RenderPipelineFailure2D::InvalidPass, index);

            if (record.legacyScene && !legacySceneAvailable)
                return failureResult(RenderPipelineFailure2D::LegacySceneRequired, index);

            const sf::RenderTarget* outputTarget = &backbuffer;

            if (passConfig.target == RenderPipelinePassTarget2D::Surface)
            {
                if (!passConfig.surface->ready() || passConfig.surface->target() == nullptr ||
                    passConfig.surface->texture() == nullptr)
                {
                    return failureResult(RenderPipelineFailure2D::SurfaceUnavailable, index);
                }

                outputTarget = passConfig.surface->target();

                if (passConfig.present.enabled && outputTarget == &backbuffer)
                    return failureResult(RenderPipelineFailure2D::FeedbackLoop, index);
            }

            for (const RenderSurface2DConstHandle& input : passConfig.inputs)
            {
                if (!input->ready() || input->target() == nullptr || input->texture() == nullptr)
                    return failureResult(RenderPipelineFailure2D::InputUnavailable, index);

                if (input->target() == outputTarget)
                    return failureResult(RenderPipelineFailure2D::FeedbackLoop, index);
            }
        }

        return {};
    }

    RenderPipelineResult2D RenderPipeline2D::executeImpl(
        sf::RenderTarget& backbuffer, sf::RenderWindow* legacyWindow, Scene* legacyScene,
        const RenderPipelineFrame2D& frame)
    {
        if (m_executing)
            return failureResult(RenderPipelineFailure2D::ReentrantExecution);

        const bool legacyAvailable = legacyWindow != nullptr && legacyScene != nullptr;
        RenderPipelineResult2D result = preflight(backbuffer, legacyAvailable, frame);
        if (!result.succeeded()) return result;

        m_executing = true;

        try
        {
            for (std::size_t index = 0u; index < m_passes.size(); ++index)
            {
                PassRecord& record = m_passes[index];
                RenderPipelinePass2D& passConfig = record.pass;
                if (!passConfig.enabled) continue;

                sf::RenderTarget* target = &backbuffer;
                if (passConfig.target == RenderPipelinePassTarget2D::Surface)
                    target = passConfig.surface->target();

                if (passConfig.clear.enabled)
                {
                    if (passConfig.target == RenderPipelinePassTarget2D::Surface)
                    {
                        if (!passConfig.surface->clear(passConfig.clear.color))
                        {
                            m_executing = false;
                            return failureResult(RenderPipelineFailure2D::SurfaceClearFailed, index,
                                                 result.completedPasses);
                        }
                    }
                    else
                    {
                        target->clear(passConfig.clear.color);
                    }
                }

                const RenderContext2D context{
                    frame.interpolationAlpha, frame.projection, passConfig.pass};

                if (record.legacyScene)
                {
                    legacyScene->render(*legacyWindow, context);
                }
                else
                {
                    const RenderPipelineExecution2D execution{*target, context, passConfig.inputs};
                    if (!record.callback(execution))
                    {
                        m_executing = false;
                        return failureResult(RenderPipelineFailure2D::CallbackFailed, index,
                                             result.completedPasses);
                    }
                }

                if (passConfig.target == RenderPipelinePassTarget2D::Surface)
                {
                    if (!passConfig.surface->display())
                    {
                        m_executing = false;
                        return failureResult(RenderPipelineFailure2D::SurfacePublishFailed, index,
                                             result.completedPasses);
                    }

                    if (passConfig.present.enabled &&
                        !passConfig.surface->present(backbuffer,
                                                     passConfig.present.presentation))
                    {
                        m_executing = false;
                        return failureResult(RenderPipelineFailure2D::SurfacePresentFailed, index,
                                             result.completedPasses);
                    }
                }

                ++result.completedPasses;
            }
        }
        catch (...)
        {
            m_executing = false;
            throw;
        }

        m_executing = false;
        return result;
    }
}

#include <Lorenzo2D/Renderer/ShaderPostProcessChain2D.hpp>

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <utility>

namespace l2d
{
    namespace
    {
        ShaderPostProcessResult2D failureResult(
            ShaderPostProcessFailure2D failure,
            std::optional<std::size_t> failedPass = std::nullopt,
            std::size_t completedPasses = 0u)
        {
            ShaderPostProcessResult2D result;
            result.failure = failure;
            result.failedPass = failedPass;
            result.completedPasses = completedPasses;
            return result;
        }

        class ScopedDefaultView2D final
        {
          public:
            explicit ScopedDefaultView2D(sf::RenderTarget& target)
                : m_target(target), m_previous(target.getView())
            {
                m_target.setView(m_target.getDefaultView());
            }

            ~ScopedDefaultView2D()
            {
                m_target.setView(m_previous);
            }

            ScopedDefaultView2D(const ScopedDefaultView2D&) = delete;
            ScopedDefaultView2D& operator=(const ScopedDefaultView2D&) = delete;

          private:
            sf::RenderTarget& m_target;
            sf::View m_previous;
        };
    }

    std::string_view shaderPostProcessFailureName(ShaderPostProcessFailure2D failure) noexcept
    {
        switch (failure)
        {
        case ShaderPostProcessFailure2D::None:
            return "none";
        case ShaderPostProcessFailure2D::InvalidPass:
            return "invalid-pass";
        case ShaderPostProcessFailure2D::SourceUnavailable:
            return "source-unavailable";
        case ShaderPostProcessFailure2D::SourceUnpublished:
            return "source-unpublished";
        case ShaderPostProcessFailure2D::DestinationUnavailable:
            return "destination-unavailable";
        case ShaderPostProcessFailure2D::FeedbackLoop:
            return "feedback-loop";
        case ShaderPostProcessFailure2D::ShaderUnavailable:
            return "shader-unavailable";
        case ShaderPostProcessFailure2D::MaterialIncomplete:
            return "material-incomplete";
        case ShaderPostProcessFailure2D::WorkspaceAllocationFailed:
            return "workspace-allocation-failed";
        case ShaderPostProcessFailure2D::WorkspaceClearFailed:
            return "workspace-clear-failed";
        case ShaderPostProcessFailure2D::MaterialApplyFailed:
            return "material-apply-failed";
        case ShaderPostProcessFailure2D::WorkspacePublishFailed:
            return "workspace-publish-failed";
        }

        return "unknown";
    }

    bool ShaderPostProcessResult2D::succeeded() const noexcept
    {
        return failure == ShaderPostProcessFailure2D::None;
    }

    bool ShaderPostProcessChain2D::isValidPass(const ShaderPostProcessPass2D& pass) noexcept
    {
        return !pass.name.empty() && pass.name.size() <= MaximumPassNameBytes &&
               pass.name.find('\0') == std::string::npos && pass.material != nullptr;
    }

    bool ShaderPostProcessChain2D::addPass(ShaderPostProcessPass2D pass)
    {
        if (m_passes.size() >= MaximumPassCount || !isValidPass(pass) ||
            !isPassNameAvailable(pass.name, std::nullopt))
        {
            return false;
        }

        m_passes.push_back(std::move(pass));
        return true;
    }

    bool ShaderPostProcessChain2D::setPass(std::size_t index, ShaderPostProcessPass2D pass)
    {
        if (index >= m_passes.size() || !isValidPass(pass) ||
            !isPassNameAvailable(pass.name, index))
        {
            return false;
        }

        m_passes[index] = std::move(pass);
        return true;
    }

    bool ShaderPostProcessChain2D::setPassEnabled(std::size_t index, bool enabled) noexcept
    {
        if (index >= m_passes.size()) return false;
        m_passes[index].enabled = enabled;
        return true;
    }

    bool ShaderPostProcessChain2D::movePass(std::size_t from, std::size_t to)
    {
        if (from >= m_passes.size() || to >= m_passes.size()) return false;
        if (from == to) return true;

        ShaderPostProcessPass2D pass = std::move(m_passes[from]);
        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(from));
        m_passes.insert(m_passes.begin() + static_cast<std::ptrdiff_t>(to), std::move(pass));
        return true;
    }

    bool ShaderPostProcessChain2D::removePass(std::size_t index)
    {
        if (index >= m_passes.size()) return false;
        m_passes.erase(m_passes.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    void ShaderPostProcessChain2D::clear()
    {
        m_passes.clear();
    }

    std::size_t ShaderPostProcessChain2D::passCount() const noexcept
    {
        return m_passes.size();
    }

    const ShaderPostProcessPass2D* ShaderPostProcessChain2D::pass(std::size_t index) const noexcept
    {
        return index < m_passes.size() ? &m_passes[index] : nullptr;
    }

    bool ShaderPostProcessChain2D::workspaceReady() const noexcept
    {
        return m_workspace[0] != nullptr && m_workspace[0]->ready();
    }

    sf::Vector2u ShaderPostProcessChain2D::workspaceSize() const noexcept
    {
        return workspaceReady() ? m_workspace[0]->size() : sf::Vector2u{};
    }

    void ShaderPostProcessChain2D::resetWorkspace()
    {
        for (auto& workspace : m_workspace)
            workspace.reset();
    }

    ShaderPostProcessResult2D ShaderPostProcessChain2D::apply(const RenderSurface2D& source,
                                                              sf::RenderTarget& destination)
    {
        if (!source.ready() || source.target() == nullptr || source.texture() == nullptr)
            return failureResult(ShaderPostProcessFailure2D::SourceUnavailable);

        if (source.contentGeneration() == 0u)
            return failureResult(ShaderPostProcessFailure2D::SourceUnpublished);

        if (destination.getSize().x == 0u || destination.getSize().y == 0u)
            return failureResult(ShaderPostProcessFailure2D::DestinationUnavailable);

        if (source.target() == &destination)
            return failureResult(ShaderPostProcessFailure2D::FeedbackLoop);

        std::vector<std::size_t> enabledPasses;
        enabledPasses.reserve(m_passes.size());

        for (std::size_t index = 0u; index < m_passes.size(); ++index)
        {
            const ShaderPostProcessPass2D& passConfig = m_passes[index];
            if (!passConfig.enabled) continue;

            if (!isValidPass(passConfig))
                return failureResult(ShaderPostProcessFailure2D::InvalidPass, index);

            const Shader2DHandle shader = passConfig.material->shader();
            if (!shader || !shader->loaded())
                return failureResult(ShaderPostProcessFailure2D::ShaderUnavailable, index);

            if (!passConfig.material->isComplete())
                return failureResult(ShaderPostProcessFailure2D::MaterialIncomplete, index);

            enabledPasses.push_back(index);
        }

        if (enabledPasses.empty())
        {
            ScopedDefaultView2D viewGuard(destination);
            if (!drawFullscreen(*source.texture(), destination, {}))
                return failureResult(ShaderPostProcessFailure2D::MaterialApplyFailed);
            return {};
        }

        const std::size_t requiredWorkspaceCount =
            enabledPasses.size() <= 1u ? 0u : std::min<std::size_t>(2u, enabledPasses.size() - 1u);
        const RenderSurfaceConfig2D workspaceConfig{source.size(), source.config().smooth, false};

        if (!ensureWorkspace(workspaceConfig, requiredWorkspaceCount))
            return failureResult(ShaderPostProcessFailure2D::WorkspaceAllocationFailed);

        const sf::Texture* currentTexture = source.texture();
        ShaderPostProcessResult2D result;

        for (std::size_t ordinal = 0u; ordinal < enabledPasses.size(); ++ordinal)
        {
            const std::size_t passIndex = enabledPasses[ordinal];
            const ShaderPostProcessPass2D& passConfig = m_passes[passIndex];
            const bool finalPass = ordinal + 1u == enabledPasses.size();

            if (finalPass)
            {
                ScopedDefaultView2D viewGuard(destination);
                if (passConfig.clearOutput) destination.clear(passConfig.clearColor);

                if (!drawFullscreen(*currentTexture, destination, passConfig.material))
                {
                    return failureResult(ShaderPostProcessFailure2D::MaterialApplyFailed, passIndex,
                                         result.completedPasses);
                }
            }
            else
            {
                RenderSurface2D& workspace = *m_workspace[ordinal % 2u];
                if (passConfig.clearOutput && !workspace.clear(passConfig.clearColor))
                {
                    return failureResult(ShaderPostProcessFailure2D::WorkspaceClearFailed,
                                         passIndex, result.completedPasses);
                }

                sf::RenderTarget* target = workspace.target();
                if (target == nullptr)
                {
                    return failureResult(ShaderPostProcessFailure2D::WorkspaceAllocationFailed,
                                         passIndex, result.completedPasses);
                }

                {
                    ScopedDefaultView2D viewGuard(*target);
                    if (!drawFullscreen(*currentTexture, *target, passConfig.material))
                    {
                        return failureResult(ShaderPostProcessFailure2D::MaterialApplyFailed,
                                             passIndex, result.completedPasses);
                    }
                }

                if (!workspace.display())
                {
                    return failureResult(ShaderPostProcessFailure2D::WorkspacePublishFailed,
                                         passIndex, result.completedPasses);
                }

                currentTexture = workspace.texture();
                if (currentTexture == nullptr)
                {
                    return failureResult(ShaderPostProcessFailure2D::WorkspacePublishFailed,
                                         passIndex, result.completedPasses);
                }
            }

            ++result.completedPasses;
        }

        return result;
    }

    bool ShaderPostProcessChain2D::isPassNameAvailable(
        std::string_view name, std::optional<std::size_t> skip) const noexcept
    {
        for (std::size_t index = 0u; index < m_passes.size(); ++index)
        {
            if (skip && *skip == index) continue;
            if (m_passes[index].name == name) return false;
        }

        return true;
    }

    bool ShaderPostProcessChain2D::ensureWorkspace(const RenderSurfaceConfig2D& config,
                                                   std::size_t requiredCount)
    {
        if (requiredCount == 0u) return true;
        if (requiredCount > m_workspace.size() || !RenderSurface2D::isValidConfig(config))
            return false;

        std::array<std::unique_ptr<RenderSurface2D>, 2u> replacements;

        for (std::size_t index = 0u; index < requiredCount; ++index)
        {
            if (m_workspace[index] && workspaceMatches(*m_workspace[index], config)) continue;

            auto candidate = std::make_unique<RenderSurface2D>();
            if (!candidate->create(config)) return false;
            replacements[index] = std::move(candidate);
        }

        for (std::size_t index = 0u; index < requiredCount; ++index)
        {
            if (replacements[index]) m_workspace[index] = std::move(replacements[index]);
        }

        return true;
    }

    bool ShaderPostProcessChain2D::workspaceMatches(const RenderSurface2D& surface,
                                                    const RenderSurfaceConfig2D& config) noexcept
    {
        if (!surface.ready()) return false;
        const RenderSurfaceConfig2D& existing = surface.config();
        return existing.size == config.size && existing.smooth == config.smooth &&
               existing.repeated == config.repeated;
    }

    bool ShaderPostProcessChain2D::drawFullscreen(const sf::Texture& texture,
                                                  sf::RenderTarget& destination,
                                                  const Material2DHandle& material)
    {
        const sf::Vector2u sourceSize = texture.getSize();
        const sf::Vector2u destinationSize = destination.getSize();
        if (sourceSize.x == 0u || sourceSize.y == 0u || destinationSize.x == 0u ||
            destinationSize.y == 0u)
        {
            return false;
        }

        sf::Sprite sprite(texture);
        sprite.setPosition({0.f, 0.f});
        sprite.setScale({static_cast<float>(destinationSize.x) / static_cast<float>(sourceSize.x),
                         static_cast<float>(destinationSize.y) / static_cast<float>(sourceSize.y)});

        sf::RenderStates states;
        states.blendMode = sf::BlendNone;
        if (material && !material->apply(states)) return false;

        destination.draw(sprite, states);
        return true;
    }
}

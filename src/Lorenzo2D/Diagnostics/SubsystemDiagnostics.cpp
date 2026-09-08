#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>

#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <cstdint>
#include <limits>
#include <memory>

namespace l2d
{
    namespace
    {
        std::uint64_t counterValue(std::size_t value) noexcept
        {
            constexpr std::uint64_t Maximum = std::numeric_limits<std::uint64_t>::max();

            if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
            {
                if (value > static_cast<std::size_t>(Maximum)) return Maximum;
            }

            return static_cast<std::uint64_t>(value);
        }

        void addCount(DiagnosticCounters& counters, DiagnosticCounter counter,
                      std::size_t amount) noexcept
        {
            counters.add(counter, counterValue(amount));
        }
    }

    void accumulateSceneDiagnostics(const Scene& scene, DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::ActiveEntities, scene.activeGameObjectCount());

        for (const std::unique_ptr<GameObject>& gameObject : scene.gameObjects())
        {
            if (gameObject == nullptr || gameObject->isDestroyQueued() || !gameObject->isActive())
                continue;

            addCount(counters, DiagnosticCounter::ActiveComponents,
                     gameObject->activeComponentCount());
        }
    }

    void accumulatePhysicsDiagnostics(const PhysicsWorld2D& world, DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::Colliders, world.broadPhaseStats().proxyCount);
    }

    void accumulateRenderQueueDiagnostics(const RenderQueue2D& queue, DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::RenderedItems, queue.size());
    }

    void accumulateTileMapRenderDiagnostics(const TileMapRenderStats& stats,
                                            DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::DrawCalls, stats.drawCallCount);
        addCount(counters, DiagnosticCounter::RenderedItems, stats.submittedTileCount);
    }

    void accumulateAssetDiagnostics(const AssetManager& assets, DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::LoadedAssets, assets.loadedAssetCount());
        addCount(counters, DiagnosticCounter::LiveAssets, assets.liveAssetSlotCount());
    }

    void accumulateAudioDiagnostics(const AudioSystem& audio, DiagnosticCounters& counters)
    {
        addCount(counters, DiagnosticCounter::ActiveAudioVoices, audio.activeVoiceCount());
    }

    void recordPhysicsQueries(DiagnosticCounters& counters, std::size_t count) noexcept
    {
        addCount(counters, DiagnosticCounter::PhysicsQueries, count);
    }

    void recordNavigationPathDiagnostics(const NavigationPath2D& path, DiagnosticCounters& counters,
                                         bool replan) noexcept
    {
        addCount(counters, DiagnosticCounter::NavigationExpansions, path.visitedNodes);
        if (replan) counters.increment(DiagnosticCounter::NavigationReplans);
    }

    void recordSaveWriteDiagnostics(DiagnosticCounters& counters, std::size_t bytes) noexcept
    {
        addCount(counters, DiagnosticCounter::SaveBytesWritten, bytes);
    }

    void recordSaveReadDiagnostics(DiagnosticCounters& counters, std::size_t bytes) noexcept
    {
        addCount(counters, DiagnosticCounter::SaveBytesRead, bytes);
    }
}

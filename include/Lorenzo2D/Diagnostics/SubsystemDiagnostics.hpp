#pragma once

#include <cstddef>
#include <string_view>

namespace l2d
{
    class AssetManager;
    class AudioSystem;
    class DiagnosticCounters;
    class PhysicsWorld2D;
    class RenderQueue2D;
    class Scene;
    struct NavigationPath2D;
    struct TileMapRenderStats;

    namespace diagnostic_scope
    {
        inline constexpr std::string_view FixedStep = "fixed-step";
        inline constexpr std::string_view Render = "render";
        inline constexpr std::string_view Physics = "physics";
        inline constexpr std::string_view Navigation = "navigation";
        inline constexpr std::string_view Asset = "asset";
        inline constexpr std::string_view Ui = "ui";
        inline constexpr std::string_view Audio = "audio";
        inline constexpr std::string_view Save = "save";
    }

    // Accumulation functions add telemetry into an existing counter set.
    // Reset the counters at the start of the reporting interval, then call each
    // relevant accumulator once per subsystem instance or render pass.
    void accumulateSceneDiagnostics(const Scene& scene, DiagnosticCounters& counters);
    void accumulatePhysicsDiagnostics(const PhysicsWorld2D& world, DiagnosticCounters& counters);
    void accumulateRenderQueueDiagnostics(const RenderQueue2D& queue, DiagnosticCounters& counters);
    void accumulateTileMapRenderDiagnostics(const TileMapRenderStats& stats,
                                             DiagnosticCounters& counters);
    void accumulateAssetDiagnostics(const AssetManager& assets, DiagnosticCounters& counters);
    void accumulateAudioDiagnostics(const AudioSystem& audio, DiagnosticCounters& counters);

    // Event-style telemetry for operations whose counts are not retained by
    // their owning subsystem. Add these as operations occur during the current
    // reporting interval.
    void recordPhysicsQueries(DiagnosticCounters& counters, std::size_t count = 1u) noexcept;
    void recordNavigationPathDiagnostics(const NavigationPath2D& path, DiagnosticCounters& counters,
                                         bool replan = false) noexcept;
    void recordSaveWriteDiagnostics(DiagnosticCounters& counters, std::size_t bytes) noexcept;
    void recordSaveReadDiagnostics(DiagnosticCounters& counters, std::size_t bytes) noexcept;
}

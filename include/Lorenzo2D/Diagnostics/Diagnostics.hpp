#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace l2d
{
    struct ProfilerConfig
    {
        std::size_t sampleWindow = 120u;
        std::size_t maxScopes = 256u;
        std::size_t maxNameBytes = 96u;
    };

    struct ProfileStatistics
    {
        std::string name;
        double currentMilliseconds = 0.0;
        double rollingAverageMilliseconds = 0.0;
        double maximumMilliseconds = 0.0;
        std::uint64_t sampleCount = 0u;
        std::vector<double> samplesMilliseconds;
    };

    class Profiler;

    class ProfileScope final
    {
      public:
        ProfileScope(Profiler& profiler, std::string name);
        ~ProfileScope() noexcept;

        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;
        ProfileScope(ProfileScope&&) = delete;
        ProfileScope& operator=(ProfileScope&&) = delete;

      private:
        Profiler* m_profiler = nullptr;
        std::string m_name;
        std::chrono::steady_clock::time_point m_startedAt{};
        bool m_active = false;
    };

    class Profiler
    {
      public:
        explicit Profiler(ProfilerConfig config = {});

        static bool isValidConfig(const ProfilerConfig& config) noexcept;

        void setEnabled(bool enabled) noexcept;
        bool enabled() const noexcept;

        const ProfilerConfig& config() const noexcept;
        bool beginFrame();
        bool endFrame();
        void cancelFrame() noexcept;
        bool frameOpen() const noexcept;
        std::uint64_t frameIndex() const noexcept;

        bool record(std::string_view name, double milliseconds);
        ProfileScope scope(std::string name);

        std::optional<ProfileStatistics> statistics(std::string_view name) const;
        std::vector<ProfileStatistics> allStatistics() const;
        void reset() noexcept;

      private:
        friend class ProfileScope;

        struct Entry
        {
            double currentMilliseconds = 0.0;
            double maximumMilliseconds = 0.0;
            double rollingSumMilliseconds = 0.0;
            std::uint64_t sampleCount = 0u;
            std::deque<double> samplesMilliseconds;
        };

        bool validName(std::string_view name) const noexcept;
        bool scopeKnown(std::string_view name) const;
        std::size_t pendingNewScopeCount() const;
        bool commit(std::string_view name, double milliseconds);
        ProfileStatistics makeStatistics(const std::string& name, const Entry& entry) const;

        ProfilerConfig m_config;
        std::map<std::string, Entry> m_entries;
        std::map<std::string, double> m_pendingFrame;
        std::uint64_t m_frameIndex = 0u;
        bool m_enabled = true;
        bool m_frameOpen = false;
    };

    enum class DiagnosticCounter : std::size_t
    {
        ActiveEntities = 0u,
        ActiveComponents,
        Colliders,
        PhysicsQueries,
        NavigationExpansions,
        NavigationReplans,
        DrawCalls,
        RenderedItems,
        LoadedAssets,
        LiveAssets,
        ActiveAudioVoices,
        SaveBytesWritten,
        SaveBytesRead,
        Count
    };

    struct DiagnosticCounterValue
    {
        DiagnosticCounter counter = DiagnosticCounter::ActiveEntities;
        std::string name;
        std::uint64_t value = 0u;
    };

    class DiagnosticCounters
    {
      public:
        static std::string_view name(DiagnosticCounter counter) noexcept;

        std::uint64_t value(DiagnosticCounter counter) const noexcept;
        void set(DiagnosticCounter counter, std::uint64_t value) noexcept;
        void add(DiagnosticCounter counter, std::uint64_t amount) noexcept;
        void increment(DiagnosticCounter counter) noexcept;
        void reset() noexcept;
        std::vector<DiagnosticCounterValue> values() const;

      private:
        static constexpr std::size_t CounterCount =
            static_cast<std::size_t>(DiagnosticCounter::Count);
        std::array<std::uint64_t, CounterCount> m_values{};
    };

    struct DiagnosticSnapshot
    {
        std::uint32_t formatVersion = 1u;
        std::string engineVersion;
        std::uint64_t frameIndex = 0u;
        std::vector<ProfileStatistics> timings;
        std::vector<DiagnosticCounterValue> counters;
    };

    DiagnosticSnapshot captureDiagnosticSnapshot(const Profiler& profiler,
                                                 const DiagnosticCounters& counters);

    class DiagnosticReport
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1u;

        static std::string toJson(const DiagnosticSnapshot& snapshot, bool pretty = true);
        static bool writeJson(std::ostream& output, const DiagnosticSnapshot& snapshot,
                              bool pretty = true);
    };
}

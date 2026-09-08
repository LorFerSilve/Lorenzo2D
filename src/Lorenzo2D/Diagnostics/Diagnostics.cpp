#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Core/Version.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace l2d
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        constexpr std::size_t MaxSampleWindow = 4096u;
        constexpr std::size_t MaxScopes = 4096u;
        constexpr std::size_t MaxNameBytes = 256u;

        bool validMilliseconds(double milliseconds) noexcept
        {
            return std::isfinite(milliseconds) && milliseconds >= 0.0;
        }

        std::size_t counterIndex(DiagnosticCounter counter) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(counter);
            const std::size_t count = static_cast<std::size_t>(DiagnosticCounter::Count);
            return index < count ? index : count;
        }
    }

    ProfileScope::ProfileScope(Profiler& profiler, std::string name)
        : m_profiler(&profiler), m_name(std::move(name))
    {
        m_active = profiler.enabled() && profiler.validName(m_name);
        if (m_active) m_startedAt = std::chrono::steady_clock::now();
    }

    ProfileScope::~ProfileScope() noexcept
    {
        if (!m_active || m_profiler == nullptr) return;

        try
        {
            const std::chrono::steady_clock::time_point finishedAt =
                std::chrono::steady_clock::now();
            const double milliseconds =
                std::chrono::duration<double, std::milli>(finishedAt - m_startedAt).count();
            (void)m_profiler->record(m_name, milliseconds);
        }
        catch (...)
        {
        }
    }

    Profiler::Profiler(ProfilerConfig config) : m_config(config)
    {
        if (!isValidConfig(config))
            throw std::invalid_argument("ProfilerConfig contains an invalid bound");
    }

    bool Profiler::isValidConfig(const ProfilerConfig& config) noexcept
    {
        return config.sampleWindow > 0u && config.sampleWindow <= MaxSampleWindow &&
               config.maxScopes > 0u && config.maxScopes <= MaxScopes && config.maxNameBytes > 0u &&
               config.maxNameBytes <= MaxNameBytes;
    }

    void Profiler::setEnabled(bool enabled) noexcept
    {
        m_enabled = enabled;
        if (!enabled) cancelFrame();
    }

    bool Profiler::enabled() const noexcept
    {
        return m_enabled;
    }

    const ProfilerConfig& Profiler::config() const noexcept
    {
        return m_config;
    }

    bool Profiler::beginFrame()
    {
        if (!m_enabled || m_frameOpen) return false;

        m_pendingFrame.clear();
        m_frameOpen = true;
        return true;
    }

    bool Profiler::endFrame()
    {
        if (!m_frameOpen) return false;

        for (const auto& [name, milliseconds] : m_pendingFrame)
            (void)commit(name, milliseconds);

        m_pendingFrame.clear();
        m_frameOpen = false;
        ++m_frameIndex;
        return true;
    }

    void Profiler::cancelFrame() noexcept
    {
        m_pendingFrame.clear();
        m_frameOpen = false;
    }

    bool Profiler::frameOpen() const noexcept
    {
        return m_frameOpen;
    }

    std::uint64_t Profiler::frameIndex() const noexcept
    {
        return m_frameIndex;
    }

    bool Profiler::record(std::string_view name, double milliseconds)
    {
        if (!m_enabled || !validName(name) || !validMilliseconds(milliseconds)) return false;

        if (!m_frameOpen) return commit(name, milliseconds);

        const std::string key(name);
        auto pending = m_pendingFrame.find(key);

        if (pending != m_pendingFrame.end())
        {
            const double combined = pending->second + milliseconds;
            if (!validMilliseconds(combined)) return false;

            pending->second = combined;
            return true;
        }

        if (!scopeKnown(name) && m_entries.size() + pendingNewScopeCount() >= m_config.maxScopes)
            return false;

        m_pendingFrame.emplace(key, milliseconds);
        return true;
    }

    ProfileScope Profiler::scope(std::string name)
    {
        return ProfileScope(*this, std::move(name));
    }

    std::optional<ProfileStatistics> Profiler::statistics(std::string_view name) const
    {
        const auto iterator = m_entries.find(std::string(name));
        if (iterator == m_entries.end()) return std::nullopt;

        return makeStatistics(iterator->first, iterator->second);
    }

    std::vector<ProfileStatistics> Profiler::allStatistics() const
    {
        std::vector<ProfileStatistics> result;
        result.reserve(m_entries.size());

        for (const auto& [name, entry] : m_entries)
            result.push_back(makeStatistics(name, entry));

        return result;
    }

    void Profiler::reset() noexcept
    {
        m_entries.clear();
        m_pendingFrame.clear();
        m_frameIndex = 0u;
        m_frameOpen = false;
    }

    bool Profiler::validName(std::string_view name) const noexcept
    {
        return !name.empty() && name.size() <= m_config.maxNameBytes;
    }

    bool Profiler::scopeKnown(std::string_view name) const
    {
        return m_entries.find(std::string(name)) != m_entries.end() ||
               m_pendingFrame.find(std::string(name)) != m_pendingFrame.end();
    }

    std::size_t Profiler::pendingNewScopeCount() const
    {
        std::size_t result = 0u;

        for (const auto& [name, milliseconds] : m_pendingFrame)
        {
            (void)milliseconds;
            if (m_entries.find(name) == m_entries.end()) ++result;
        }

        return result;
    }

    bool Profiler::commit(std::string_view name, double milliseconds)
    {
        if (!validName(name) || !validMilliseconds(milliseconds)) return false;

        const std::string key(name);
        auto iterator = m_entries.find(key);

        if (iterator == m_entries.end())
        {
            if (m_entries.size() >= m_config.maxScopes) return false;
            iterator = m_entries.emplace(key, Entry{}).first;
        }

        Entry& entry = iterator->second;
        entry.currentMilliseconds = milliseconds;
        entry.maximumMilliseconds = entry.sampleCount == 0u
                                        ? milliseconds
                                        : std::max(entry.maximumMilliseconds, milliseconds);
        ++entry.sampleCount;

        entry.samplesMilliseconds.push_back(milliseconds);
        entry.rollingSumMilliseconds += milliseconds;

        if (entry.samplesMilliseconds.size() > m_config.sampleWindow)
        {
            entry.rollingSumMilliseconds -= entry.samplesMilliseconds.front();
            entry.samplesMilliseconds.pop_front();
        }

        return true;
    }

    ProfileStatistics Profiler::makeStatistics(const std::string& name, const Entry& entry) const
    {
        ProfileStatistics result;
        result.name = name;
        result.currentMilliseconds = entry.currentMilliseconds;
        result.maximumMilliseconds = entry.maximumMilliseconds;
        result.sampleCount = entry.sampleCount;
        result.samplesMilliseconds.assign(entry.samplesMilliseconds.begin(),
                                          entry.samplesMilliseconds.end());

        if (!entry.samplesMilliseconds.empty())
        {
            result.rollingAverageMilliseconds =
                entry.rollingSumMilliseconds /
                static_cast<double>(entry.samplesMilliseconds.size());
        }

        return result;
    }

    std::string_view DiagnosticCounters::name(DiagnosticCounter counter) noexcept
    {
        switch (counter)
        {
        case DiagnosticCounter::ActiveEntities:
            return "active_entities";
        case DiagnosticCounter::ActiveComponents:
            return "active_components";
        case DiagnosticCounter::Colliders:
            return "colliders";
        case DiagnosticCounter::PhysicsQueries:
            return "physics_queries";
        case DiagnosticCounter::NavigationExpansions:
            return "navigation_expansions";
        case DiagnosticCounter::NavigationReplans:
            return "navigation_replans";
        case DiagnosticCounter::DrawCalls:
            return "draw_calls";
        case DiagnosticCounter::RenderedItems:
            return "rendered_items";
        case DiagnosticCounter::LoadedAssets:
            return "loaded_assets";
        case DiagnosticCounter::LiveAssets:
            return "live_assets";
        case DiagnosticCounter::ActiveAudioVoices:
            return "active_audio_voices";
        case DiagnosticCounter::SaveBytesWritten:
            return "save_bytes_written";
        case DiagnosticCounter::SaveBytesRead:
            return "save_bytes_read";
        case DiagnosticCounter::Count:
            return "invalid";
        }

        return "invalid";
    }

    std::uint64_t DiagnosticCounters::value(DiagnosticCounter counter) const noexcept
    {
        const std::size_t index = counterIndex(counter);
        return index < m_values.size() ? m_values[index] : 0u;
    }

    void DiagnosticCounters::set(DiagnosticCounter counter, std::uint64_t value) noexcept
    {
        const std::size_t index = counterIndex(counter);
        if (index < m_values.size()) m_values[index] = value;
    }

    void DiagnosticCounters::add(DiagnosticCounter counter, std::uint64_t amount) noexcept
    {
        const std::size_t index = counterIndex(counter);
        if (index >= m_values.size()) return;

        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const std::uint64_t current = m_values[index];
        m_values[index] = amount > maximum - current ? maximum : current + amount;
    }

    void DiagnosticCounters::increment(DiagnosticCounter counter) noexcept
    {
        add(counter, 1u);
    }

    void DiagnosticCounters::reset() noexcept
    {
        m_values.fill(0u);
    }

    std::vector<DiagnosticCounterValue> DiagnosticCounters::values() const
    {
        std::vector<DiagnosticCounterValue> result;
        result.reserve(m_values.size());

        for (std::size_t index = 0u; index < m_values.size(); ++index)
        {
            const auto counter = static_cast<DiagnosticCounter>(index);
            result.push_back({counter, std::string(name(counter)), m_values[index]});
        }

        return result;
    }

    DiagnosticSnapshot captureDiagnosticSnapshot(const Profiler& profiler,
                                                 const DiagnosticCounters& counters)
    {
        DiagnosticSnapshot snapshot;
        snapshot.formatVersion = DiagnosticReport::FormatVersion;
        snapshot.engineVersion = std::string(VersionString);
        snapshot.frameIndex = profiler.frameIndex();
        snapshot.timings = profiler.allStatistics();
        snapshot.counters = counters.values();
        return snapshot;
    }

    std::string DiagnosticReport::toJson(const DiagnosticSnapshot& snapshot, bool pretty)
    {
        Json root = Json::object();
        root["format_version"] = snapshot.formatVersion;
        root["engine_version"] = snapshot.engineVersion;
        root["frame_index"] = snapshot.frameIndex;

        Json timings = Json::array();
        for (const ProfileStatistics& timing : snapshot.timings)
        {
            Json encoded = Json::object();
            encoded["name"] = timing.name;
            encoded["current_ms"] = timing.currentMilliseconds;
            encoded["rolling_average_ms"] = timing.rollingAverageMilliseconds;
            encoded["maximum_ms"] = timing.maximumMilliseconds;
            encoded["sample_count"] = timing.sampleCount;
            encoded["samples_ms"] = timing.samplesMilliseconds;
            timings.push_back(std::move(encoded));
        }
        root["timings"] = std::move(timings);

        Json counters = Json::array();
        for (const DiagnosticCounterValue& counter : snapshot.counters)
        {
            Json encoded = Json::object();
            encoded["name"] = counter.name;
            encoded["value"] = counter.value;
            counters.push_back(std::move(encoded));
        }
        root["counters"] = std::move(counters);

        return pretty ? root.dump(2) + "\n" : root.dump() + "\n";
    }

    bool DiagnosticReport::writeJson(std::ostream& output, const DiagnosticSnapshot& snapshot,
                                     bool pretty)
    {
        if (!output) return false;

        const std::string serialized = toJson(snapshot, pretty);
        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        return static_cast<bool>(output);
    }
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace l2d
{
    class DeterministicHasher64
    {
      public:
        static constexpr std::uint64_t OffsetBasis = 14695981039346656037ull;
        static constexpr std::uint64_t Prime = 1099511628211ull;

        void reset() noexcept;
        void appendByte(std::uint8_t value) noexcept;
        void appendUInt64(std::uint64_t value) noexcept;
        void appendInt64(std::int64_t value) noexcept;
        void appendString(std::string_view value) noexcept;
        std::uint64_t value() const noexcept;

      private:
        std::uint64_t m_value = OffsetBasis;
    };

    struct ReplayTraceConfig
    {
        std::size_t maxTicks = 1'000'000u;
        std::size_t maxInputBytesPerTick = 4096u;
        std::size_t maxTotalInputBytes = 64u * 1024u * 1024u;
    };

    struct ReplayTick
    {
        std::uint64_t tick = 0u;
        std::vector<std::uint8_t> inputBytes;
        std::uint64_t stateHash = 0u;
    };

    class ReplayTrace
    {
      public:
        explicit ReplayTrace(ReplayTraceConfig config = {});

        static bool isValidConfig(const ReplayTraceConfig& config) noexcept;

        const ReplayTraceConfig& config() const noexcept;
        bool record(std::uint64_t tick, std::vector<std::uint8_t> inputBytes,
                    std::uint64_t stateHash);
        std::size_t size() const noexcept;
        std::size_t totalInputBytes() const noexcept;
        const std::vector<ReplayTick>& ticks() const noexcept;
        void clear() noexcept;

      private:
        ReplayTraceConfig m_config;
        std::vector<ReplayTick> m_ticks;
        std::size_t m_totalInputBytes = 0u;
    };

    enum class ReplayDivergence
    {
        None,
        Tick,
        Input,
        State,
        Length
    };

    struct ReplayComparison
    {
        ReplayDivergence divergence = ReplayDivergence::None;
        std::optional<std::uint64_t> firstDivergenceTick;
        std::size_t matchedTicks = 0u;

        bool equivalent() const noexcept;
    };

    ReplayComparison compareReplayTraces(const ReplayTrace& expected, const ReplayTrace& actual);
}

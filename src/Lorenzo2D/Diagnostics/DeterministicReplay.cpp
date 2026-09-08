#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace l2d
{
    namespace
    {
        constexpr std::size_t MaxReplayTicks = 1'000'000u;
        constexpr std::size_t MaxInputBytesPerTick = 64u * 1024u;
        constexpr std::size_t MaxTotalInputBytes = 256u * 1024u * 1024u;
    }

    void DeterministicHasher64::reset() noexcept
    {
        m_value = OffsetBasis;
    }

    void DeterministicHasher64::appendByte(std::uint8_t value) noexcept
    {
        m_value ^= static_cast<std::uint64_t>(value);
        m_value *= Prime;
    }

    void DeterministicHasher64::appendUInt64(std::uint64_t value) noexcept
    {
        for (unsigned int byte = 0u; byte < 8u; ++byte)
        {
            const unsigned int shift = byte * 8u;
            appendByte(static_cast<std::uint8_t>((value >> shift) & 0xffu));
        }
    }

    void DeterministicHasher64::appendInt64(std::int64_t value) noexcept
    {
        appendUInt64(static_cast<std::uint64_t>(value));
    }

    void DeterministicHasher64::appendString(std::string_view value) noexcept
    {
        appendUInt64(static_cast<std::uint64_t>(value.size()));
        for (const char character : value)
            appendByte(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
    }

    std::uint64_t DeterministicHasher64::value() const noexcept
    {
        return m_value;
    }

    ReplayTrace::ReplayTrace(ReplayTraceConfig config) : m_config(config)
    {
        if (!isValidConfig(config))
            throw std::invalid_argument("ReplayTraceConfig contains an invalid bound");
    }

    bool ReplayTrace::isValidConfig(const ReplayTraceConfig& config) noexcept
    {
        return config.maxTicks > 0u && config.maxTicks <= MaxReplayTicks &&
               config.maxInputBytesPerTick <= MaxInputBytesPerTick &&
               config.maxTotalInputBytes <= MaxTotalInputBytes;
    }

    const ReplayTraceConfig& ReplayTrace::config() const noexcept
    {
        return m_config;
    }

    bool ReplayTrace::record(std::uint64_t tick, std::vector<std::uint8_t> inputBytes,
                             std::uint64_t stateHash)
    {
        if (m_ticks.size() >= m_config.maxTicks) return false;
        if (!m_ticks.empty() && tick <= m_ticks.back().tick) return false;
        if (inputBytes.size() > m_config.maxInputBytesPerTick) return false;

        const std::size_t remainingInputBytes = m_config.maxTotalInputBytes - m_totalInputBytes;
        if (inputBytes.size() > remainingInputBytes) return false;

        const std::size_t inputSize = inputBytes.size();
        m_ticks.push_back({tick, std::move(inputBytes), stateHash});
        m_totalInputBytes += inputSize;
        return true;
    }

    std::size_t ReplayTrace::size() const noexcept
    {
        return m_ticks.size();
    }

    std::size_t ReplayTrace::totalInputBytes() const noexcept
    {
        return m_totalInputBytes;
    }

    const std::vector<ReplayTick>& ReplayTrace::ticks() const noexcept
    {
        return m_ticks;
    }

    void ReplayTrace::clear() noexcept
    {
        m_ticks.clear();
        m_totalInputBytes = 0u;
    }

    bool ReplayComparison::equivalent() const noexcept
    {
        return divergence == ReplayDivergence::None;
    }

    ReplayComparison compareReplayTraces(const ReplayTrace& expected, const ReplayTrace& actual)
    {
        const std::vector<ReplayTick>& expectedTicks = expected.ticks();
        const std::vector<ReplayTick>& actualTicks = actual.ticks();
        const std::size_t sharedSize = std::min(expectedTicks.size(), actualTicks.size());

        for (std::size_t index = 0u; index < sharedSize; ++index)
        {
            const ReplayTick& expectedTick = expectedTicks[index];
            const ReplayTick& actualTick = actualTicks[index];

            if (expectedTick.tick != actualTick.tick)
            {
                return {ReplayDivergence::Tick, std::min(expectedTick.tick, actualTick.tick),
                        index};
            }

            if (expectedTick.inputBytes != actualTick.inputBytes)
                return {ReplayDivergence::Input, expectedTick.tick, index};

            if (expectedTick.stateHash != actualTick.stateHash)
                return {ReplayDivergence::State, expectedTick.tick, index};
        }

        if (expectedTicks.size() != actualTicks.size())
        {
            const std::uint64_t divergenceTick = expectedTicks.size() > sharedSize
                                                     ? expectedTicks[sharedSize].tick
                                                     : actualTicks[sharedSize].tick;
            return {ReplayDivergence::Length, divergenceTick, sharedSize};
        }

        return {ReplayDivergence::None, std::nullopt, sharedSize};
    }
}

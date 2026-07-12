#include <Lorenzo2D/Core/FixedStepScheduler.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr double DEFAULT_FIXED_DELTA_TIME = 1.0 / 60.0;
        constexpr double DEFAULT_MAXIMUM_FRAME_DELTA_TIME = 0.1;
        constexpr std::uint32_t DEFAULT_MAXIMUM_TICKS_PER_FRAME = 8;
        constexpr double MAXIMUM_TICK_SNAP_TOLERANCE = 1e-9;

        FixedStepConfig sanitizeConfig(FixedStepConfig config)
        {
            const float callbackFixedDeltaTime =
                static_cast<float>(config.fixedDeltaTime);

            if (
                !std::isfinite(config.fixedDeltaTime) ||
                config.fixedDeltaTime <= 0.0 ||
                !std::isfinite(callbackFixedDeltaTime) ||
                callbackFixedDeltaTime <= 0.f
            )
            {
                config.fixedDeltaTime = DEFAULT_FIXED_DELTA_TIME;
            }

            if (
                !std::isfinite(config.maximumFrameDeltaTime) ||
                config.maximumFrameDeltaTime <= 0.0
            )
            {
                config.maximumFrameDeltaTime =
                    DEFAULT_MAXIMUM_FRAME_DELTA_TIME;
            }

            if (config.maximumTicksPerFrame == 0)
            {
                config.maximumTicksPerFrame =
                    DEFAULT_MAXIMUM_TICKS_PER_FRAME;
            }

            return config;
        }

        double sanitizeDeltaTime(double deltaTime)
        {
            if (!std::isfinite(deltaTime) || deltaTime < 0.0)
                return 0.0;

            return deltaTime;
        }

        double saturatingAdd(double left, double right)
        {
            if (right <= 0.0)
                return left;

            const double maximum = std::numeric_limits<double>::max();

            if (left >= maximum - right)
                return maximum;

            return left + right;
        }

        std::uint64_t saturatingAdd(
            std::uint64_t left,
            std::uint64_t right
        )
        {
            const std::uint64_t maximum =
                std::numeric_limits<std::uint64_t>::max();

            if (left >= maximum - right)
                return maximum;

            return left + right;
        }

        std::uint64_t toTickCount(double tickCount)
        {
            if (!std::isfinite(tickCount))
                return std::numeric_limits<std::uint64_t>::max();

            if (tickCount <= 0.0)
                return 0;

            const double maximum = static_cast<double>(
                std::numeric_limits<std::uint64_t>::max()
            );

            if (tickCount >= maximum)
                return std::numeric_limits<std::uint64_t>::max();

            return static_cast<std::uint64_t>(tickCount);
        }

        struct NearbyInteger
        {
            double value = 0.0;
            bool snapped = false;
        };

        NearbyInteger snapToNearbyInteger(
            double value,
            double remainderRatio
        )
        {
            if (!std::isfinite(value))
                return { value, false };

            // Repeated frame additions can leave an exact tick boundary a
            // handful of ULPs to either side of its mathematical value.
            const double nearestInteger = std::round(value);
            const double tolerance = std::min(
                8.0 * std::numeric_limits<double>::epsilon() *
                    std::max(1.0, std::fabs(value)),
                MAXIMUM_TICK_SNAP_TOLERANCE
            );
            const double distance = std::fabs(value - nearestInteger);
            const bool remainderIsAtBoundary =
                std::isfinite(remainderRatio) &&
                (
                    remainderRatio <= tolerance ||
                    1.0 - remainderRatio <= tolerance
                );

            if (
                distance <= tolerance &&
                (distance > 0.0 || remainderIsAtBoundary)
            )
            {
                return { nearestInteger, true };
            }

            return { value, false };
        }

        double interpolationAlpha(double accumulator, double fixedDeltaTime)
        {
            double alpha = accumulator / fixedDeltaTime;

            if (!std::isfinite(alpha) || alpha <= 0.0)
                return 0.0;

            if (alpha >= 1.0)
                return std::nextafter(1.0, 0.0);

            return alpha;
        }
    }

    FixedStepScheduler::FixedStepScheduler(const FixedStepConfig& config)
        : m_config(sanitizeConfig(config))
    {
    }

    FixedStepFrame FixedStepScheduler::advance(double rawDeltaTime)
    {
        FixedStepFrame frame;

        frame.rawDeltaTime = sanitizeDeltaTime(rawDeltaTime);
        frame.frameDeltaTime = std::min(
            frame.rawDeltaTime,
            m_config.maximumFrameDeltaTime
        );

        m_frameCount = saturatingAdd(m_frameCount, std::uint64_t{ 1 });
        m_realElapsedTime = saturatingAdd(
            m_realElapsedTime,
            frame.rawDeltaTime
        );

        m_accumulator = saturatingAdd(
            m_accumulator,
            frame.frameDeltaTime
        );

        double remainder = std::fmod(
            m_accumulator,
            m_config.fixedDeltaTime
        );

        if (!std::isfinite(remainder) || remainder < 0.0)
            remainder = 0.0;

        const NearbyInteger tickRatio = snapToNearbyInteger(
            m_accumulator / m_config.fixedDeltaTime,
            remainder / m_config.fixedDeltaTime
        );

        if (tickRatio.value >= 1.0)
        {
            const double availableTicks = std::floor(tickRatio.value);

            const double ticksToRun = std::min(
                availableTicks,
                static_cast<double>(m_config.maximumTicksPerFrame)
            );

            frame.ticksToRun = static_cast<std::uint32_t>(ticksToRun);

            const double droppedTicks = availableTicks - ticksToRun;
            frame.droppedTicks = toTickCount(droppedTicks);

            if (tickRatio.snapped)
                remainder = 0.0;

            const double wholeStepTime = m_accumulator - remainder;
            const double executedSimulationTime =
                ticksToRun * m_config.fixedDeltaTime;

            frame.droppedSimulationTime = std::max(
                0.0,
                wholeStepTime - executedSimulationTime
            );

            m_accumulator = remainder;

            m_tickCount = saturatingAdd(
                m_tickCount,
                static_cast<std::uint64_t>(frame.ticksToRun)
            );

            m_droppedTickCount = saturatingAdd(
                m_droppedTickCount,
                frame.droppedTicks
            );

            m_simulationTime = saturatingAdd(
                m_simulationTime,
                executedSimulationTime
            );

            m_droppedSimulationTime = saturatingAdd(
                m_droppedSimulationTime,
                frame.droppedSimulationTime
            );
        }

        frame.interpolationAlpha = l2d::interpolationAlpha(
            m_accumulator,
            m_config.fixedDeltaTime
        );

        return frame;
    }

    void FixedStepScheduler::reset()
    {
        m_accumulator = 0.0;

        m_frameCount = 0;
        m_tickCount = 0;

        m_droppedTickCount = 0;
        m_droppedSimulationTime = 0.0;

        m_realElapsedTime = 0.0;
        m_simulationTime = 0.0;
    }

    const FixedStepConfig& FixedStepScheduler::config() const
    {
        return m_config;
    }

    double FixedStepScheduler::accumulator() const
    {
        return m_accumulator;
    }

    std::uint64_t FixedStepScheduler::frameCount() const
    {
        return m_frameCount;
    }

    std::uint64_t FixedStepScheduler::tickCount() const
    {
        return m_tickCount;
    }

    std::uint64_t FixedStepScheduler::droppedTickCount() const
    {
        return m_droppedTickCount;
    }

    double FixedStepScheduler::droppedSimulationTime() const
    {
        return m_droppedSimulationTime;
    }

    double FixedStepScheduler::realElapsedTime() const
    {
        return m_realElapsedTime;
    }

    double FixedStepScheduler::simulationTime() const
    {
        return m_simulationTime;
    }
}

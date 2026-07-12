#pragma once

#include <cstdint>

namespace l2d
{
    struct FixedStepConfig
    {
        double fixedDeltaTime = 1.0 / 60.0;
        double maximumFrameDeltaTime = 0.1;
        std::uint32_t maximumTicksPerFrame = 8;
    };

    struct FixedStepFrame
    {
        double rawDeltaTime = 0.0;
        double frameDeltaTime = 0.0;

        std::uint32_t ticksToRun = 0;
        std::uint64_t droppedTicks = 0;
        double droppedSimulationTime = 0.0;

        double interpolationAlpha = 0.0;
    };

    class FixedStepScheduler
    {
    public:
        explicit FixedStepScheduler(
            const FixedStepConfig& config = FixedStepConfig{}
        );

        FixedStepFrame advance(double rawDeltaTime);
        void reset();

        const FixedStepConfig& config() const;
        double accumulator() const;

        std::uint64_t frameCount() const;

        // Counts the ticks returned through FixedStepFrame::ticksToRun. A
        // consumer that can abandon a frame must track completed ticks itself.
        std::uint64_t tickCount() const;

        std::uint64_t droppedTickCount() const;
        double droppedSimulationTime() const;

        double realElapsedTime() const;

        // Advances by the ticks scheduled through FixedStepFrame::ticksToRun.
        double simulationTime() const;

    private:
        FixedStepConfig m_config;

        double m_accumulator = 0.0;

        std::uint64_t m_frameCount = 0;
        std::uint64_t m_tickCount = 0;

        std::uint64_t m_droppedTickCount = 0;
        double m_droppedSimulationTime = 0.0;

        double m_realElapsedTime = 0.0;
        double m_simulationTime = 0.0;
    };
}

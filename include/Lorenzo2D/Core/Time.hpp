#pragma once

#include <cstdint>

namespace l2d
{
    class Application;

    class Time
    {
    public:
        static float rawDeltaTime();
        static float frameDeltaTime();
        static float deltaTime();

        static float fixedDeltaTime();
        static float interpolationAlpha();

        // Accumulated clamped frame time. The difference from
        // realElapsedTime() is wall time rejected by the frame clamp.
        static float elapsedTime();
        static double realElapsedTime();

        // Advances only for fixed ticks completed by Application.
        static double simulationTime();

        static float fps();

        static std::uint64_t frameCount();
        static std::uint64_t tickCount();
        static std::uint32_t ticksThisFrame();

        // Counts whole fixed ticks removed after frame clamping, either by the
        // catch-up cap or because application shutdown abandoned planned ticks.
        static std::uint64_t droppedTickCount();
        static double droppedSimulationTime();

        // Accumulated wall-clock time rejected by maximumFrameDeltaTime before
        // fixed ticks are selected. This is separate from dropped whole ticks.
        static double clampedFrameTime();

    private:
        static void reset(double fixedDeltaTime);

        static void beginFrame(
            double rawDeltaTime,
            double frameDeltaTime
        );

        static void completeFixedTick();

        static void endFrame(
            double interpolationAlpha,
            std::uint64_t droppedTicks,
            double droppedSimulationTime,
            double clampedFrameTime
        );

    private:
        static double s_rawDeltaTime;
        static double s_frameDeltaTime;
        static double s_fixedDeltaTime;
        static double s_interpolationAlpha;

        static double s_elapsedTime;
        static double s_realElapsedTime;
        static double s_simulationTime;

        static float s_fps;

        static double s_fpsTimer;
        static std::uint64_t s_fpsFrameCounter;

        static std::uint64_t s_totalFrameCount;
        static std::uint64_t s_totalTickCount;
        static std::uint32_t s_ticksThisFrame;

        static std::uint64_t s_droppedTickCount;
        static double s_droppedSimulationTime;
        static double s_clampedFrameTime;

        friend class Application;
    };
}

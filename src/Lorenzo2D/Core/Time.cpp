#include <Lorenzo2D/Core/Time.hpp>

#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr double DEFAULT_FIXED_DELTA_TIME = 1.0 / 60.0;

        double sanitizeNonNegative(double value)
        {
            if (!std::isfinite(value) || value < 0.0)
                return 0.0;

            return value;
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
    }

    double Time::s_rawDeltaTime = 0.0;
    double Time::s_frameDeltaTime = 0.0;
    double Time::s_fixedDeltaTime = DEFAULT_FIXED_DELTA_TIME;
    double Time::s_interpolationAlpha = 0.0;

    double Time::s_elapsedTime = 0.0;
    double Time::s_realElapsedTime = 0.0;
    double Time::s_simulationTime = 0.0;

    float Time::s_fps = 0.f;

    double Time::s_fpsTimer = 0.0;
    std::uint64_t Time::s_fpsFrameCounter = 0;

    std::uint64_t Time::s_totalFrameCount = 0;
    std::uint64_t Time::s_totalTickCount = 0;
    std::uint32_t Time::s_ticksThisFrame = 0;

    std::uint64_t Time::s_droppedTickCount = 0;
    double Time::s_droppedSimulationTime = 0.0;
    double Time::s_clampedFrameTime = 0.0;

    float Time::rawDeltaTime()
    {
        return static_cast<float>(s_rawDeltaTime);
    }

    float Time::frameDeltaTime()
    {
        return static_cast<float>(s_frameDeltaTime);
    }

    float Time::deltaTime()
    {
        return frameDeltaTime();
    }

    float Time::fixedDeltaTime()
    {
        return static_cast<float>(s_fixedDeltaTime);
    }

    float Time::interpolationAlpha()
    {
        const float alpha = static_cast<float>(s_interpolationAlpha);

        if (alpha >= 1.f)
            return std::nextafter(1.f, 0.f);

        return alpha;
    }

    float Time::elapsedTime()
    {
        return static_cast<float>(s_elapsedTime);
    }

    double Time::realElapsedTime()
    {
        return s_realElapsedTime;
    }

    double Time::simulationTime()
    {
        return s_simulationTime;
    }

    float Time::fps()
    {
        return s_fps;
    }

    std::uint64_t Time::frameCount()
    {
        return s_totalFrameCount;
    }

    std::uint64_t Time::tickCount()
    {
        return s_totalTickCount;
    }

    std::uint32_t Time::ticksThisFrame()
    {
        return s_ticksThisFrame;
    }

    std::uint64_t Time::droppedTickCount()
    {
        return s_droppedTickCount;
    }

    double Time::droppedSimulationTime()
    {
        return s_droppedSimulationTime;
    }

    double Time::clampedFrameTime()
    {
        return s_clampedFrameTime;
    }

    void Time::reset(double fixedDeltaTime)
    {
        float callbackFixedDeltaTime = static_cast<float>(fixedDeltaTime);

        if (
            !std::isfinite(fixedDeltaTime) ||
            fixedDeltaTime <= 0.0 ||
            !std::isfinite(callbackFixedDeltaTime) ||
            callbackFixedDeltaTime <= 0.f
        )
        {
            callbackFixedDeltaTime = static_cast<float>(
                DEFAULT_FIXED_DELTA_TIME
            );
        }

        s_rawDeltaTime = 0.0;
        s_frameDeltaTime = 0.0;
        s_fixedDeltaTime = static_cast<double>(callbackFixedDeltaTime);
        s_interpolationAlpha = 0.0;

        s_elapsedTime = 0.0;
        s_realElapsedTime = 0.0;
        s_simulationTime = 0.0;

        s_fps = 0.f;
        s_fpsTimer = 0.0;
        s_fpsFrameCounter = 0;

        s_totalFrameCount = 0;
        s_totalTickCount = 0;
        s_ticksThisFrame = 0;

        s_droppedTickCount = 0;
        s_droppedSimulationTime = 0.0;
        s_clampedFrameTime = 0.0;
    }

    void Time::beginFrame(double rawDeltaTime, double frameDeltaTime)
    {
        s_rawDeltaTime = sanitizeNonNegative(rawDeltaTime);
        s_frameDeltaTime = sanitizeNonNegative(frameDeltaTime);
        s_interpolationAlpha = 0.0;
        s_ticksThisFrame = 0;

        s_elapsedTime = saturatingAdd(
            s_elapsedTime,
            s_frameDeltaTime
        );

        s_realElapsedTime = saturatingAdd(
            s_realElapsedTime,
            s_rawDeltaTime
        );

        s_totalFrameCount = saturatingAdd(
            s_totalFrameCount,
            std::uint64_t{ 1 }
        );

        s_fpsTimer = saturatingAdd(s_fpsTimer, s_rawDeltaTime);
        s_fpsFrameCounter = saturatingAdd(
            s_fpsFrameCounter,
            std::uint64_t{ 1 }
        );

        if (s_fpsTimer >= 1.0)
        {
            s_fps = static_cast<float>(
                static_cast<double>(s_fpsFrameCounter) / s_fpsTimer
            );

            s_fpsTimer = 0.0;
            s_fpsFrameCounter = 0;
        }
    }

    void Time::completeFixedTick()
    {
        s_totalTickCount = saturatingAdd(
            s_totalTickCount,
            std::uint64_t{ 1 }
        );

        if (s_ticksThisFrame < std::numeric_limits<std::uint32_t>::max())
            ++s_ticksThisFrame;

        s_simulationTime = saturatingAdd(
            s_simulationTime,
            s_fixedDeltaTime
        );
    }

    void Time::endFrame(
        double interpolationAlpha,
        std::uint64_t droppedTicks,
        double droppedSimulationTime,
        double clampedFrameTime
    )
    {
        interpolationAlpha = sanitizeNonNegative(interpolationAlpha);

        if (interpolationAlpha >= 1.0)
        {
            interpolationAlpha = std::nextafter(1.0, 0.0);
        }

        s_interpolationAlpha = interpolationAlpha;

        s_droppedTickCount = saturatingAdd(
            s_droppedTickCount,
            droppedTicks
        );

        s_droppedSimulationTime = saturatingAdd(
            s_droppedSimulationTime,
            sanitizeNonNegative(droppedSimulationTime)
        );

        s_clampedFrameTime = saturatingAdd(
            s_clampedFrameTime,
            sanitizeNonNegative(clampedFrameTime)
        );
    }
}

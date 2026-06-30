#include <Lorenzo2D/Core/Time.hpp>

namespace l2d
{
    float Time::s_deltaTime = 0.f;
    float Time::s_elapsedTime = 0.f;
    float Time::s_fps = 0.f;

    float Time::s_fpsTimer = 0.f;
    std::uint32_t Time::s_fpsFrameCounter = 0;
    std::uint64_t Time::s_totalFrameCount = 0;

    float Time::deltaTime()
    {
        return s_deltaTime;
    }

    float Time::elapsedTime()
    {
        return s_elapsedTime;
    }

    float Time::fps()
    {
        return s_fps;
    }

    std::uint64_t Time::frameCount()
    {
        return s_totalFrameCount;
    }

    void Time::update(float deltaTime)
    {
        // Bescherming tegen extreem grote deltaTime als je venster even hangt.
        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

        s_deltaTime = deltaTime;
        s_elapsedTime += deltaTime;
        s_totalFrameCount++;

        s_fpsTimer += deltaTime;
        s_fpsFrameCounter++;

        if (s_fpsTimer >= 1.f)
        {
            s_fps = static_cast<float>(s_fpsFrameCounter) / s_fpsTimer;

            s_fpsTimer = 0.f;
            s_fpsFrameCounter = 0;
        }
    }
}
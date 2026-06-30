#pragma once

#include <cstdint>

namespace l2d
{
    class Application;

    class Time
    {
    public:
        static float deltaTime();
        static float elapsedTime();
        static float fps();

        static std::uint64_t frameCount();

    private:
        static void update(float deltaTime);

    private:
        static float s_deltaTime;
        static float s_elapsedTime;
        static float s_fps;

        static float s_fpsTimer;
        static std::uint32_t s_fpsFrameCounter;
        static std::uint64_t s_totalFrameCount;

        friend class Application;
    };
}
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Core/Version.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string_view>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Result
    {
        double milliseconds = 0.0;
        std::uint64_t checksum = 0u;
    };

    Result benchmarkDisabledScopes()
    {
        l2d::Profiler profiler;
        profiler.setEnabled(false);

        constexpr std::size_t Iterations = 500000u;
        const Clock::time_point started = Clock::now();

        for (std::size_t iteration = 0u; iteration < Iterations; ++iteration)
        {
            auto scope = profiler.scope("disabled");
            (void)scope;
        }

        const Clock::time_point finished = Clock::now();
        return {std::chrono::duration<double, std::milli>(finished - started).count(),
                profiler.frameIndex()};
    }

    Result benchmarkFrameAggregation()
    {
        l2d::ProfilerConfig config;
        config.sampleWindow = 240u;
        config.maxScopes = 16u;
        l2d::Profiler profiler(config);

        constexpr std::size_t Frames = 100000u;
        constexpr std::string_view Names[] = {"frame",  "fixed-step", "physics", "navigation",
                                              "assets", "ui",         "audio",   "render"};

        const Clock::time_point started = Clock::now();

        for (std::size_t frame = 0u; frame < Frames; ++frame)
        {
            if (!profiler.beginFrame()) return {};

            for (std::size_t index = 0u; index < std::size(Names); ++index)
            {
                const double milliseconds = static_cast<double>((frame + index) % 17u) * 0.05;
                if (!profiler.record(Names[index], milliseconds)) return {};
                if (!profiler.record(Names[index], 0.01)) return {};
            }

            if (!profiler.endFrame()) return {};
        }

        const Clock::time_point finished = Clock::now();

        std::uint64_t checksum = profiler.frameIndex();
        for (const l2d::ProfileStatistics& statistics : profiler.allStatistics())
            checksum += statistics.sampleCount;

        return {std::chrono::duration<double, std::milli>(finished - started).count(), checksum};
    }
}

int main()
{
    const Result disabled = benchmarkDisabledScopes();
    const Result aggregation = benchmarkFrameAggregation();

    std::cout << "Lorenzo2D " << l2d::VersionString << " diagnostics benchmark\n"
              << "disabled scopes: " << disabled.milliseconds
              << " ms checksum=" << disabled.checksum << '\n'
              << "frame aggregation: " << aggregation.milliseconds
              << " ms checksum=" << aggregation.checksum << '\n';

    return aggregation.checksum == 0u ? 1 : 0;
}

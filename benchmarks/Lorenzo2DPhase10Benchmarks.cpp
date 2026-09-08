#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Save/SaveGame.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Result
    {
        double milliseconds = 0.0;
        std::size_t checksum = 0u;
    };

    Result benchmarkUi()
    {
        l2d::UiCanvas2D canvas;
        for (std::size_t row = 0u; row < 8u; ++row)
        {
            for (std::size_t column = 0u; column < 16u; ++column)
            {
                l2d::UiButton2D button;
                button.id = "button-" + std::to_string(row * 16u + column);
                button.position = {static_cast<float>(column) * 52.f,
                                   static_cast<float>(row) * 38.f};
                button.size = {48.f, 34.f};
                (void)canvas.addButton(std::move(button));
            }
        }

        constexpr std::size_t iterations = 250000u;
        std::size_t checksum = 0u;
        const Clock::time_point start = Clock::now();

        for (std::size_t iteration = 0u; iteration < iterations; ++iteration)
        {
            l2d::PointerState pointer;
            pointer.screenPosition = {static_cast<int>((iteration * 37u) % 832u),
                                      static_cast<int>((iteration * 19u) % 304u)};
            pointer.down = iteration % 5u == 0u;
            pointer.pressed = pointer.down;
            pointer.released = iteration % 5u == 1u;
            canvas.update(pointer);
            checksum += canvas.hoveredButton().has_value() ? 1u : 0u;
        }

        const Clock::time_point finish = Clock::now();
        return {std::chrono::duration<double, std::milli>(finish - start).count(), checksum};
    }

    Result benchmarkSaveSerialization()
    {
        l2d::SaveDocument document("phase10.benchmark", 1u);
        for (std::size_t index = 0u; index < 512u; ++index)
        {
            (void)document.setInteger("integer-" + std::to_string(index),
                                      static_cast<std::int64_t>(index));
        }

        constexpr std::size_t iterations = 250u;
        std::size_t checksum = 0u;
        const Clock::time_point start = Clock::now();

        for (std::size_t iteration = 0u; iteration < iterations; ++iteration)
        {
            std::ostringstream output;
            if (!l2d::SaveGameSerializer::save(output, document)) continue;
            const std::string serialized = output.str();
            checksum += serialized.size();

            l2d::SaveDocument loaded;
            std::istringstream input(serialized);
            if (l2d::SaveGameSerializer::load(input, loaded)) checksum += loaded.size();
        }

        const Clock::time_point finish = Clock::now();
        return {std::chrono::duration<double, std::milli>(finish - start).count(), checksum};
    }
}

int main()
{
    const Result ui = benchmarkUi();
    const Result save = benchmarkSaveSerialization();

    std::cout << "Lorenzo2D " << l2d::VersionString << " Phase 10 diagnostics\n"
              << "UI interaction: " << ui.milliseconds << " ms checksum=" << ui.checksum << '\n'
              << "Save round trips: " << save.milliseconds << " ms checksum=" << save.checksum
              << '\n';

    return ui.checksum == 0u || save.checksum == 0u ? 1 : 0;
}

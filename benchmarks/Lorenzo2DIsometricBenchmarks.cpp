#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Tilemap/IsometricTileGrid2D.hpp>

#include <SFML/Graphics/View.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>

int main()
{
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t iterations = 2000u;
    constexpr std::size_t samplesPerIteration = 1024u;
    const l2d::IsometricTileGrid2D grid(
        512u, 512u, l2d::IsometricProjection2D({32.f, 32.f}, {64.f, 32.f}, {8192.f, 0.f}));
    sf::View view({0.f, 0.f}, {1920.f, 1080.f});
    view.setCenter(grid.projection().cellToRender({256, 256}));

    std::size_t checksum = 0u;
    const Clock::time_point start = Clock::now();
    for (std::size_t iteration = 0u; iteration < iterations; ++iteration)
    {
        for (std::size_t sample = 0u; sample < samplesPerIteration; ++sample)
        {
            const l2d::TileMapCell cell{(sample * 37u + iteration) % grid.width(),
                                        (sample * 83u + iteration) % grid.height()};
            const auto render = grid.renderPosition(cell);
            if (!render) continue;
            const auto picked = grid.pick(*render);
            if (picked) checksum += picked->column + picked->row;
        }

        const auto visible = grid.visibleRegionForView(view, 2u);
        if (visible) checksum += visible->columnCount + visible->rowCount;
    }
    const Clock::time_point finish = Clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finish - start;

    std::cout << "Lorenzo2D " << l2d::VersionString << " isometric diagnostics\n"
              << iterations << " iterations x " << samplesPerIteration
              << " projection/picking samples\n"
              << elapsed.count() << " ms checksum=" << checksum << '\n';
    return checksum == 0u ? 1 : 0;
}

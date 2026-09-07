#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Tilemap/IsometricTileGrid2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/Graphics/View.hpp>

#include <iostream>

int main()
{
    l2d::TileMapData data;
    if (!data.setDimensions(64u, 64u) || !data.setTileSize({32.f, 32.f})) return 1;
    data.setOrientation(l2d::TileMapOrientation::Isometric);

    const l2d::IsometricTileGrid2D grid(data, {64.f, 32.f}, {960.f, 64.f});
    const l2d::TileMapCell cell{10u, 7u};
    const auto renderPosition = grid.renderPosition(cell);
    if (!renderPosition || grid.pick(*renderPosition) != cell) return 2;

    sf::View view({0.f, 0.f}, {960.f, 540.f});
    view.setCenter(*renderPosition);
    const auto streamRegion = grid.visibleRegionForView(view, 2u);
    if (!streamRegion) return 3;

    const l2d::RenderContext2D context{1.f, &grid.projection(), l2d::RenderPass2D::World};
    std::cout << "isometric cell " << cell.column << ',' << cell.row << " depth "
              << context.depthFor(*grid.worldPosition(cell)) << '\n';
    return 0;
}

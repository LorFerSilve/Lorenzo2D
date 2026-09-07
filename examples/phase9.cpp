#include <Lorenzo2D/Renderer/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Tilemap/IsometricTileGrid2D.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/Graphics/View.hpp>

#include <iostream>

int main()
{
    l2d::TileMapData data;
    if (!data.setDimensions(32u, 24u) || !data.setTileSize({32.f, 32.f})) return 1;
    data.setOrientation(l2d::TileMapOrientation::Isometric);

    const l2d::IsometricTileGrid2D grid(data, {64.f, 32.f}, {640.f, 64.f});
    const l2d::TileMapCell selected{7u, 5u};
    const auto renderPosition = grid.renderPosition(selected);
    if (!renderPosition || grid.pick(*renderPosition) != selected) return 2;

    sf::View view({0.f, 0.f}, {640.f, 360.f});
    view.setCenter(*renderPosition);
    const auto visibleRegion = grid.visibleRegionForView(view, 2u);
    if (!visibleRegion) return 3;

    const l2d::RenderContext2D context{
        1.f, &grid.projection(), l2d::RenderPass2D::World};
    const sf::Vector2f world = context.renderToWorld(*renderPosition);

    std::cout << "Picked cell " << selected.column << ',' << selected.row << " at world "
              << world.x << ',' << world.y << "\nVisible region: "
              << visibleRegion->firstColumn << ',' << visibleRegion->firstRow << " + "
              << visibleRegion->columnCount << 'x' << visibleRegion->rowCount << '\n';
    return 0;
}

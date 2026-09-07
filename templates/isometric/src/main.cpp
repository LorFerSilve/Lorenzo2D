#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Isometric/IsometricPlacementGrid2D.hpp>
#include <Lorenzo2D/Isometric/IsometricProjection2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <optional>
#include <utility>

class IsometricGame final : public l2d::Application
{
  public:
    IsometricGame()
        : l2d::Application(800, 450, "Lorenzo2D isometric game")
    {
        l2d::TileMapData data;
        (void)data.setDimensions(12u, 10u);
        (void)data.setTileSize({32.f, 32.f});
        data.setOrientation(l2d::TileMapOrientation::Isometric);

        l2d::TileDefinition groundDefinition;
        groundDefinition.id = 1u;
        (void)data.setDefinition(groundDefinition);

        l2d::TileMapLayer ground;
        ground.name = "Ground";
        ground.tiles.assign(data.cellCount(), 1u);
        (void)data.addLayer(std::move(ground));

        m_tileMap.setRenderChunkSize({4u, 4u});
        m_tileMap.setSolidTileColor(sf::Color(58, 83, 104));
        (void)m_tileMap.loadFromData(m_scene, data);

        l2d::IsometricProjectionConfig2D projectionConfig;
        projectionConfig.worldCellSize = data.tileSize();
        projectionConfig.renderTileSize = {64.f, 32.f};
        projectionConfig.renderOrigin = {400.f, 48.f};
        (void)m_projection.setConfig(projectionConfig);
        (void)m_placement.resetFromTileMap(data);
    }

  private:
    void onFrameStart(float) override
    {
        if (!l2d::Pointer::primary().pressed) return;

        const sf::Vector2f renderPosition = l2d::Pointer::worldPosition(getWindow());
        const std::optional<sf::Vector2u> cell =
            m_placement.pickCell(renderPosition, m_projection);
        if (!cell || !m_placement.place(*cell)) return;

        const std::optional<sf::Vector2f> center = m_placement.cellWorldCenter(*cell);
        if (!center) return;

        l2d::GameObject& marker = m_scene.createGameObject("Placement");
        marker.transform.setPosition(*center);
        marker.addComponent<l2d::CircleRenderer>(10.f, sf::Color(245, 184, 65));
        l2d::RenderOrder2D& order =
            marker.addComponent<l2d::RenderOrder2D>(l2d::RenderDepthMode2D::ProjectedY);
        order.setLayer(10);
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        const l2d::RenderContext2D context{
            interpolationAlpha, &m_projection, l2d::RenderPass2D::World};
        m_scene.render(window, context);
    }

    l2d::Scene m_scene{"Isometric game"};
    l2d::TileMap m_tileMap;
    l2d::IsometricProjection2D m_projection;
    l2d::IsometricPlacementGrid2D m_placement;
};

int main()
{
    IsometricGame application;
    application.run();
    return 0;
}

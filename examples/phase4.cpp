#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/ParticleEmitter2D.hpp>
#include <Lorenzo2D/Renderer/PostProcessStack2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>

#include <string>
#include <utility>

class Phase4Example final : public l2d::Application
{
  public:
    Phase4Example() : l2d::Application(800, 450, "Lorenzo2D phase 4 showcase")
    {
        l2d::GameObject& background = m_scene.createGameObject("Background");
        background.setZOrder(-100);
        background.addComponent<l2d::RectangleRenderer>(sf::Vector2f{800.f, 450.f},
                                                        sf::Color(16, 22, 38));

        l2d::AsciiTileMapImporter importer;
        importer.mapCharacter('#', 1u);
        importer.mapCharacter('G', 2u);
        importer.mapCharacter('W', 3u);

        l2d::TileMapData data;
        importer.import({"#########################", "#GGGGGGGGGGGGGGGGGGGGGGG#",
                         "#GGWWWWGGGGGGGGGGGGGGGGG#", "#GGGGGGGGGGGGGGGGGGGGGGG#",
                         "#GGGGGGGGG######GGGGGGGG#", "#GGGGGGGGGGGGGGGGGGGGGGG#",
                         "#GGGGGGGGGGGGGGGGGGGGGGG#", "#########################"},
                        {32.f, 32.f}, data);

        l2d::TileDefinition wall = *data.definition(1u);
        wall.collision = l2d::TileCollisionKind::Solid;
        wall.navigable = false;
        data.setDefinition(wall);
        l2d::TileDefinition ground = *data.definition(2u);
        ground.properties["terrain"] = std::string("grass");
        data.setDefinition(ground);
        l2d::TileDefinition water = *data.definition(3u);
        water.movementCost = 2.5f;
        water.properties["terrain"] = std::string("water");
        data.setDefinition(water);

        l2d::TileMapLayer triggers;
        triggers.name = "Triggers";
        triggers.role = l2d::TileMapLayerRole::Trigger;
        triggers.tiles.resize(data.cellCount(), l2d::EmptyTile);
        triggers.tiles[5u * data.width() + 12u] = 2u;
        triggers.properties["event"] = std::string("bridge");
        data.addLayer(std::move(triggers));

        m_tiles.setRenderChunkSize({4u, 4u});
        m_tiles.setSolidTileColor(sf::Color(70, 85, 115));
        m_tiles.loadFromData(m_scene, data);
        m_tiles.setStreamRegion({0u, 0u, 25u, 8u});

        l2d::GameObject& hero = m_scene.createGameObject("Hero");
        hero.setZOrder(10);
        hero.transform.setPosition({370.f, 230.f});
        hero.addComponent<l2d::RectangleRenderer>(sf::Vector2f{60.f, 70.f},
                                                  sf::Color(250, 185, 55));

        l2d::ParticleEmitterConfig2D particles;
        particles.emissionRate = 35.f;
        particles.minimumLifetime = 0.4f;
        particles.maximumLifetime = 0.9f;
        particles.minimumSpeed = 35.f;
        particles.maximumSpeed = 90.f;
        particles.directionDegrees = -90.f;
        particles.spreadDegrees = 55.f;
        particles.gravity = {0.f, 65.f};
        particles.startSize = 7.f;
        particles.endSize = 1.f;
        particles.startColor = sf::Color(255, 230, 100);
        particles.endColor = sf::Color(255, 80, 20, 0);
        l2d::GameObject& emitter = m_scene.createGameObject("Particles");
        emitter.setZOrder(20);
        emitter.transform.setPosition({400.f, 230.f});
        emitter.addComponent<l2d::ParticleEmitter2D>(particles);

        m_effects.addPass({sf::Color(25, 45, 90, 24), l2d::PostProcessBlend2D::Alpha, true});
    }

  private:
    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
        m_effects.apply(window);
    }

    l2d::Scene m_scene{"Phase 4"};
    l2d::TileMap m_tiles;
    l2d::PostProcessStack2D m_effects;
};

int main()
{
    Phase4Example application;
    application.run();
    return 0;
}

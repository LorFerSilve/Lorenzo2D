#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/ParticleEmitter2D.hpp>
#include <Lorenzo2D/Renderer/PostProcessStack2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <vector>

class Phase4Example final : public l2d::Application
{
  public:
    Phase4Example() : l2d::Application(800, 450, "Lorenzo2D phase 4 showcase")
    {
        l2d::GameObject& background = m_scene.createGameObject("Background");
        background.setZOrder(-100);
        background.addComponent<l2d::RectangleRenderer>(sf::Vector2f{800.f, 450.f},
                                                        sf::Color(16, 22, 38));

        sf::Image atlas({32u, 16u}, sf::Color(40, 130, 220));

        for (unsigned int y = 0; y < 16u; ++y)
        {
            for (unsigned int x = 16u; x < 32u; ++x)
            {
                atlas.setPixel({x, y}, sf::Color(60, 210, 235));
            }
        }

        l2d::TextureHandle atlasTexture(std::make_shared<sf::Texture>(atlas));
        l2d::TileSet tileSet;
        tileSet.setTexture(atlasTexture);
        tileSet.setAnimatedTile('W',
                                std::vector<l2d::TileAnimationFrame>{{{{0, 0}, {16, 16}}, 0.25f},
                                                                     {{{16, 0}, {16, 16}}, 0.25f}});
        m_tiles.setTileSize({32.f, 32.f});
        m_tiles.setRenderChunkSize({4u, 4u});
        m_tiles.setSolidTileColor(sf::Color(70, 85, 115));
        m_tiles.setTileSet(tileSet);
        m_tiles.loadFromLayout(m_scene, {"#########################", "#.......................#",
                                         "#..WWWW.................#", "#.......................#",
                                         "#.........######........#", "#.......................#",
                                         "#.......................#", "#########################"});
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
        m_editTimer += fixedDeltaTime;

        if (m_editTimer >= 1.f)
        {
            m_editTimer -= 1.f;
            m_bridgeVisible = !m_bridgeVisible;
            m_tiles.setTile(5u, 12u, m_bridgeVisible ? '#' : '.');
        }

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
    float m_editTimer = 0.f;
    bool m_bridgeVisible = false;
};

int main()
{
    Phase4Example application;
    application.run();
    return 0;
}

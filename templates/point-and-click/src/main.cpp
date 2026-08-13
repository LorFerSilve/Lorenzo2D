#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Navigation/PathFollower2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <optional>

class PointAndClickGame final : public l2d::Application
{
  public:
    PointAndClickGame() : l2d::Application(800, 448, "Lorenzo2D point-and-click game")
    {
        l2d::NavigationGridConfig2D config;
        config.size = {25u, 14u};
        config.cellSize = {32.f, 32.f};
        config.connectivity = l2d::NavigationConnectivity2D::EightWay;
        (void)m_grid.reset(config);

        l2d::GameObject& background = m_scene.createGameObject("Background");
        background.setZOrder(-10);
        background.addComponent<l2d::RectangleRenderer>(sf::Vector2f{800.f, 448.f},
                                                        sf::Color(21, 29, 46));
        addObstacle({0.f, 0.f}, {800.f, 24.f});
        addObstacle({0.f, 424.f}, {800.f, 24.f});
        addObstacle({0.f, 0.f}, {24.f, 448.f});
        addObstacle({776.f, 0.f}, {24.f, 448.f});
        addObstacle({352.f, 96.f}, {96.f, 256.f});

        l2d::GameObject& player = m_scene.createGameObject("Player");
        player.transform.setPosition({80.f, 80.f});
        player.addComponent<l2d::RectangleRenderer>(sf::Vector2f{24.f, 24.f},
                                                    sf::Color(245, 184, 65));
        l2d::BoxCollider2D& collider =
            player.addComponent<l2d::BoxCollider2D>(sf::Vector2f{24.f, 24.f});
        collider.setOffset({12.f, 12.f});
        player.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        player.addComponent<l2d::TopDownController2D>();
        l2d::PathFollowerConfig2D followerConfig;
        followerConfig.agentRadius = 12.f;
        followerConfig.positionOffset = {12.f, 12.f};
        m_follower = &player.addComponent<l2d::PathFollower2D>(followerConfig);
        l2d::PhysicsQueryFilter2D bakeFilter;
        bakeFilter.ignoredObject = player.id();
        (void)m_grid.bakeObstacles(l2d::PhysicsQueryContext2D(m_scene), 12.f, bakeFilter);
    }

  private:
    void addObstacle(sf::Vector2f position, sf::Vector2f size)
    {
        l2d::GameObject& obstacle = m_scene.createGameObject("Obstacle");
        obstacle.transform.setPosition(position);
        obstacle.addComponent<l2d::RectangleRenderer>(size, sf::Color(66, 82, 111));
        l2d::BoxCollider2D& collider = obstacle.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
    }

    void onFrameStart(float) override
    {
        if (l2d::Pointer::primary().pressed)
            m_pendingDestination = l2d::Pointer::worldPosition(getWindow());
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
        if (m_pendingDestination)
        {
            (void)m_follower->setDestination(m_grid, m_pathfinder, *m_pendingDestination);
            m_pendingDestination.reset();
        }
        (void)m_follower->follow(l2d::PhysicsQueryContext2D(m_scene), fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

    l2d::Scene m_scene{"Point-and-click game"};
    l2d::NavigationGrid2D m_grid;
    l2d::AStarPathfinder2D m_pathfinder;
    l2d::PathFollower2D* m_follower = nullptr;
    std::optional<sf::Vector2f> m_pendingDestination;
};

int main()
{
    PointAndClickGame game;
    game.run();
    return 0;
}

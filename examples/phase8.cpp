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

class Phase8Example final : public l2d::Application
{
  public:
    Phase8Example()
        : l2d::Application(960, 544, "Lorenzo2D phase 8 | Click a walkable cell to navigate")
    {
        l2d::NavigationGridConfig2D gridConfig;
        gridConfig.size = {30u, 17u};
        gridConfig.cellSize = {32.f, 32.f};
        gridConfig.connectivity = l2d::NavigationConnectivity2D::EightWay;
        (void)m_grid.reset(gridConfig);

        l2d::GameObject& background = m_scene.createGameObject("Background");
        background.setZOrder(-10);
        background.addComponent<l2d::RectangleRenderer>(sf::Vector2f{960.f, 544.f},
                                                        sf::Color(21, 29, 46));
        createObstacle({0.f, 0.f}, {960.f, 24.f});
        createObstacle({0.f, 520.f}, {960.f, 24.f});
        createObstacle({0.f, 0.f}, {24.f, 544.f});
        createObstacle({936.f, 0.f}, {24.f, 544.f});
        createObstacle({320.f, 96.f}, {96.f, 288.f});
        createObstacle({608.f, 256.f}, {160.f, 64.f});

        l2d::GameObject& player = m_scene.createGameObject("Navigation agent");
        player.transform.setPosition({80.f, 80.f});
        player.setZOrder(10);
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
    void createObstacle(sf::Vector2f position, sf::Vector2f size)
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

    l2d::Scene m_scene{"Phase 8 navigation"};
    l2d::NavigationGrid2D m_grid;
    l2d::AStarPathfinder2D m_pathfinder;
    l2d::PathFollower2D* m_follower = nullptr;
    std::optional<sf::Vector2f> m_pendingDestination;
};

int main()
{
    Phase8Example application;
    application.run();
    return 0;
}

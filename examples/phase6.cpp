#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

class Phase6Example final : public l2d::Application
{
  public:
    Phase6Example()
        : l2d::Application(960, 544,
                           "Lorenzo2D phase 6 | WASD: free movement | arrows: grid steps"),
          m_actions(l2d::Input::snapshot())
    {
        m_actions.bindAxis2D("free-move", l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::D),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::W),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::S));
        m_actions.bindAxis2D("grid-move", l2d::Input::physicalKey(sf::Keyboard::Scancode::Left),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::Right),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::Up),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::Down));

        for (int x = 32; x < 960; x += 32)
            createDecoration({static_cast<float>(x), 0.f}, {1.f, 544.f}, sf::Color(37, 48, 67));
        for (int y = 32; y < 544; y += 32)
            createDecoration({0.f, static_cast<float>(y)}, {960.f, 1.f}, sf::Color(37, 48, 67));

        createObstacle({0.f, 0.f}, {960.f, 24.f});
        createObstacle({0.f, 520.f}, {960.f, 24.f});
        createObstacle({0.f, 0.f}, {24.f, 544.f});
        createObstacle({936.f, 0.f}, {24.f, 544.f});
        createObstacle({224.f, 96.f}, {32.f, 256.f});
        createObstacle({544.f, 192.f}, {256.f, 32.f});
        createObstacle({704.f, 320.f}, {32.f, 160.f});

        l2d::GameObject& freeCharacter = createCharacter({80.f, 80.f}, sf::Color(245, 184, 65));
        m_topDown = &freeCharacter.addComponent<l2d::TopDownController2D>();

        l2d::GameObject& gridCharacter = createCharacter({400.f, 64.f}, sf::Color(80, 195, 245));
        l2d::GridStepControllerConfig2D gridConfig;
        gridConfig.cellSize = {32.f, 32.f};
        gridConfig.gridOrigin = {16.f, 0.f};
        gridConfig.stepDuration = 0.14f;
        m_grid = &gridCharacter.addComponent<l2d::GridStepController2D>(gridConfig);
        (void)m_grid->synchronizeToGrid();
    }

  private:
    void createDecoration(sf::Vector2f position, sf::Vector2f size, sf::Color color)
    {
        l2d::GameObject& object = m_scene.createGameObject("Grid line");
        object.transform.setPosition(position);
        object.addComponent<l2d::RectangleRenderer>(size, color);
        object.setZOrder(-10);
    }

    void createObstacle(sf::Vector2f position, sf::Vector2f size)
    {
        l2d::GameObject& object = m_scene.createGameObject("Obstacle");
        object.transform.setPosition(position);
        object.addComponent<l2d::RectangleRenderer>(size, sf::Color(76, 92, 118));
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
    }

    l2d::GameObject& createCharacter(sf::Vector2f position, sf::Color color)
    {
        l2d::GameObject& object = m_scene.createGameObject("Character");
        object.transform.setPosition(position);
        const sf::Vector2f size{24.f, 24.f};
        object.addComponent<l2d::RectangleRenderer>(size, color);
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        object.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        object.setZOrder(10);
        return object;
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
        const l2d::PhysicsQueryContext2D queries(m_scene);
        (void)m_topDown->move(queries, m_actions.axis2D("free-move"), fixedDeltaTime);
        (void)m_grid->move(queries, m_actions.axis2D("grid-move"), fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

    l2d::Scene m_scene{"Phase 6 top-down controllers"};
    l2d::InputMap m_actions;
    l2d::TopDownController2D* m_topDown = nullptr;
    l2d::GridStepController2D* m_grid = nullptr;
};

int main()
{
    Phase6Example application;
    application.run();
    return 0;
}

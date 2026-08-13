#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

class TopDownGame final : public l2d::Application
{
  public:
    TopDownGame()
        : l2d::Application(800, 450, "Lorenzo2D top-down game"), m_actions(l2d::Input::snapshot())
    {
        m_actions.bindAxis2D("move", l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::D),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::W),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::S));
        addWall({0.f, 0.f}, {800.f, 24.f});
        addWall({0.f, 426.f}, {800.f, 24.f});
        addWall({0.f, 0.f}, {24.f, 450.f});
        addWall({776.f, 0.f}, {24.f, 450.f});
        addWall({350.f, 110.f}, {100.f, 230.f});

        l2d::GameObject& player = m_scene.createGameObject("Player");
        player.transform.setPosition({100.f, 200.f});
        player.addComponent<l2d::RectangleRenderer>(sf::Vector2f{28.f, 28.f},
                                                    sf::Color(245, 184, 65));
        l2d::BoxCollider2D& collider =
            player.addComponent<l2d::BoxCollider2D>(sf::Vector2f{28.f, 28.f});
        collider.setOffset({14.f, 14.f});
        player.addComponent<l2d::CharacterMotor2D>(l2d::topDownCharacterMotorConfig2D());
        m_controller = &player.addComponent<l2d::TopDownController2D>();
    }

  private:
    void addWall(sf::Vector2f position, sf::Vector2f size)
    {
        l2d::GameObject& wall = m_scene.createGameObject("Wall");
        wall.transform.setPosition(position);
        wall.addComponent<l2d::RectangleRenderer>(size, sf::Color(70, 85, 110));
        l2d::BoxCollider2D& collider = wall.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
        (void)m_controller->move(l2d::PhysicsQueryContext2D(m_scene), m_actions.axis2D("move"),
                                 fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

    l2d::Scene m_scene{"Top-down game"};
    l2d::InputMap m_actions;
    l2d::TopDownController2D* m_controller = nullptr;
};

int main()
{
    TopDownGame game;
    game.run();
    return 0;
}

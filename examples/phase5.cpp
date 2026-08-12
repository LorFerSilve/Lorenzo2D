#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

class Phase5Example final : public l2d::Application
{
  public:
    Phase5Example()
        : l2d::Application(800, 450, "Lorenzo2D phase 5 character motor"),
          m_actions(l2d::Input::snapshot())
    {
        m_actions.bindAxis2D("move", l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::D),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::W),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::S));

        createObstacle("Ground", {0.f, 410.f}, {800.f, 40.f}, sf::Color(65, 80, 105));
        createObstacle("Left wall", {0.f, 0.f}, {30.f, 450.f}, sf::Color(65, 80, 105));
        createObstacle("Right wall", {770.f, 0.f}, {30.f, 450.f}, sf::Color(65, 80, 105));

        l2d::GameObject& slope =
            createObstacle("Slope", {500.f, 330.f}, {190.f, 25.f}, sf::Color(90, 120, 155));
        slope.transform.setRotation(-18.f);

        m_platform = &createObstacle("Moving platform", {170.f, 300.f}, {170.f, 20.f},
                                     sf::Color(105, 165, 205));

        m_character = &m_scene.createGameObject("Character");
        m_character->transform.setPosition({370.f, 160.f});
        m_character->addComponent<l2d::RectangleRenderer>(sf::Vector2f{30.f, 46.f},
                                                          sf::Color(245, 185, 65));
        l2d::BoxCollider2D& collider =
            m_character->addComponent<l2d::BoxCollider2D>(sf::Vector2f{30.f, 46.f});
        collider.setOffset({15.f, 23.f});
        m_motor = &m_character->addComponent<l2d::CharacterMotor2D>();
    }

  private:
    l2d::GameObject& createObstacle(const char* name, sf::Vector2f position, sf::Vector2f size,
                                    sf::Color color)
    {
        l2d::GameObject& object = m_scene.createGameObject(name);
        object.transform.setPosition(position);
        object.addComponent<l2d::RectangleRenderer>(size, color);
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        return object;
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);

        constexpr float platformSpeed = 70.f;
        constexpr float leftLimit = 100.f;
        constexpr float rightLimit = 360.f;
        m_platform->transform.move({m_platformDirection * platformSpeed * fixedDeltaTime, 0.f});
        if (m_platform->transform.position().x <= leftLimit)
            m_platformDirection = 1.f;
        else if (m_platform->transform.position().x >= rightLimit)
            m_platformDirection = -1.f;

        const sf::Vector2f input = m_actions.axis2D("move");
        constexpr float moveSpeed = 230.f;
        m_motor->move(l2d::PhysicsQueryContext2D(m_scene), input * moveSpeed * fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

    l2d::Scene m_scene{"Phase 5"};
    l2d::InputMap m_actions;
    l2d::GameObject* m_character = nullptr;
    l2d::GameObject* m_platform = nullptr;
    l2d::CharacterMotor2D* m_motor = nullptr;
    float m_platformDirection = 1.f;
};

int main()
{
    Phase5Example application;
    application.run();
    return 0;
}

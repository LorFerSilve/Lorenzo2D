#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

class Phase7Example final : public l2d::Application
{
  public:
    Phase7Example()
        : l2d::Application(960, 544, "Lorenzo2D phase 7 | A/D: run | Space: jump | S: drop"),
          m_actions(l2d::Input::snapshot())
    {
        m_actions.bindAxis1D("run", l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::D));
        m_actions.bindButton("jump", l2d::Input::physicalKey(sf::Keyboard::Scancode::Space));
        m_actions.bindButton("drop", l2d::Input::physicalKey(sf::Keyboard::Scancode::S));

        createSolid("Ground", {0.f, 500.f}, {960.f, 44.f}, sf::Color(61, 76, 102));
        createSolid("Left wall", {0.f, 0.f}, {24.f, 544.f}, sf::Color(61, 76, 102));
        createSolid("Right wall", {936.f, 0.f}, {24.f, 544.f}, sf::Color(61, 76, 102));
        createSolid("Step 1", {230.f, 492.f}, {48.f, 8.f}, sf::Color(86, 108, 142));
        createSolid("Step 2", {278.f, 484.f}, {48.f, 16.f}, sf::Color(86, 108, 142));

        l2d::GameObject& slope =
            createSolid("Walkable slope", {680.f, 465.f}, {210.f, 24.f}, sf::Color(97, 126, 162));
        slope.transform.setRotation(-14.f);

        createOneWay("One-way platform", {370.f, 385.f}, {160.f, 14.f});
        m_platform =
            &createSolid("Moving platform", {555.f, 330.f}, {150.f, 18.f}, sf::Color(82, 171, 190));

        l2d::GameObject& player = m_scene.createGameObject("Player");
        player.transform.setPosition({96.f, 430.f});
        const sf::Vector2f size{28.f, 42.f};
        player.addComponent<l2d::RectangleRenderer>(size, sf::Color(245, 184, 65));
        l2d::BoxCollider2D& collider = player.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        player.addComponent<l2d::CharacterMotor2D>(
            l2d::platformerCharacterMotorConfig2D(OneWayCategory));
        m_controller = &player.addComponent<l2d::PlatformerController2D>();
        player.setZOrder(10);
    }

  private:
    static constexpr std::uint32_t OneWayCategory = 1u << 1u;

    l2d::GameObject& createSolid(const char* name, sf::Vector2f position, sf::Vector2f size,
                                 sf::Color color)
    {
        l2d::GameObject& object = m_scene.createGameObject(name);
        object.transform.setPosition(position);
        object.addComponent<l2d::RectangleRenderer>(size, color);
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        return object;
    }

    void createOneWay(const char* name, sf::Vector2f position, sf::Vector2f size)
    {
        l2d::GameObject& object = createSolid(name, position, size, sf::Color(169, 105, 184));
        l2d::BoxCollider2D* collider = object.getComponent<l2d::BoxCollider2D>();
        l2d::CollisionFilter2D filter = collider->filter();
        filter.categoryBits = OneWayCategory;
        collider->setFilter(filter);
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);

        constexpr float platformSpeed = 70.f;
        m_platform->transform.move({m_platformDirection * platformSpeed * fixedDeltaTime, 0.f});
        if (m_platform->transform.position().x <= 520.f)
            m_platformDirection = 1.f;
        else if (m_platform->transform.position().x >= 700.f)
            m_platformDirection = -1.f;

        l2d::PlatformerInput2D input;
        input.horizontal = m_actions.axis1D("run");
        input.jumpPressed = m_actions.consumePressed("jump");
        input.jumpReleased = m_actions.consumeReleased("jump");
        input.dropDown = m_actions.down("drop");
        (void)m_controller->move(l2d::PhysicsQueryContext2D(m_scene), input, fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

    l2d::Scene m_scene{"Phase 7 platformer"};
    l2d::InputMap m_actions;
    l2d::PlatformerController2D* m_controller = nullptr;
    l2d::GameObject* m_platform = nullptr;
    float m_platformDirection = 1.f;
};

int main()
{
    Phase7Example application;
    application.run();
    return 0;
}

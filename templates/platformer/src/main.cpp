#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

class PlatformerGame final : public l2d::Application
{
  public:
    PlatformerGame()
        : l2d::Application(800, 450, "Lorenzo2D platformer"),
          m_actions(l2d::Input::snapshot())
    {
        m_actions.bindAxis1D("run", l2d::Input::physicalKey(sf::Keyboard::Scancode::A),
                             l2d::Input::physicalKey(sf::Keyboard::Scancode::D));
        m_actions.bindButton("jump", l2d::Input::physicalKey(sf::Keyboard::Scancode::Space));
        m_actions.bindButton("drop", l2d::Input::physicalKey(sf::Keyboard::Scancode::S));

        addPlatform({0.f, 420.f}, {800.f, 30.f}, 1u, sf::Color(68, 82, 108));
        addPlatform({310.f, 330.f}, {170.f, 14.f}, OneWayCategory,
                    sf::Color(165, 103, 184));
        m_movingPlatform = &addPlatform({520.f, 360.f}, {130.f, 16.f}, 1u,
                                        sf::Color(80, 168, 190));
        addPlatform({220.f, 412.f}, {42.f, 8.f}, 1u, sf::Color(92, 112, 143));

        l2d::GameObject& player = m_scene.createGameObject("Player");
        player.transform.setPosition({90.f, 370.f});
        const sf::Vector2f size{26.f, 40.f};
        player.addComponent<l2d::RectangleRenderer>(size, sf::Color(245, 184, 65));
        l2d::BoxCollider2D& collider = player.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        player.addComponent<l2d::CharacterMotor2D>(
            l2d::platformerCharacterMotorConfig2D(OneWayCategory));
        m_controller = &player.addComponent<l2d::PlatformerController2D>();
    }

  private:
    static constexpr std::uint32_t OneWayCategory = 1u << 1u;

    l2d::GameObject& addPlatform(sf::Vector2f position, sf::Vector2f size,
                                 std::uint32_t category, sf::Color color)
    {
        l2d::GameObject& platform = m_scene.createGameObject("Platform");
        platform.transform.setPosition(position);
        platform.addComponent<l2d::RectangleRenderer>(size, color);
        l2d::BoxCollider2D& collider = platform.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
        l2d::CollisionFilter2D filter = collider.filter();
        filter.categoryBits = category;
        collider.setFilter(filter);
        return platform;
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
        m_movingPlatform->transform.move({m_platformDirection * 60.f * fixedDeltaTime, 0.f});
        if (m_movingPlatform->transform.position().x < 470.f)
            m_platformDirection = 1.f;
        else if (m_movingPlatform->transform.position().x > 640.f)
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

    l2d::Scene m_scene{"Platformer starter"};
    l2d::InputMap m_actions;
    l2d::PlatformerController2D* m_controller = nullptr;
    l2d::GameObject* m_movingPlatform = nullptr;
    float m_platformDirection = 1.f;
};

int main()
{
    PlatformerGame game;
    game.run();
    return 0;
}

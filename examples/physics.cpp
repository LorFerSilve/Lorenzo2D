#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>
#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Color.hpp>

class PhysicsExample final : public l2d::Application
{
  public:
    PhysicsExample()
        : l2d::Application(640, 360, "Lorenzo2D physics example"), m_world(makePhysicsConfig())
    {
        createStaticBox("Ground", {0.f, 320.f}, {640.f, 40.f}, sf::Color(65, 75, 95));
        createStaticBox("Thin CCD wall", {500.f, 180.f}, {8.f, 140.f}, sf::Color(220, 100, 100));

        l2d::GameObject& compound = m_scene.createGameObject("Compound body");
        compound.transform.setPosition({100.f, 60.f});
        compound.addComponent<l2d::RectangleRenderer>(sf::Vector2f{80.f, 40.f},
                                                      sf::Color(85, 175, 245));
        l2d::RigidBody2D& compoundBody = compound.addComponent<l2d::RigidBody2D>();
        compoundBody.setUseGravity(true);
        l2d::CircleCollider2D& left = compound.addComponent<l2d::CircleCollider2D>(20.f);
        left.setOffset({20.f, 20.f});
        l2d::CircleCollider2D& right = compound.addComponent<l2d::CircleCollider2D>(20.f);
        right.setOffset({60.f, 20.f});

        l2d::GameObject& anchor = m_scene.createGameObject("Joint anchor");
        anchor.transform.setPosition({390.f, 40.f});

        l2d::GameObject& pendulum = m_scene.createGameObject("Joint pendulum");
        pendulum.transform.setPosition({360.f, 120.f});
        pendulum.addComponent<l2d::RectangleRenderer>(sf::Vector2f{60.f, 20.f},
                                                      sf::Color(245, 190, 80));
        l2d::RigidBody2D& pendulumBody = pendulum.addComponent<l2d::RigidBody2D>();
        pendulumBody.setUseGravity(true);
        pendulumBody.setFixedRotation(false);
        pendulumBody.setInertia(300.f);
        l2d::BoxCollider2D& pendulumCollider =
            pendulum.addComponent<l2d::BoxCollider2D>(sf::Vector2f{60.f, 20.f});
        pendulumCollider.setOffset({30.f, 10.f});
        l2d::DistanceJoint2D& joint =
            pendulum.addComponent<l2d::DistanceJoint2D>(anchor.id(), 100.f);
        joint.setLocalAnchor({30.f, 10.f});
        joint.setDamping(0.15f);

        l2d::GameObject& bullet = m_scene.createGameObject("CCD bullet");
        bullet.transform.setPosition({10.f, 250.f});
        bullet.addComponent<l2d::CircleRenderer>(8.f, sf::Color(120, 245, 145));
        l2d::RigidBody2D& bulletBody = bullet.addComponent<l2d::RigidBody2D>();
        bulletBody.setVelocity({1500.f, 0.f});
        bulletBody.setAllowsSleep(false);
        l2d::CircleCollider2D& bulletCollider = bullet.addComponent<l2d::CircleCollider2D>(8.f);
        bulletCollider.setOffset({8.f, 8.f});

        m_debugRenderer.setEnabled(true);
    }

  private:
    static l2d::PhysicsWorld2DConfig makePhysicsConfig()
    {
        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 500.f};
        config.velocityIterations = 12;
        config.positionIterations = 6;
        config.continuousCollisionDetection = true;
        config.warmStarting = true;
        config.sleeping = true;
        return config;
    }

    void createStaticBox(const char* name, sf::Vector2f position, sf::Vector2f size,
                         sf::Color color)
    {
        l2d::GameObject& object = m_scene.createGameObject(name);
        object.transform.setPosition(position);
        object.addComponent<l2d::RectangleRenderer>(size, color);
        l2d::BoxCollider2D& collider = object.addComponent<l2d::BoxCollider2D>(size);
        collider.setOffset(size * 0.5f);
    }

    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
    }

    void onFixedSimulation(float fixedDeltaTime) override
    {
        m_world.step(m_scene, fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
        m_debugRenderer.render(m_scene, window, interpolationAlpha);
    }

  private:
    l2d::Scene m_scene{"Physics"};
    l2d::PhysicsWorld2D m_world;
    l2d::PhysicsDebugRenderer2D m_debugRenderer;
};

int main()
{
    PhysicsExample app;
    app.run();
    return 0;
}

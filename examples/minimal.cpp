#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Color.hpp>

class MinimalExample final : public l2d::Application
{
  public:
    MinimalExample() : l2d::Application(640, 360, "Lorenzo2D minimal example")
    {
        l2d::GameObject& object = m_scene.createGameObject("Hello rectangle");
        object.transform.setPosition({220.f, 130.f});
        object.addComponent<l2d::RectangleRenderer>(sf::Vector2f{200.f, 100.f},
                                                    sf::Color(70, 150, 255));
    }

  private:
    void onFixedPreSimulation(float fixedDeltaTime) override
    {
        m_scene.fixedUpdate(fixedDeltaTime);
    }

    void onRender(sf::RenderWindow& window, float interpolationAlpha) override
    {
        m_scene.render(window, interpolationAlpha);
    }

  private:
    l2d::Scene m_scene{"Minimal"};
};

int main()
{
    MinimalExample app;
    app.run();
    return 0;
}

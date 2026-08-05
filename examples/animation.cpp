#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Animation/Animator.hpp>
#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    l2d::TextureHandle makeAnimationStrip()
    {
        constexpr unsigned int frameWidth = 32u;
        constexpr unsigned int frameHeight = 32u;
        constexpr unsigned int frameCount = 3u;
        constexpr unsigned int textureWidth = frameWidth * frameCount;

        std::vector<std::uint8_t> pixels(textureWidth * frameHeight * 4u, 255u);
        const sf::Color colors[] = {sf::Color(255, 90, 90), sf::Color(90, 220, 130),
                                    sf::Color(90, 150, 255)};

        for (unsigned int y = 0; y < frameHeight; ++y)
        {
            for (unsigned int x = 0; x < textureWidth; ++x)
            {
                const sf::Color color = colors[x / frameWidth];
                const std::size_t offset = (static_cast<std::size_t>(y) * textureWidth + x) * 4u;
                pixels[offset] = color.r;
                pixels[offset + 1u] = color.g;
                pixels[offset + 2u] = color.b;
                pixels[offset + 3u] = color.a;
            }
        }

        std::shared_ptr<sf::Texture> texture =
            std::make_shared<sf::Texture>(sf::Vector2u{textureWidth, frameHeight});
        texture->update(pixels.data());
        return l2d::TextureHandle(std::move(texture));
    }
}

class AnimationExample final : public l2d::Application
{
  public:
    AnimationExample() : l2d::Application(640, 360, "Lorenzo2D animation example")
    {
        l2d::GameObject& object = m_scene.createGameObject("Animated sprite");
        object.transform.setPosition({224.f, 116.f});

        l2d::SpriteRenderer& renderer =
            object.addComponent<l2d::SpriteRenderer>(makeAnimationStrip());
        renderer.setTextureRect({{0, 0}, {32, 32}});
        renderer.setSize({128.f, 128.f});

        l2d::AnimationClip pulse("pulse");
        pulse.addGridFrames({0, 0}, {32, 32}, 3u, 0.2f);

        l2d::Animator& animator = object.addComponent<l2d::Animator>();
        animator.addClip(std::move(pulse));
        animator.play("pulse");
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
    l2d::Scene m_scene{"Animation"};
};

int main()
{
    AnimationExample app;
    app.run();
    return 0;
}

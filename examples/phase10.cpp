#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>
#include <Lorenzo2D/Save/SaveGame.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/SoundChannel.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    l2d::SoundBufferHandle makeClickSound()
    {
        std::vector<std::int16_t> samples(2205u);
        for (std::size_t index = 0u; index < samples.size(); ++index)
        {
            const std::int16_t amplitude = index % 32u < 16u ? std::int16_t{1800}
                                                             : std::int16_t{-1800};
            samples[index] = amplitude;
        }

        auto buffer = std::make_shared<sf::SoundBuffer>();
        const std::vector<sf::SoundChannel> channels{sf::SoundChannel::Mono};
        if (!buffer->loadFromSamples(samples.data(), samples.size(), 1u, 44100u, channels))
            return {};

        return l2d::SoundBufferHandle(std::move(buffer));
    }

    class Phase10Example final : public l2d::Application
    {
      public:
        Phase10Example()
            : l2d::Application(640u, 360u, "Lorenzo2D Phase 10"),
              m_save("phase10.example", 1u), m_click(makeClickSound())
        {
            l2d::UiButton2D button;
            button.id = "save";
            button.label = "Save";
            button.position = {240.f, 150.f};
            button.size = {160.f, 60.f};
            (void)m_ui.addButton(std::move(button));

            (void)m_save.setInteger("activation_count", 0);
        }

      private:
        void onFrameStart(float) override
        {
            m_ui.update(l2d::Pointer::primary());

            if (m_ui.wasActivated("save"))
            {
                ++m_activationCount;
                (void)m_save.setInteger("activation_count", m_activationCount);
                (void)l2d::SaveGameSerializer::saveToFile("phase10-example-save.json", m_save);

                l2d::AudioPlayOptions2D options;
                options.bus = l2d::AudioBus2D::Ui;
                options.volume = 35.f;
                (void)m_audio.playSound(m_click, options);
            }
        }

        void onUpdate(float) override
        {
            m_audio.update();
        }

        void onRender(sf::RenderWindow& window, float) override
        {
            window.clear(sf::Color(24, 28, 36));
            m_ui.render(window);
        }

        l2d::UiCanvas2D m_ui;
        l2d::AudioSystem m_audio;
        l2d::SaveDocument m_save;
        l2d::SoundBufferHandle m_click;
        std::int64_t m_activationCount = 0;
    };
}

int main()
{
    Phase10Example example;
    example.run();
    return 0;
}

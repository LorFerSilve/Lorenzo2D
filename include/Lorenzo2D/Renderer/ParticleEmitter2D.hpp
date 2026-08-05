#pragma once

#include <Lorenzo2D/ECS/Component.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    struct ParticleEmitterConfig2D
    {
        float emissionRate = 10.f;
        float minimumLifetime = 0.5f;
        float maximumLifetime = 1.f;
        float minimumSpeed = 20.f;
        float maximumSpeed = 60.f;
        float directionDegrees = -90.f;
        float spreadDegrees = 45.f;
        sf::Vector2f gravity = {0.f, 0.f};
        float startSize = 6.f;
        float endSize = 0.f;
        sf::Color startColor = sf::Color::White;
        sf::Color endColor = sf::Color::Transparent;
        std::size_t maxParticles = 1000u;
        std::uint32_t seed = 0xC0FFEEu;
    };

    struct ParticleState2D
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        float age = 0.f;
        float lifetime = 1.f;
        float size = 0.f;
        sf::Color color = sf::Color::White;
    };

    class ParticleEmitter2D : public Component
    {
      public:
        static constexpr std::size_t MaximumParticleCount = 100000u;

        ParticleEmitter2D();
        explicit ParticleEmitter2D(ParticleEmitterConfig2D config);

        void setConfig(ParticleEmitterConfig2D config);
        const ParticleEmitterConfig2D& config() const;

        void play();
        void stop();
        bool isPlaying() const;

        std::size_t emit(std::size_t count);
        void clear();
        std::size_t aliveCount() const;
        const std::vector<ParticleState2D>& particles() const;

        void onUpdate(float deltaTime) override;
        void onRender(sf::RenderWindow& window) override;
        void onRender(sf::RenderWindow& window, float interpolationAlpha) override;

      private:
        static ParticleEmitterConfig2D sanitize(ParticleEmitterConfig2D config);
        void updateParticlePresentation(ParticleState2D& particle) const;

        ParticleEmitterConfig2D m_config;
        std::mt19937 m_random;
        std::vector<ParticleState2D> m_particles;
        float m_emissionAccumulator = 0.f;
        bool m_playing = true;
    };
}

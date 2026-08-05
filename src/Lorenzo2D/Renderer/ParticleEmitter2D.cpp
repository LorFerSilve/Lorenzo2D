#include <Lorenzo2D/Renderer/ParticleEmitter2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        float nonNegative(float value)
        {
            return std::isfinite(value) && value > 0.f ? value : 0.f;
        }

        float finiteOrZero(float value)
        {
            return std::isfinite(value) ? value : 0.f;
        }

        std::uint8_t interpolateChannel(std::uint8_t start, std::uint8_t end, float alpha)
        {
            const float value = static_cast<float>(start) +
                                (static_cast<float>(end) - static_cast<float>(start)) * alpha;
            return static_cast<std::uint8_t>(std::clamp(value, 0.f, 255.f));
        }

    }

    ParticleEmitter2D::ParticleEmitter2D() : ParticleEmitter2D(ParticleEmitterConfig2D{}) {}

    ParticleEmitter2D::ParticleEmitter2D(ParticleEmitterConfig2D config)
        : m_config(sanitize(config)), m_random(m_config.seed)
    {
    }

    void ParticleEmitter2D::setConfig(ParticleEmitterConfig2D config)
    {
        const std::uint32_t previousSeed = m_config.seed;
        m_config = sanitize(config);

        if (m_config.seed != previousSeed) m_random.seed(m_config.seed);

        if (m_particles.size() > m_config.maxParticles)
        {
            m_particles.erase(m_particles.begin() +
                                  static_cast<std::ptrdiff_t>(m_config.maxParticles),
                              m_particles.end());
        }
    }

    const ParticleEmitterConfig2D& ParticleEmitter2D::config() const
    {
        return m_config;
    }

    void ParticleEmitter2D::play()
    {
        m_playing = true;
    }

    void ParticleEmitter2D::stop()
    {
        m_playing = false;
    }

    bool ParticleEmitter2D::isPlaying() const
    {
        return m_playing;
    }

    std::size_t ParticleEmitter2D::emit(std::size_t count)
    {
        GameObject* gameObject = owner();
        const sf::Vector2f origin =
            gameObject == nullptr ? sf::Vector2f{} : gameObject->transform.position();
        const std::size_t available = m_config.maxParticles - m_particles.size();
        const std::size_t emissionCount = std::min(count, available);
        std::uniform_real_distribution<float> lifetime(m_config.minimumLifetime,
                                                       m_config.maximumLifetime);
        std::uniform_real_distribution<float> speed(m_config.minimumSpeed, m_config.maximumSpeed);
        std::uniform_real_distribution<float> spread(-m_config.spreadDegrees * 0.5f,
                                                     m_config.spreadDegrees * 0.5f);

        for (std::size_t index = 0; index < emissionCount; ++index)
        {
            const float angle = (m_config.directionDegrees + spread(m_random)) * PI / 180.f;
            const float magnitude = speed(m_random);
            ParticleState2D particle;
            particle.position = origin;
            particle.velocity = {std::cos(angle) * magnitude, std::sin(angle) * magnitude};
            particle.lifetime = lifetime(m_random);
            updateParticlePresentation(particle);
            m_particles.push_back(particle);
        }

        return emissionCount;
    }

    void ParticleEmitter2D::clear()
    {
        m_particles.clear();
        m_emissionAccumulator = 0.f;
    }

    std::size_t ParticleEmitter2D::aliveCount() const
    {
        return m_particles.size();
    }

    const std::vector<ParticleState2D>& ParticleEmitter2D::particles() const
    {
        return m_particles;
    }

    void ParticleEmitter2D::onUpdate(float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.f) return;

        for (ParticleState2D& particle : m_particles)
        {
            particle.age += deltaTime;
            particle.velocity += m_config.gravity * deltaTime;
            particle.position += particle.velocity * deltaTime;
            updateParticlePresentation(particle);
        }

        m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                         [](const ParticleState2D& particle)
                                         { return particle.age >= particle.lifetime; }),
                          m_particles.end());

        if (!m_playing || m_config.emissionRate <= 0.f) return;

        m_emissionAccumulator += m_config.emissionRate * deltaTime;
        const float limited =
            std::min(m_emissionAccumulator, static_cast<float>(m_config.maxParticles));
        const std::size_t count = static_cast<std::size_t>(std::floor(limited));
        m_emissionAccumulator = limited - static_cast<float>(count);
        emit(count);
    }

    void ParticleEmitter2D::onRender(sf::RenderWindow& window)
    {
        onRender(window, 1.f);
    }

    void ParticleEmitter2D::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        (void)interpolationAlpha;
        sf::VertexArray vertices(sf::PrimitiveType::Triangles);
        vertices.resize(m_particles.size() * 6u);
        std::size_t vertex = 0;

        for (const ParticleState2D& particle : m_particles)
        {
            const float half = particle.size * 0.5f;
            const sf::Vector2f topLeft = particle.position - sf::Vector2f{half, half};
            const sf::Vector2f topRight = particle.position + sf::Vector2f{half, -half};
            const sf::Vector2f bottomLeft = particle.position + sf::Vector2f{-half, half};
            const sf::Vector2f bottomRight = particle.position + sf::Vector2f{half, half};
            vertices[vertex++] = sf::Vertex{topLeft, particle.color};
            vertices[vertex++] = sf::Vertex{bottomLeft, particle.color};
            vertices[vertex++] = sf::Vertex{bottomRight, particle.color};
            vertices[vertex++] = sf::Vertex{topLeft, particle.color};
            vertices[vertex++] = sf::Vertex{bottomRight, particle.color};
            vertices[vertex++] = sf::Vertex{topRight, particle.color};
        }

        if (!m_particles.empty()) window.draw(vertices);
    }

    ParticleEmitterConfig2D ParticleEmitter2D::sanitize(ParticleEmitterConfig2D config)
    {
        config.emissionRate = nonNegative(config.emissionRate);
        config.minimumLifetime = std::max(nonNegative(config.minimumLifetime), 0.0001f);
        config.maximumLifetime =
            std::max(nonNegative(config.maximumLifetime), config.minimumLifetime);
        config.minimumSpeed = nonNegative(config.minimumSpeed);
        config.maximumSpeed = std::max(nonNegative(config.maximumSpeed), config.minimumSpeed);
        config.directionDegrees = finiteOrZero(config.directionDegrees);
        config.spreadDegrees = std::min(nonNegative(config.spreadDegrees), 360.f);
        config.gravity = {finiteOrZero(config.gravity.x), finiteOrZero(config.gravity.y)};
        config.startSize = nonNegative(config.startSize);
        config.endSize = nonNegative(config.endSize);
        config.maxParticles = std::min(config.maxParticles, MaximumParticleCount);
        return config;
    }

    void ParticleEmitter2D::updateParticlePresentation(ParticleState2D& particle) const
    {
        const float alpha = std::clamp(particle.age / particle.lifetime, 0.f, 1.f);
        particle.size = m_config.startSize + (m_config.endSize - m_config.startSize) * alpha;
        particle.color = {interpolateChannel(m_config.startColor.r, m_config.endColor.r, alpha),
                          interpolateChannel(m_config.startColor.g, m_config.endColor.g, alpha),
                          interpolateChannel(m_config.startColor.b, m_config.endColor.b, alpha),
                          interpolateChannel(m_config.startColor.a, m_config.endColor.a, alpha)};
    }
}

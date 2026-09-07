#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/PlaybackDevice.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/SoundSource.hpp>

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <utility>

namespace l2d
{
    namespace
    {
        bool validVolume(float volume) noexcept
        {
            return std::isfinite(volume) && volume >= 0.f && volume <= 100.f;
        }

        float effectiveVolume(float sourceVolume, float busVolume, float masterVolume) noexcept
        {
            const double scaled = static_cast<double>(sourceVolume) *
                                  static_cast<double>(busVolume) *
                                  static_cast<double>(masterVolume) / 10000.0;
            return static_cast<float>(std::clamp(scaled, 0.0, 100.0));
        }
    }

    struct AudioSystem::Impl
    {
        struct Voice
        {
            AudioVoiceId id = InvalidAudioVoiceId;
            SoundBufferHandle buffer;
            sf::Sound sound;
            AudioBus2D bus = AudioBus2D::Effects;
            float sourceVolume = 100.f;

            Voice(AudioVoiceId voiceId, SoundBufferHandle source, const AudioPlayOptions2D& options)
                : id(voiceId), buffer(std::move(source)), sound(*buffer), bus(options.bus),
                  sourceVolume(options.volume)
            {
                sound.setPitch(options.pitch);
                sound.setPan(options.pan);
                sound.setLooping(options.looping);
            }
        };

        AudioVoiceId nextVoice = 1u;
        std::list<Voice> voices;
        std::unique_ptr<sf::Music> music;
        float masterVolume = 100.f;
        float effectsVolume = 100.f;
        float uiVolume = 100.f;
        float musicVolume = 100.f;
        bool musicLooping = false;

        float busVolume(AudioBus2D bus) const noexcept
        {
            return bus == AudioBus2D::Ui ? uiVolume : effectsVolume;
        }

        void applyVoiceVolume(Voice& voice) const noexcept
        {
            voice.sound.setVolume(
                effectiveVolume(voice.sourceVolume, busVolume(voice.bus), masterVolume));
        }

        void applyAllVoiceVolumes() noexcept
        {
            for (Voice& voice : voices)\n                applyVoiceVolume(voice);
        }

        void applyMusicVolume() noexcept
        {
            if (music) music->setVolume(effectiveVolume(100.f, musicVolume, masterVolume));
        }

        AudioVoiceId allocateVoiceId() noexcept
        {
            for (std::size_t attempts = 0u; attempts <= voices.size(); ++attempts)
            {
                AudioVoiceId candidate = nextVoice++;
                if (nextVoice == InvalidAudioVoiceId) nextVoice = 1u;
                if (candidate == InvalidAudioVoiceId) continue;

                const auto existing =
                    std::find_if(voices.begin(), voices.end(),
                                 [candidate](const Voice& voice) { return voice.id == candidate; });
                if (existing == voices.end()) return candidate;
            }

            return InvalidAudioVoiceId;
        }

        auto findVoice(AudioVoiceId id)
        {
            return std::find_if(voices.begin(), voices.end(),
                                [id](const Voice& voice) { return voice.id == id; });
        }
    };

    AudioSystem::AudioSystem() : m_impl(std::make_unique<Impl>()) {}

    AudioSystem::~AudioSystem() = default;
    AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
    AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

    bool AudioSystem::useNullPlaybackDevice()
    {
        return sf::PlaybackDevice::setDeviceToNull();
    }

    bool AudioSystem::isValidPlayOptions(const AudioPlayOptions2D& options) noexcept
    {
        return validVolume(options.volume) && std::isfinite(options.pitch) && options.pitch > 0.f &&
               std::isfinite(options.pan) && options.pan >= -1.f && options.pan <= 1.f;
    }

    AudioVoiceId AudioSystem::playSound(SoundBufferHandle buffer, AudioPlayOptions2D options)
    {
        if (!buffer || buffer->getSampleCount() == 0u || !isValidPlayOptions(options))
            return InvalidAudioVoiceId;

        const AudioVoiceId id = m_impl->allocateVoiceId();
        if (id == InvalidAudioVoiceId) return InvalidAudioVoiceId;

        m_impl->voices.emplace_back(id, std::move(buffer), options);
        Impl::Voice& voice = m_impl->voices.back();
        m_impl->applyVoiceVolume(voice);
        voice.sound.play();
        return id;
    }

    bool AudioSystem::pauseVoice(AudioVoiceId voice)
    {
        const auto iterator = m_impl->findVoice(voice);
        if (iterator == m_impl->voices.end()) return false;
        iterator->sound.pause();
        return true;
    }

    bool AudioSystem::resumeVoice(AudioVoiceId voice)
    {
        const auto iterator = m_impl->findVoice(voice);
        if (iterator == m_impl->voices.end()) return false;
        iterator->sound.play();
        return true;
    }

    bool AudioSystem::stopVoice(AudioVoiceId voice)
    {
        const auto iterator = m_impl->findVoice(voice);
        if (iterator == m_impl->voices.end()) return false;
        iterator->sound.stop();
        m_impl->voices.erase(iterator);
        return true;
    }

    void AudioSystem::stopAllSounds()
    {
        for (Impl::Voice& voice : m_impl->voices)\n            voice.sound.stop();
        m_impl->voices.clear();
    }

    void AudioSystem::update()
    {
        m_impl->voices.remove_if(
            [](const Impl::Voice& voice)
            { return voice.sound.getStatus() == sf::SoundSource::Status::Stopped; });
    }

    std::size_t AudioSystem::activeVoiceCount() const noexcept
    {
        return m_impl->voices.size();
    }

    bool AudioSystem::setMasterVolume(float volume) noexcept
    {
        if (!validVolume(volume)) return false;
        m_impl->masterVolume = volume;
        m_impl->applyAllVoiceVolumes();
        m_impl->applyMusicVolume();
        return true;
    }

    bool AudioSystem::setEffectsVolume(float volume) noexcept
    {
        if (!validVolume(volume)) return false;
        m_impl->effectsVolume = volume;
        m_impl->applyAllVoiceVolumes();
        return true;
    }

    bool AudioSystem::setUiVolume(float volume) noexcept
    {
        if (!validVolume(volume)) return false;
        m_impl->uiVolume = volume;
        m_impl->applyAllVoiceVolumes();
        return true;
    }

    bool AudioSystem::setMusicVolume(float volume) noexcept
    {
        if (!validVolume(volume)) return false;
        m_impl->musicVolume = volume;
        m_impl->applyMusicVolume();
        return true;
    }

    float AudioSystem::masterVolume() const noexcept
    {
        return m_impl->masterVolume;
    }

    float AudioSystem::effectsVolume() const noexcept
    {
        return m_impl->effectsVolume;
    }

    float AudioSystem::uiVolume() const noexcept
    {
        return m_impl->uiVolume;
    }

    float AudioSystem::musicVolume() const noexcept
    {
        return m_impl->musicVolume;
    }

    bool AudioSystem::openMusicFromFile(const std::filesystem::path& path)
    {
        if (path.empty()) return false;

        auto replacement = std::make_unique<sf::Music>();
        if (!replacement->openFromFile(path)) return false;

        replacement->setLooping(m_impl->musicLooping);
        replacement->setVolume(effectiveVolume(100.f, m_impl->musicVolume, m_impl->masterVolume));
        m_impl->music = std::move(replacement);
        return true;
    }

    bool AudioSystem::openMusic(const ResourceLocator& locator, const std::string& resource)
    {
        const std::optional<ResourceLocator::Path> path = locator.locate(resource);
        return path && openMusicFromFile(*path);
    }

    bool AudioSystem::hasMusic() const noexcept
    {
        return m_impl->music != nullptr;
    }

    bool AudioSystem::musicPlaying() const noexcept
    {
        return m_impl->music && m_impl->music->getStatus() == sf::SoundSource::Status::Playing;
    }

    void AudioSystem::playMusic()
    {
        if (m_impl->music) m_impl->music->play();
    }

    void AudioSystem::pauseMusic()
    {
        if (m_impl->music) m_impl->music->pause();
    }

    void AudioSystem::stopMusic()
    {
        if (m_impl->music) m_impl->music->stop();
    }

    void AudioSystem::setMusicLooping(bool looping)
    {
        m_impl->musicLooping = looping;
        if (m_impl->music) m_impl->music->setLooping(looping);
    }

    bool AudioSystem::musicLooping() const noexcept
    {
        return m_impl->musicLooping;
    }
}

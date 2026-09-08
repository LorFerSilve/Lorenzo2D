#pragma once

#include <Lorenzo2D/Assets/AssetHandle.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace l2d
{
    class ResourceLocator;

    using AudioVoiceId = std::uint64_t;
    inline constexpr AudioVoiceId InvalidAudioVoiceId = 0u;

    enum class AudioBus2D
    {
        Effects,
        Ui
    };

    struct AudioPlayOptions2D
    {
        AudioBus2D bus = AudioBus2D::Effects;
        float volume = 100.f;
        float pitch = 1.f;
        float pan = 0.f;
        bool looping = false;
    };

    class AudioSystem
    {
      public:
        AudioSystem();
        ~AudioSystem();

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;
        AudioSystem(AudioSystem&&) noexcept;
        AudioSystem& operator=(AudioSystem&&) noexcept;

        static bool useNullPlaybackDevice();
        static bool isValidPlayOptions(const AudioPlayOptions2D& options) noexcept;

        AudioVoiceId playSound(SoundBufferHandle buffer, AudioPlayOptions2D options = {});
        bool pauseVoice(AudioVoiceId voice);
        bool resumeVoice(AudioVoiceId voice);
        bool stopVoice(AudioVoiceId voice);
        void stopAllSounds();
        void update();
        std::size_t activeVoiceCount() const noexcept;

        bool setMasterVolume(float volume) noexcept;
        bool setEffectsVolume(float volume) noexcept;
        bool setUiVolume(float volume) noexcept;
        bool setMusicVolume(float volume) noexcept;

        float masterVolume() const noexcept;
        float effectsVolume() const noexcept;
        float uiVolume() const noexcept;
        float musicVolume() const noexcept;

        bool openMusicFromFile(const std::filesystem::path& path);
        bool openMusic(const ResourceLocator& locator, const std::string& resource);
        bool hasMusic() const noexcept;
        bool musicPlaying() const noexcept;
        void playMusic();
        void pauseMusic();
        void stopMusic();
        void setMusicLooping(bool looping);
        bool musicLooping() const noexcept;

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}

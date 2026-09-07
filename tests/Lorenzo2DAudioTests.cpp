#include "TestSupport.hpp"

#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Audio/AudioSystem.hpp>

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/SoundChannel.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    using l2d::test::runTest;

    l2d::SoundBufferHandle makeBuffer()
    {
        std::vector<std::int16_t> samples(512u);
        for (std::size_t index = 0u; index < samples.size(); ++index)
            samples[index] = index % 2u == 0u ? std::int16_t{1200} : std::int16_t{-1200};

        auto buffer = std::make_shared<sf::SoundBuffer>();
        const std::vector<sf::SoundChannel> channels{sf::SoundChannel::Mono};
        L2D_REQUIRE(buffer->loadFromSamples(samples.data(), samples.size(), 1u, 44100u, channels));
        return l2d::SoundBufferHandle(std::move(buffer));
    }

    void testOptionsAndVolumes()
    {
        l2d::AudioPlayOptions2D options;
        L2D_REQUIRE(l2d::AudioSystem::isValidPlayOptions(options));
        options.pitch = 0.f;
        L2D_REQUIRE(!l2d::AudioSystem::isValidPlayOptions(options));

        l2d::AudioSystem audio;
        L2D_REQUIRE(audio.setMasterVolume(80.f));
        L2D_REQUIRE(audio.setEffectsVolume(70.f));
        L2D_REQUIRE(audio.setUiVolume(60.f));
        L2D_REQUIRE(audio.setMusicVolume(50.f));
        L2D_REQUIRE(audio.masterVolume() == 80.f);
        L2D_REQUIRE(audio.effectsVolume() == 70.f);
        L2D_REQUIRE(audio.uiVolume() == 60.f);
        L2D_REQUIRE(audio.musicVolume() == 50.f);
        L2D_REQUIRE(!audio.setMasterVolume(101.f));
    }

    void testVoiceLifecycle()
    {
        l2d::AudioSystem audio;
        const l2d::SoundBufferHandle buffer = makeBuffer();

        l2d::AudioPlayOptions2D options;
        options.bus = l2d::AudioBus2D::Ui;
        options.volume = 40.f;

        const l2d::AudioVoiceId voice = audio.playSound(buffer, options);
        L2D_REQUIRE(voice != l2d::InvalidAudioVoiceId);
        L2D_REQUIRE(audio.activeVoiceCount() == 1u);
        L2D_REQUIRE(audio.pauseVoice(voice));
        L2D_REQUIRE(audio.resumeVoice(voice));
        L2D_REQUIRE(audio.stopVoice(voice));
        L2D_REQUIRE(audio.activeVoiceCount() == 0u);
        L2D_REQUIRE(!audio.stopVoice(voice));
        L2D_REQUIRE(audio.playSound({}, options) == l2d::InvalidAudioVoiceId);
    }

    void testSoundBufferAssetLifetimeAndLiveBinding()
    {
        l2d::AssetManager assets;
        const l2d::SoundBufferHandle first = makeBuffer();
        L2D_REQUIRE(assets.storeSoundBuffer("click", first));

        const l2d::LiveSoundBufferHandle live = assets.liveSoundBuffer("click");
        L2D_REQUIRE(live.snapshot() == first);
        const std::uint64_t generation = live.generation();

        const l2d::SoundBufferHandle second = makeBuffer();
        L2D_REQUIRE(assets.storeSoundBuffer("click", second));
        L2D_REQUIRE(live.generation() == generation + 1u);
        L2D_REQUIRE(live.snapshot() == second);
        L2D_REQUIRE(assets.unloadSoundBuffer("click"));
        L2D_REQUIRE(!live.snapshot());
        L2D_REQUIRE(static_cast<bool>(first));
    }
}

int main()
{
    if (!l2d::AudioSystem::useNullPlaybackDevice()) return 2;

    int failures = 0;
    runTest("audio options and volumes", testOptionsAndVolumes, failures);
    runTest("audio voice lifecycle", testVoiceLifecycle, failures);
    runTest("audio sound buffer assets", testSoundBufferAssetLifetimeAndLiveBinding, failures);
    return failures == 0 ? 0 : 1;
}

# Audio

Lorenzo2D 1.0 exposes game-facing audio through `AudioSystem` and integrates
short sound effects with `AssetManager`.

The engine links against SFML Graphics and SFML Audio as public package
dependencies.

## Sound buffers and asset lifetime

`SoundBufferHandle` follows the same immutable shared-lease contract as
textures and fonts. `LiveSoundBufferHandle` follows a named registry slot and
advances its generation after successful replacement or unload.

`AssetManager` provides `loadSoundBuffer`, `storeSoundBuffer`,
`getSoundBuffer`, `liveSoundBuffer`, `unloadSoundBuffer`, and matching
count/query/clear operations.

Existing snapshot handles remain valid after registry replacement or removal.

## Sound playback

`AudioSystem::playSound` accepts a `SoundBufferHandle` and returns a stable
`AudioVoiceId`. Invalid or empty buffers and invalid playback options are
rejected.

`AudioPlayOptions2D` supports:

- effects or UI bus routing;
- source volume from 0 to 100;
- positive finite pitch;
- pan from -1 to 1;
- looping.

Active voices can be paused, resumed, stopped individually, or stopped as a
group. Call `update()` during the presentation frame to retire voices that
finished naturally.

## Volume buses

Master, effects, UI, and music volumes are independently configurable in the
range 0 to 100. Effective source volume is the source volume multiplied by its
bus and master volumes.

Invalid volume changes are rejected without replacing the previous valid value.

## Music

`openMusicFromFile` and the `ResourceLocator` overload open streamed music
transactionally. A failed replacement preserves the currently loaded music.

Music exposes play, pause, stop, looping, and music-bus volume.

## Headless validation

`AudioSystem::useNullPlaybackDevice()` selects SFML's silent playback device.
The regression suite uses this path so lifecycle and asset tests do not depend on
physical audio hardware.

## Scope and exclusions

The 1.0 baseline does not provide spatial/3D audio authoring, DSP graphs,
cross-fade scheduling, audio snapshots, voice stealing priorities, compressed
asset preprocessing, or a native mixer editor.

#pragma once

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/ECS/Component.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace l2d
{
    // Drives the SpriteRenderer on the same game object.
    class Animator : public Component
    {
      public:
        bool addClip(AnimationClip clip);
        bool removeClip(const std::string& name);
        void clearClips();

        bool hasClip(const std::string& name) const;
        const AnimationClip* findClip(const std::string& name) const;
        std::size_t clipCount() const;

        // Starting a different clip restarts it. Replaying the current clip
        // resumes it unless restart is true.
        bool play(const std::string& name, bool restart = false);
        void pause();
        void stop();

        bool isPlaying() const;
        bool isFinished() const;
        const std::string& currentClipName() const;
        std::size_t currentFrameIndex() const;
        float frameElapsedTime() const;

        // Invalid or negative speeds are rejected. Zero acts as a pause while
        // preserving the explicit playing state.
        bool setPlaybackSpeed(float speed);
        float playbackSpeed() const;

        // Reapplies the current frame after a SpriteRenderer is added or
        // replaced. Returns false when either side is unavailable.
        bool applyCurrentFrame();

        void onUpdate(float deltaTime) override;

      private:
        void resetPlaybackState();

      private:
        std::unordered_map<std::string, AnimationClip> m_clips;
        std::string m_currentClip;
        std::size_t m_currentFrame = 0;
        double m_elapsedInFrame = 0.0;
        float m_playbackSpeed = 1.f;
        bool m_playing = false;
        bool m_finished = false;
    };
}

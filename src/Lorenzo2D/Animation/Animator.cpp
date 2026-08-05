#include <Lorenzo2D/Animation/Animator.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <cmath>
#include <utility>

namespace l2d
{
    bool Animator::addClip(AnimationClip clip)
    {
        if (clip.name().empty() || clip.frames().empty()) return false;

        const std::string name = clip.name();
        m_clips.insert_or_assign(name, std::move(clip));

        if (m_currentClip == name)
        {
            resetPlaybackState();
            applyCurrentFrame();
        }

        return true;
    }

    bool Animator::removeClip(const std::string& name)
    {
        if (m_clips.erase(name) == 0u) return false;

        if (m_currentClip == name)
        {
            m_currentClip.clear();
            resetPlaybackState();
        }

        return true;
    }

    void Animator::clearClips()
    {
        m_clips.clear();
        m_currentClip.clear();
        resetPlaybackState();
    }

    bool Animator::hasClip(const std::string& name) const
    {
        return m_clips.find(name) != m_clips.end();
    }

    const AnimationClip* Animator::findClip(const std::string& name) const
    {
        const auto iterator = m_clips.find(name);
        return iterator == m_clips.end() ? nullptr : &iterator->second;
    }

    std::size_t Animator::clipCount() const
    {
        return m_clips.size();
    }

    bool Animator::play(const std::string& name, bool restart)
    {
        if (!hasClip(name)) return false;

        if (m_currentClip != name || restart || m_finished)
        {
            m_currentClip = name;
            resetPlaybackState();
        }

        m_playing = true;
        m_finished = false;
        applyCurrentFrame();
        return true;
    }

    void Animator::pause()
    {
        m_playing = false;
    }

    void Animator::stop()
    {
        resetPlaybackState();
        applyCurrentFrame();
    }

    bool Animator::isPlaying() const
    {
        return m_playing;
    }

    bool Animator::isFinished() const
    {
        return m_finished;
    }

    const std::string& Animator::currentClipName() const
    {
        return m_currentClip;
    }

    std::size_t Animator::currentFrameIndex() const
    {
        return m_currentFrame;
    }

    float Animator::frameElapsedTime() const
    {
        return static_cast<float>(m_elapsedInFrame);
    }

    bool Animator::setPlaybackSpeed(float speed)
    {
        if (!std::isfinite(speed) || speed < 0.f) return false;

        m_playbackSpeed = speed;
        return true;
    }

    float Animator::playbackSpeed() const
    {
        return m_playbackSpeed;
    }

    bool Animator::applyCurrentFrame()
    {
        GameObject* gameObject = owner();
        const AnimationClip* clip = findClip(m_currentClip);

        if (gameObject == nullptr || clip == nullptr || m_currentFrame >= clip->frameCount())
        {
            return false;
        }

        SpriteRenderer* renderer = gameObject->getComponent<SpriteRenderer>();

        if (renderer == nullptr) return false;

        renderer->setTextureRect(clip->frames()[m_currentFrame].textureRect);
        return true;
    }

    void Animator::onUpdate(float deltaTime)
    {
        if (!m_playing || m_playbackSpeed == 0.f || !std::isfinite(deltaTime) || deltaTime <= 0.f)
        {
            return;
        }

        const AnimationClip* clip = findClip(m_currentClip);

        if (clip == nullptr || clip->frames().empty())
        {
            resetPlaybackState();
            return;
        }

        double advance = static_cast<double>(deltaTime) * static_cast<double>(m_playbackSpeed);

        if (!std::isfinite(advance)) return;

        const double clipDuration = static_cast<double>(clip->duration());

        if (clip->isLooping() && clipDuration > 0.0 && advance >= clipDuration)
        {
            advance = std::fmod(advance, clipDuration);
        }

        m_elapsedInFrame += advance;

        while (m_elapsedInFrame >= static_cast<double>(clip->frames()[m_currentFrame].duration))
        {
            m_elapsedInFrame -= static_cast<double>(clip->frames()[m_currentFrame].duration);

            if (m_currentFrame + 1u < clip->frameCount())
            {
                ++m_currentFrame;
            }
            else if (clip->isLooping())
            {
                m_currentFrame = 0u;
            }
            else
            {
                m_currentFrame = clip->frameCount() - 1u;
                m_elapsedInFrame = 0.0;
                m_playing = false;
                m_finished = true;
                break;
            }
        }

        applyCurrentFrame();
    }

    void Animator::resetPlaybackState()
    {
        m_currentFrame = 0u;
        m_elapsedInFrame = 0.0;
        m_playing = false;
        m_finished = false;
    }
}

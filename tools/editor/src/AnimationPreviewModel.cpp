#include <Lorenzo2DEditor/AnimationPreviewModel.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        bool finiteNonNegative(float value) noexcept
        {
            return std::isfinite(value) && value >= 0.f;
        }
    }

    AnimationPreviewModel::AnimationPreviewModel(EditorDocument& document) noexcept
        : m_document(&document)
    {
    }

    const EditorObjectRecord* AnimationPreviewModel::selectedAnimatorObject() const noexcept
    {
        const EditorObjectRecord* object = m_document->findObject(m_document->selectedObject());
        if (!object || !object->prefab.animator) return nullptr;
        return object;
    }

    std::vector<l2d::AssetId> AnimationPreviewModel::availableClipAssets() const
    {
        const EditorObjectRecord* object = selectedAnimatorObject();
        if (!object) return {};
        return object->prefab.animator->clips;
    }

    std::optional<l2d::AssetId> AnimationPreviewModel::initialClipAsset() const
    {
        const EditorObjectRecord* object = selectedAnimatorObject();
        if (!object || object->prefab.animator->initialClip.empty()) return std::nullopt;
        return object->prefab.animator->initialClip;
    }

    bool AnimationPreviewModel::selectedAnimatorReferences(const l2d::AssetId& asset) const noexcept
    {
        const EditorObjectRecord* object = selectedAnimatorObject();
        if (!object || asset.empty()) return false;
        const auto& clips = object->prefab.animator->clips;
        return std::find(clips.begin(), clips.end(), asset) != clips.end();
    }

    bool AnimationPreviewModel::bindingStillValid() const noexcept
    {
        if (!m_clip || m_objectId == InvalidEditorObjectId || m_clipAsset.empty()) return false;
        const EditorObjectRecord* object = selectedAnimatorObject();
        return object && object->id == m_objectId && selectedAnimatorReferences(m_clipAsset);
    }

    bool AnimationPreviewModel::bindClip(l2d::AssetId asset, l2d::AnimationClip clip)
    {
        const EditorObjectRecord* object = selectedAnimatorObject();
        if (!object || !selectedAnimatorReferences(asset) || clip.frameCount() == 0u ||
            clip.frameCount() > MaximumPreviewFrames || !finiteNonNegative(clip.duration()) ||
            clip.duration() <= 0.f)
            return false;

        m_objectId = object->id;
        m_clipAsset = std::move(asset);
        m_clip = std::move(clip);
        m_playbackSpeed = object->prefab.animator->playbackSpeed;
        if (!std::isfinite(m_playbackSpeed) || m_playbackSpeed < 0.f ||
            m_playbackSpeed > MaximumPlaybackSpeed)
            m_playbackSpeed = 1.f;
        resetPlayback();
        m_playing = object->prefab.animator->playing;
        return true;
    }

    bool AnimationPreviewModel::bindInitialClip(l2d::AnimationClip clip)
    {
        const auto asset = initialClipAsset();
        return asset && bindClip(*asset, std::move(clip));
    }

    void AnimationPreviewModel::clear() noexcept
    {
        m_objectId = InvalidEditorObjectId;
        m_clipAsset.clear();
        m_clip.reset();
        m_elapsed = 0.0;
        m_playbackSpeed = 1.f;
        m_frameIndex = 0u;
        m_frameElapsed = 0.0;
        m_playing = false;
        m_finished = false;
    }

    bool AnimationPreviewModel::hasClip() const noexcept
    {
        return bindingStillValid();
    }

    bool AnimationPreviewModel::play()
    {
        if (!bindingStillValid())
        {
            clear();
            return false;
        }
        if (m_finished)
        {
            m_elapsed = 0.0;
            m_finished = false;
            resolveCursor();
        }
        m_playing = true;
        return true;
    }

    void AnimationPreviewModel::pause() noexcept
    {
        m_playing = false;
    }

    void AnimationPreviewModel::stop() noexcept
    {
        if (!bindingStillValid())
        {
            clear();
            return;
        }
        resetPlayback();
    }

    bool AnimationPreviewModel::setPlaybackSpeed(float speed) noexcept
    {
        if (!std::isfinite(speed) || speed < 0.f || speed > MaximumPlaybackSpeed) return false;
        m_playbackSpeed = speed;
        return true;
    }

    float AnimationPreviewModel::playbackSpeed() const noexcept
    {
        return m_playbackSpeed;
    }

    bool AnimationPreviewModel::seek(float seconds)
    {
        if (!bindingStillValid() || !finiteNonNegative(seconds)) return false;
        const double duration = static_cast<double>(m_clip->duration());
        m_elapsed = std::min(static_cast<double>(seconds), duration);
        m_finished = !m_clip->isLooping() && m_elapsed >= duration;
        if (m_finished) m_playing = false;
        resolveCursor();
        return true;
    }

    bool AnimationPreviewModel::stepFrame(int offset)
    {
        if (!bindingStillValid() || offset == 0) return false;
        const auto& frames = m_clip->frames();
        const std::size_t count = frames.size();
        if (count == 0u) return false;

        long long target = static_cast<long long>(m_frameIndex) + static_cast<long long>(offset);
        if (m_clip->isLooping())
        {
            const long long signedCount = static_cast<long long>(count);
            target %= signedCount;
            if (target < 0) target += signedCount;
        }
        else
        {
            target = std::max<long long>(
                0, std::min<long long>(target, static_cast<long long>(count - 1u)));
        }

        double elapsed = 0.0;
        for (std::size_t i = 0u; i < static_cast<std::size_t>(target); ++i)
            elapsed += static_cast<double>(frames[i].duration);
        m_elapsed = elapsed;
        m_frameIndex = static_cast<std::size_t>(target);
        m_frameElapsed = 0.0;
        m_finished = false;
        m_playing = false;
        return true;
    }

    void AnimationPreviewModel::update(float deltaTime) noexcept
    {
        if (!bindingStillValid())
        {
            clear();
            return;
        }
        if (!m_playing || !finiteNonNegative(deltaTime) || deltaTime == 0.f ||
            m_playbackSpeed == 0.f)
            return;

        const double duration = static_cast<double>(m_clip->duration());
        const double advance =
            static_cast<double>(deltaTime) * static_cast<double>(m_playbackSpeed);
        if (!std::isfinite(advance)) return;

        if (m_clip->isLooping())
        {
            m_elapsed = std::fmod(m_elapsed + advance, duration);
            if (m_elapsed < 0.0) m_elapsed += duration;
            m_finished = false;
        }
        else
        {
            m_elapsed = std::min(m_elapsed + advance, duration);
            if (m_elapsed >= duration)
            {
                m_finished = true;
                m_playing = false;
            }
        }
        resolveCursor();
    }

    void AnimationPreviewModel::resetPlayback() noexcept
    {
        m_elapsed = 0.0;
        m_frameIndex = 0u;
        m_frameElapsed = 0.0;
        m_playing = false;
        m_finished = false;
        resolveCursor();
    }

    void AnimationPreviewModel::resolveCursor() noexcept
    {
        if (!m_clip || m_clip->frames().empty())
        {
            m_frameIndex = 0u;
            m_frameElapsed = 0.0;
            return;
        }

        const auto& frames = m_clip->frames();
        double remaining = m_elapsed;
        for (std::size_t index = 0u; index < frames.size(); ++index)
        {
            const double frameDuration = static_cast<double>(frames[index].duration);
            const bool last = index + 1u == frames.size();
            if (remaining < frameDuration || last)
            {
                m_frameIndex = index;
                m_frameElapsed = std::min(remaining, frameDuration);
                return;
            }
            remaining -= frameDuration;
        }
    }

    std::optional<AnimationPreviewSnapshot> AnimationPreviewModel::snapshot() const
    {
        if (!bindingStillValid()) return std::nullopt;
        const auto& frames = m_clip->frames();
        if (m_frameIndex >= frames.size()) return std::nullopt;

        AnimationPreviewSnapshot result;
        result.objectId = m_objectId;
        result.clipAsset = m_clipAsset;
        result.clipName = m_clip->name();
        result.textureRect = frames[m_frameIndex].textureRect;
        result.frameIndex = m_frameIndex;
        result.frameCount = frames.size();
        result.frameElapsed = static_cast<float>(m_frameElapsed);
        result.clipElapsed = static_cast<float>(m_elapsed);
        result.clipDuration = m_clip->duration();
        result.playbackSpeed = m_playbackSpeed;
        result.looping = m_clip->isLooping();
        result.playing = m_playing;
        result.finished = m_finished;
        return result;
    }
}

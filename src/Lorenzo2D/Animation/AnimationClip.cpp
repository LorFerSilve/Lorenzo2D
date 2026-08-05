#include <Lorenzo2D/Animation/AnimationClip.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        bool isValidFrame(sf::IntRect rectangle, float duration)
        {
            return rectangle.size.x != 0 && rectangle.size.y != 0 && std::isfinite(duration) &&
                   duration > 0.f;
        }

        bool checkedCoordinate(int first, int size, std::size_t offset, int& result)
        {
            if (first < 0 || size <= 0) return false;

            const auto maximumOffset = static_cast<unsigned long long>(
                (std::numeric_limits<long long>::max() - static_cast<long long>(first)) / size);

            if (static_cast<unsigned long long>(offset) > maximumOffset) return false;

            const long long value = static_cast<long long>(first) +
                                    static_cast<long long>(size) * static_cast<long long>(offset);

            if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            {
                return false;
            }

            result = static_cast<int>(value);
            return true;
        }
    }

    AnimationClip::AnimationClip(std::string name, bool looping)
        : m_name(std::move(name)), m_looping(looping)
    {
    }

    const std::string& AnimationClip::name() const
    {
        return m_name;
    }

    void AnimationClip::setName(std::string name)
    {
        m_name = std::move(name);
    }

    bool AnimationClip::isLooping() const
    {
        return m_looping;
    }

    void AnimationClip::setLooping(bool looping)
    {
        m_looping = looping;
    }

    bool AnimationClip::addFrame(sf::IntRect textureRect, float duration)
    {
        if (!isValidFrame(textureRect, duration)) return false;

        m_frames.push_back({textureRect, duration});
        return true;
    }

    void AnimationClip::clearFrames()
    {
        m_frames.clear();
    }

    const std::vector<AnimationFrame>& AnimationClip::frames() const
    {
        return m_frames;
    }

    std::size_t AnimationClip::frameCount() const
    {
        return m_frames.size();
    }

    float AnimationClip::duration() const
    {
        double total = 0.0;

        for (const AnimationFrame& frame : m_frames)
        {
            total += static_cast<double>(frame.duration);
        }

        const double maximum = static_cast<double>(std::numeric_limits<float>::max());
        return static_cast<float>(total > maximum ? maximum : total);
    }

    bool AnimationClip::addGridFrames(sf::Vector2i firstCell, sf::Vector2i frameSize,
                                      std::size_t frameCount, float frameDuration, bool horizontal)
    {
        if (firstCell.x < 0 || firstCell.y < 0 || frameSize.x <= 0 || frameSize.y <= 0 ||
            frameCount == 0u || !std::isfinite(frameDuration) || frameDuration <= 0.f)
        {
            return false;
        }

        std::vector<AnimationFrame> frames;
        frames.reserve(frameCount);

        for (std::size_t index = 0; index < frameCount; ++index)
        {
            int column = firstCell.x;
            int row = firstCell.y;

            if (horizontal)
            {
                if (!checkedCoordinate(firstCell.x, 1, index, column)) return false;
            }
            else if (!checkedCoordinate(firstCell.y, 1, index, row))
            {
                return false;
            }

            int left = 0;
            int top = 0;

            if (!checkedCoordinate(0, frameSize.x, static_cast<std::size_t>(column), left) ||
                !checkedCoordinate(0, frameSize.y, static_cast<std::size_t>(row), top))
            {
                return false;
            }

            frames.push_back({{{left, top}, frameSize}, frameDuration});
        }

        m_frames.insert(m_frames.end(), frames.begin(), frames.end());
        return true;
    }
}

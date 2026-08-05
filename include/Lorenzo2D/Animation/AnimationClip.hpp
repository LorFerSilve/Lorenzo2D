#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace l2d
{
    struct AnimationFrame
    {
        sf::IntRect textureRect;
        float duration = 0.1f;
    };

    class AnimationClip
    {
      public:
        explicit AnimationClip(std::string name = {}, bool looping = true);

        const std::string& name() const;
        void setName(std::string name);

        bool isLooping() const;
        void setLooping(bool looping);

        // Frames require a non-empty rectangle and a finite positive duration.
        bool addFrame(sf::IntRect textureRect, float duration = 0.1f);
        void clearFrames();

        const std::vector<AnimationFrame>& frames() const;
        std::size_t frameCount() const;
        float duration() const;

        // Appends a horizontal or vertical grid run. The operation is
        // transactional: invalid ranges do not add partial clips.
        bool addGridFrames(sf::Vector2i firstCell, sf::Vector2i frameSize, std::size_t frameCount,
                           float frameDuration = 0.1f, bool horizontal = true);

      private:
        std::string m_name;
        std::vector<AnimationFrame> m_frames;
        bool m_looping = true;
    };
}

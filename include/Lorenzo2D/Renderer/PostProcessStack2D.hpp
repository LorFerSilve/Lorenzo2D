#pragma once

#include <SFML/Graphics/Color.hpp>

#include <cstddef>
#include <vector>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    enum class PostProcessBlend2D
    {
        Alpha,
        Add,
        Multiply
    };

    struct PostProcessPass2D
    {
        sf::Color color = sf::Color::Transparent;
        PostProcessBlend2D blend = PostProcessBlend2D::Alpha;
        bool enabled = true;
    };

    // Lightweight screen-space color grading. Apply after Scene::render and
    // before display; passes execute in insertion order in pixel coordinates.
    class PostProcessStack2D
    {
      public:
        std::size_t addPass(PostProcessPass2D pass);
        bool setPass(std::size_t index, PostProcessPass2D pass);
        bool removePass(std::size_t index);
        void clear();

        std::size_t passCount() const;
        const PostProcessPass2D* pass(std::size_t index) const;
        void apply(sf::RenderWindow& window) const;

      private:
        std::vector<PostProcessPass2D> m_passes;
    };
}

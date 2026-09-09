#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Renderer/SpriteBatch2D.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <stdexcept>

class Phase12BatchingExample final : public l2d::Application
{
  public:
    Phase12BatchingExample()
        : l2d::Application(800, 450, "Lorenzo2D Phase 12 sprite batching")
    {
        sf::Image atlas({2u, 1u}, sf::Color(245, 170, 55));
        atlas.setPixel({1u, 0u}, sf::Color(70, 155, 255));

        auto texture = std::make_shared<sf::Texture>();
        if (!texture->loadFromImage(atlas))
            throw std::runtime_error("Could not create the in-memory atlas texture");
        m_atlas = l2d::TextureHandle(std::move(texture));

        constexpr std::size_t Columns = 20u;
        constexpr std::size_t Rows = 10u;
        constexpr float CellSize = 24.f;
        constexpr float Spacing = 30.f;

        for (std::size_t row = 0u; row < Rows; ++row)
        {
            for (std::size_t column = 0u; column < Columns; ++column)
            {
                l2d::SpriteBatchSubmission2D submission;
                submission.texture = m_atlas;
                submission.textureRect =
                    {{static_cast<int>((row + column) % 2u), 0}, {1, 1}};
                submission.position = {70.f + static_cast<float>(column) * Spacing,
                                       75.f + static_cast<float>(row) * Spacing};
                submission.scale = {CellSize, CellSize};

                if (!m_batch.submit(std::move(submission)).succeeded())
                    throw std::runtime_error("Could not populate the sprite batch");
            }
        }

        if (m_batch.batchCount() != 1u)
            throw std::runtime_error("Atlas submissions should share one batch");
    }

  private:
    void onRender(sf::RenderWindow& window, float) override
    {
        window.clear(sf::Color(18, 24, 42));
        const l2d::SpriteBatchDrawResult2D result = m_batch.draw(window);
        if (!result.succeeded()) window.clear(sf::Color::Red);
    }

    l2d::TextureHandle m_atlas;
    l2d::SpriteBatch2D m_batch;
};

int main()
{
    Phase12BatchingExample application;
    application.run();
    return 0;
}

#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/SpriteBatch2D.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "TestSupport.hpp"

#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

namespace
{
    using l2d::test::runTest;

    l2d::TextureHandle makeAtlasTexture()
    {
        sf::Image image({2u, 1u}, sf::Color::Red);
        image.setPixel({1u, 0u}, sf::Color::Green);

        auto texture = std::make_shared<sf::Texture>();
        L2D_REQUIRE(texture->loadFromImage(image));
        return l2d::TextureHandle(std::move(texture));
    }

    l2d::TextureHandle makeSolidTexture(sf::Color color)
    {
        const sf::Image image({1u, 1u}, color);
        auto texture = std::make_shared<sf::Texture>();
        L2D_REQUIRE(texture->loadFromImage(image));
        return l2d::TextureHandle(std::move(texture));
    }

    l2d::SpriteBatchSubmission2D submission(l2d::TextureHandle texture, sf::Vector2f position)
    {
        l2d::SpriteBatchSubmission2D result;
        result.texture = std::move(texture);
        result.position = position;
        return result;
    }

    void testSubmissionValidationBatchingAndBounds()
    {
        const l2d::TextureHandle atlas = makeAtlasTexture();
        l2d::SpriteBatch2D batch;

        l2d::SpriteBatchSubmission2D first = submission(atlas, {0.f, 0.f});
        first.textureRect = {{0, 0}, {1, 1}};
        L2D_REQUIRE(l2d::SpriteBatch2D::isValidSubmission(first));

        const l2d::SpriteBatchSubmitResult2D firstResult = batch.submit(first);
        L2D_REQUIRE(firstResult.succeeded());
        L2D_REQUIRE(firstResult.batchIndex == std::optional<std::size_t>(0u));

        l2d::SpriteBatchSubmission2D second = submission(atlas, {1.f, 0.f});
        second.textureRect = {{1, 0}, {1, 1}};
        const l2d::SpriteBatchSubmitResult2D secondResult = batch.submit(second);
        L2D_REQUIRE(secondResult.succeeded());
        L2D_REQUIRE(secondResult.batchIndex == std::optional<std::size_t>(0u));

        L2D_REQUIRE_EQUAL(batch.submissionCount(), 2u);
        L2D_REQUIRE_EQUAL(batch.batchCount(), 1u);
        L2D_REQUIRE_EQUAL(batch.stats().submittedVertexCount, 12u);
        L2D_REQUIRE_EQUAL(batch.stats().textureSwitchCount, 0u);
        L2D_REQUIRE_EQUAL(batch.stats().materialSwitchCount, 0u);

        const std::size_t beforeInvalid = batch.submissionCount();
        l2d::SpriteBatchSubmission2D invalidRect = second;
        invalidRect.textureRect = {{1, 0}, {2, 1}};
        const l2d::SpriteBatchSubmitResult2D invalidRectResult = batch.submit(invalidRect);
        L2D_REQUIRE(invalidRectResult.failure == l2d::SpriteBatchFailure2D::TextureRectInvalid);
        L2D_REQUIRE_EQUAL(batch.submissionCount(), beforeInvalid);

        l2d::SpriteBatchSubmission2D invalidTransform = first;
        invalidTransform.scale.x = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(batch.submit(invalidTransform).failure ==
                    l2d::SpriteBatchFailure2D::TransformInvalid);

        l2d::SpriteBatchSubmission2D missingTexture;
        L2D_REQUIRE(batch.submit(missingTexture).failure ==
                    l2d::SpriteBatchFailure2D::TextureUnavailable);

        l2d::SpriteBatch2D bounded;
        l2d::SpriteBatchSubmission2D boundedSubmission = submission(atlas, {0.f, 0.f});
        for (std::size_t index = 0u; index < l2d::SpriteBatch2D::MaximumSubmissionCount; ++index)
            L2D_REQUIRE(bounded.submit(boundedSubmission).succeeded());

        L2D_REQUIRE_EQUAL(bounded.submissionCount(), l2d::SpriteBatch2D::MaximumSubmissionCount);
        L2D_REQUIRE_EQUAL(bounded.batchCount(), 1u);
        L2D_REQUIRE(bounded.submit(boundedSubmission).failure ==
                    l2d::SpriteBatchFailure2D::CapacityExceeded);

        bounded.clear();
        L2D_REQUIRE(bounded.empty());
        L2D_REQUIRE_EQUAL(bounded.batchCount(), 0u);
        L2D_REQUIRE_EQUAL(bounded.stats().submittedVertexCount, 0u);
    }

    void testAtlasSubmissionsCollapseIntoOneDraw()
    {
        const l2d::TextureHandle atlas = makeAtlasTexture();
        l2d::SpriteBatch2D batch;

        l2d::SpriteBatchSubmission2D left = submission(atlas, {0.f, 0.f});
        left.textureRect = {{0, 0}, {1, 1}};
        l2d::SpriteBatchSubmission2D right = submission(atlas, {1.f, 0.f});
        right.textureRect = {{1, 0}, {1, 1}};

        L2D_REQUIRE(batch.submit(left).succeeded());
        L2D_REQUIRE(batch.submit(right).succeeded());
        L2D_REQUIRE_EQUAL(batch.batchCount(), 1u);

        sf::RenderTexture target({2u, 1u});
        target.clear(sf::Color::Black);
        const l2d::SpriteBatchDrawResult2D draw = batch.draw(target);
        L2D_REQUIRE(draw.succeeded());
        L2D_REQUIRE_EQUAL(draw.completedBatchCount, 1u);
        L2D_REQUIRE_EQUAL(draw.drawCallCount, 1u);
        L2D_REQUIRE_EQUAL(draw.renderedSpriteCount, 2u);

        target.display();
        const sf::Image image = target.getTexture().copyToImage();
        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color::Red);
        L2D_REQUIRE_EQUAL(image.getPixel({1u, 0u}), sf::Color::Green);
    }

    void testStateChangesSplitWithoutReordering()
    {
        const l2d::TextureHandle red = makeSolidTexture(sf::Color::Red);
        const l2d::TextureHandle green = makeSolidTexture(sf::Color::Green);
        l2d::SpriteBatch2D batch;

        L2D_REQUIRE(batch.submit(submission(red, {0.f, 0.f})).succeeded());
        L2D_REQUIRE(batch.submit(submission(red, {1.f, 0.f})).succeeded());
        L2D_REQUIRE(batch.submit(submission(green, {2.f, 0.f})).succeeded());
        L2D_REQUIRE(batch.submit(submission(red, {3.f, 0.f})).succeeded());

        L2D_REQUIRE_EQUAL(batch.batchCount(), 3u);
        L2D_REQUIRE_EQUAL(batch.stats().textureSwitchCount, 2u);
        L2D_REQUIRE_EQUAL(batch.stats().materialSwitchCount, 0u);

        sf::RenderTexture target({4u, 1u});
        target.clear(sf::Color::Black);
        const l2d::SpriteBatchDrawResult2D draw = batch.draw(target);
        L2D_REQUIRE(draw.succeeded());
        L2D_REQUIRE_EQUAL(draw.drawCallCount, 3u);

        target.display();
        const sf::Image image = target.getTexture().copyToImage();
        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color::Red);
        L2D_REQUIRE_EQUAL(image.getPixel({1u, 0u}), sf::Color::Red);
        L2D_REQUIRE_EQUAL(image.getPixel({2u, 0u}), sf::Color::Green);
        L2D_REQUIRE_EQUAL(image.getPixel({3u, 0u}), sf::Color::Red);

        l2d::SpriteBatch2D materialBatch;
        auto material = std::make_shared<l2d::Material2D>();
        l2d::SpriteBatchSubmission2D plain = submission(red, {0.f, 0.f});
        l2d::SpriteBatchSubmission2D materialSubmission = submission(red, {1.f, 0.f});
        materialSubmission.material = material;

        L2D_REQUIRE(materialBatch.submit(plain).succeeded());
        L2D_REQUIRE(materialBatch.submit(materialSubmission).succeeded());
        materialSubmission.position = {2.f, 0.f};
        L2D_REQUIRE(materialBatch.submit(materialSubmission).succeeded());
        L2D_REQUIRE_EQUAL(materialBatch.batchCount(), 2u);
        L2D_REQUIRE_EQUAL(materialBatch.stats().textureSwitchCount, 0u);
        L2D_REQUIRE_EQUAL(materialBatch.stats().materialSwitchCount, 1u);
    }

    void testStableFailureNames()
    {
        L2D_REQUIRE_EQUAL(l2d::spriteBatchFailureName(l2d::SpriteBatchFailure2D::None),
                          std::string_view("none"));
        L2D_REQUIRE_EQUAL(
            l2d::spriteBatchFailureName(l2d::SpriteBatchFailure2D::CapacityExceeded),
            std::string_view("capacity-exceeded"));
        L2D_REQUIRE_EQUAL(
            l2d::spriteBatchFailureName(static_cast<l2d::SpriteBatchFailure2D>(255)),
            std::string_view("unknown"));
    }
}

int main()
{
    int failures = 0;
    runTest("sprite batch validates bounded submissions", testSubmissionValidationBatchingAndBounds,
            failures);
    runTest("atlas submissions collapse into one draw", testAtlasSubmissionsCollapseIntoOneDraw,
            failures);
    runTest("state changes split without reordering", testStateChangesSplitWithoutReordering,
            failures);
    runTest("sprite batch failure names are stable", testStableFailureNames, failures);

    if (failures != 0)
    {
        std::cerr << failures << " sprite batch test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D sprite batch tests passed.\n";
    return 0;
}

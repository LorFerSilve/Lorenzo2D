#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/Material2D.hpp>
#include <Lorenzo2D/Renderer/PostProcessStack2D.hpp>
#include <Lorenzo2D/Renderer/RenderSurface2D.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>

#include "TestSupport.hpp"

#include <limits>
#include <memory>

namespace
{
    using l2d::test::runTest;

    void testRenderSurfaceValidationIsCpuOnly()
    {
        l2d::RenderSurface2D surface;

        L2D_REQUIRE(!surface.ready());
        L2D_REQUIRE(surface.target() == nullptr);
        L2D_REQUIRE(surface.texture() == nullptr);
        L2D_REQUIRE_EQUAL(surface.size(), sf::Vector2u());
        L2D_REQUIRE_EQUAL(surface.allocationGeneration(), 0u);
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), 0u);

        L2D_REQUIRE(!l2d::RenderSurface2D::isValidSize({0u, 1u}));
        L2D_REQUIRE(!l2d::RenderSurface2D::isValidSize({1u, 0u}));
        L2D_REQUIRE(l2d::RenderSurface2D::isValidSize({1u, 1u}));
        L2D_REQUIRE(l2d::RenderSurface2D::isValidSize({8192u, 4096u}));
        L2D_REQUIRE(!l2d::RenderSurface2D::isValidSize({8192u, 4097u}));
        L2D_REQUIRE(!l2d::RenderSurface2D::isValidSize({8193u, 1u}));

        const l2d::RenderSurfaceConfig2D valid{{320u, 180u}, true, false};
        L2D_REQUIRE(l2d::RenderSurface2D::isValidConfig(valid));

        l2d::RenderSurfacePresent2D present;
        L2D_REQUIRE(l2d::RenderSurface2D::isValidPresent(present));

        present.position.x = std::numeric_limits<float>::quiet_NaN();
        L2D_REQUIRE(!l2d::RenderSurface2D::isValidPresent(present));

        present.position = {0.f, 0.f};
        present.size = sf::Vector2f{0.f, 10.f};
        L2D_REQUIRE(!l2d::RenderSurface2D::isValidPresent(present));

        present.size = sf::Vector2f{10.f, 10.f};
        L2D_REQUIRE(l2d::RenderSurface2D::isValidPresent(present));

        L2D_REQUIRE(!surface.create({{0u, 0u}, false, false}));
        L2D_REQUIRE(!surface.ready());
    }

    void testRenderSurfaceCreationAndFailureAreTransactional()
    {
        l2d::RenderSurface2D surface;
        const l2d::RenderSurfaceConfig2D initial{{8u, 4u}, true, false};

        L2D_REQUIRE(surface.create(initial));
        L2D_REQUIRE(surface.ready());
        L2D_REQUIRE_EQUAL(surface.size(), sf::Vector2u(8u, 4u));
        L2D_REQUIRE_EQUAL(surface.allocationGeneration(), 1u);
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), 0u);
        L2D_REQUIRE(surface.target() != nullptr);
        L2D_REQUIRE(surface.texture() != nullptr);
        L2D_REQUIRE(surface.texture()->isSmooth());
        L2D_REQUIRE(!surface.texture()->isRepeated());

        L2D_REQUIRE(surface.clear(sf::Color::Red));
        L2D_REQUIRE(surface.display());
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), 1u);

        const sf::Texture* textureBeforeFailure = surface.texture();
        const std::uint64_t allocationBeforeFailure = surface.allocationGeneration();
        const std::uint64_t contentBeforeFailure = surface.contentGeneration();

        L2D_REQUIRE(!surface.create({{0u, 4u}, false, false}));
        L2D_REQUIRE(surface.ready());
        L2D_REQUIRE(surface.texture() == textureBeforeFailure);
        L2D_REQUIRE_EQUAL(surface.size(), sf::Vector2u(8u, 4u));
        L2D_REQUIRE_EQUAL(surface.allocationGeneration(), allocationBeforeFailure);
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), contentBeforeFailure);

        const sf::Image initialImage = surface.texture()->copyToImage();
        L2D_REQUIRE_EQUAL(initialImage.getPixel({3u, 2u}), sf::Color::Red);

        const l2d::RenderSurfaceConfig2D replacement{{4u, 4u}, false, true};
        L2D_REQUIRE(surface.create(replacement));
        L2D_REQUIRE_EQUAL(surface.size(), sf::Vector2u(4u, 4u));
        L2D_REQUIRE_EQUAL(surface.allocationGeneration(), 2u);
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), 0u);
        L2D_REQUIRE(!surface.texture()->isSmooth());
        L2D_REQUIRE(surface.texture()->isRepeated());

        surface.reset();
        L2D_REQUIRE(!surface.ready());
        L2D_REQUIRE_EQUAL(surface.allocationGeneration(), 3u);
        L2D_REQUIRE_EQUAL(surface.contentGeneration(), 0u);
        L2D_REQUIRE_EQUAL(surface.size(), sf::Vector2u());
    }

    void testRenderSurfaceDrawAndDisplayPublishPixels()
    {
        l2d::RenderSurface2D surface;
        L2D_REQUIRE(surface.create({{4u, 4u}, false, false}));
        L2D_REQUIRE(surface.clear(sf::Color::Black));

        sf::RectangleShape rectangle({2.f, 4.f});
        rectangle.setFillColor(sf::Color::Blue);
        surface.target()->draw(rectangle);

        L2D_REQUIRE(surface.display());
        const sf::Image image = surface.texture()->copyToImage();

        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color::Blue);
        L2D_REQUIRE_EQUAL(image.getPixel({1u, 3u}), sf::Color::Blue);
        L2D_REQUIRE_EQUAL(image.getPixel({3u, 1u}), sf::Color::Black);
    }

    void testRenderSurfacePresentationChainsTargets()
    {
        l2d::RenderSurface2D source;
        l2d::RenderSurface2D destination;
        L2D_REQUIRE(source.create({{2u, 2u}, false, false}));
        L2D_REQUIRE(destination.create({{4u, 4u}, false, false}));

        L2D_REQUIRE(source.clear(sf::Color::Green));
        L2D_REQUIRE(source.display());
        L2D_REQUIRE(destination.clear(sf::Color::Black));

        l2d::RenderSurfacePresent2D present;
        present.position = {1.f, 1.f};
        present.size = sf::Vector2f{2.f, 2.f};
        present.material = std::make_shared<l2d::Material2D>();
        L2D_REQUIRE(present.material->setBlendMode(l2d::MaterialBlendMode2D::Alpha));

        L2D_REQUIRE(source.present(*destination.target(), present));
        L2D_REQUIRE(!source.present(*source.target(), present));

        present.position.x = std::numeric_limits<float>::infinity();
        L2D_REQUIRE(!source.present(*destination.target(), present));

        L2D_REQUIRE(destination.display());
        const sf::Image image = destination.texture()->copyToImage();

        L2D_REQUIRE_EQUAL(image.getPixel({0u, 0u}), sf::Color::Black);
        L2D_REQUIRE_EQUAL(image.getPixel({1u, 1u}), sf::Color::Green);
        L2D_REQUIRE_EQUAL(image.getPixel({2u, 2u}), sf::Color::Green);
        L2D_REQUIRE_EQUAL(image.getPixel({3u, 3u}), sf::Color::Black);
    }

    void testCameraAndPostProcessUseGenericRenderTarget()
    {
        l2d::RenderSurface2D surface;
        L2D_REQUIRE(surface.create({{8u, 8u}, false, false}));

        l2d::Camera2D camera({4.f, 4.f});
        L2D_REQUIRE(camera.trySetCenter({2.f, 2.f}));
        camera.applyTo(*surface.target());

        L2D_REQUIRE_EQUAL(surface.target()->getView().getCenter(), camera.view().getCenter());
        L2D_REQUIRE_EQUAL(surface.target()->getView().getSize(), camera.view().getSize());

        const sf::View viewBeforePost = surface.target()->getView();

        L2D_REQUIRE(surface.clear(sf::Color::White));

        l2d::PostProcessStack2D post;
        post.addPass({sf::Color::Black, l2d::PostProcessBlend2D::Alpha, true});
        post.apply(*surface.target());

        L2D_REQUIRE_EQUAL(surface.target()->getView().getCenter(), viewBeforePost.getCenter());
        L2D_REQUIRE_EQUAL(surface.target()->getView().getSize(), viewBeforePost.getSize());

        L2D_REQUIRE(surface.display());
        const sf::Image image = surface.texture()->copyToImage();
        L2D_REQUIRE_EQUAL(image.getPixel({4u, 4u}), sf::Color::Black);
    }
}

int main()
{
    int failures = 0;

    runTest("render surface validation is CPU-only", testRenderSurfaceValidationIsCpuOnly, failures);
    runTest("render surface creation and failure are transactional",
            testRenderSurfaceCreationAndFailureAreTransactional, failures);
    runTest("render surface draw and display publish pixels",
            testRenderSurfaceDrawAndDisplayPublishPixels, failures);
    runTest("render surface presentation chains targets", testRenderSurfacePresentationChainsTargets,
            failures);
    runTest("camera and post-process use generic render target",
            testCameraAndPostProcessUseGenericRenderTarget, failures);

    return failures == 0 ? 0 : 1;
}

#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/ECS/Transform.hpp>
#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/DebugOverlay.hpp>
#include <Lorenzo2D/Renderer/OrthographicCameraController2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Renderer/RenderLayerStack2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include "RendererNumeric.hpp"
#include "TestSupport.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    using l2d::test::runTest;

    constexpr float kRendererComparisonEpsilon = 0.0001f;

    bool isFinite(sf::Vector2f value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool isFinite(const sf::Transform& transform)
    {
        const float* matrix = transform.getMatrix();

        for (std::size_t index = 0; index < 16u; ++index)
        {
            if (!std::isfinite(matrix[index])) return false;
        }

        return true;
    }

    float maximumSafeViewExtent()
    {
        return std::sqrt(std::numeric_limits<float>::max()) / 4.f;
    }

    void requireSafeCamera(const l2d::Camera2D& camera)
    {
        const sf::Vector2f baseSize = camera.size();
        const sf::Vector2f viewSize = camera.view().getSize();

        L2D_REQUIRE(isFinite(camera.center()));
        L2D_REQUIRE(isFinite(baseSize));
        L2D_REQUIRE(baseSize.x > 0.f);
        L2D_REQUIRE(baseSize.y > 0.f);
        L2D_REQUIRE(std::isfinite(camera.zoom()));
        L2D_REQUIRE(camera.zoom() > 0.f);
        L2D_REQUIRE(isFinite(viewSize));
        L2D_REQUIRE(viewSize.x > 0.f);
        L2D_REQUIRE(viewSize.y > 0.f);
        L2D_REQUIRE(viewSize.x <= maximumSafeViewExtent());
        L2D_REQUIRE(viewSize.y <= maximumSafeViewExtent());
        L2D_REQUIRE_EQUAL(camera.view().getCenter(), camera.center());
        const sf::Transform& transform = camera.view().getTransform();
        const sf::Transform& inverse = camera.view().getInverseTransform();
        L2D_REQUIRE(isFinite(transform));
        L2D_REQUIRE(isFinite(inverse));

        const float* matrix = transform.getMatrix();
        const float determinant = matrix[0] * matrix[5] - matrix[4] * matrix[1];
        L2D_REQUIRE(std::isfinite(determinant));
        L2D_REQUIRE(determinant != 0.f);
    }

    class ExtremeTransformWriter final : public l2d::Component
    {
      public:
        void onUpdate(float) override
        {
            l2d::GameObject* gameObject = owner();

            if (gameObject == nullptr) return;

            const float maximum = std::numeric_limits<float>::max();
            gameObject->transform.setPosition({-maximum, maximum});
            gameObject->transform.setRotation(-maximum);
            gameObject->transform.setScale({-maximum, maximum});
        }
    };

    void testTransformRejectsInvalidAndOverflowingMutations()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const float maximum = std::numeric_limits<float>::max();

        const l2d::Transform invalidConstructor({nan, 5.f});
        L2D_REQUIRE_EQUAL(invalidConstructor.position(), sf::Vector2f(0.f, 0.f));
        const l2d::Transform infiniteConstructor({5.f, infinity});
        L2D_REQUIRE_EQUAL(infiniteConstructor.position(), sf::Vector2f(0.f, 0.f));

        l2d::Transform transform;
        transform.setPosition({12.f, -34.f});
        const sf::Vector2f validPosition = transform.position();

        transform.setPosition({nan, 1.f});
        L2D_REQUIRE_EQUAL(transform.position(), validPosition);
        transform.setPosition({1.f, infinity});
        L2D_REQUIRE_EQUAL(transform.position(), validPosition);
        transform.move({-infinity, 1.f});
        L2D_REQUIRE_EQUAL(transform.position(), validPosition);

        transform.setPosition({maximum, 0.f});
        transform.move({maximum, 0.f});
        L2D_REQUIRE_EQUAL(transform.position(), sf::Vector2f(maximum, 0.f));

        transform.setRotation(45.f);
        transform.setRotation(nan);
        L2D_REQUIRE_EQUAL(transform.rotation(), 45.f);
        transform.rotate(infinity);
        L2D_REQUIRE_EQUAL(transform.rotation(), 45.f);
        transform.setRotation(maximum);
        transform.rotate(maximum);
        L2D_REQUIRE_EQUAL(transform.rotation(), maximum);

        transform.setScale({-2.f, 3.f});
        const sf::Vector2f validScale = transform.scale();
        transform.setScale({nan, 1.f});
        L2D_REQUIRE_EQUAL(transform.scale(), validScale);
        transform.setScale({1.f, -infinity});
        L2D_REQUIRE_EQUAL(transform.scale(), validScale);
    }

    void testTransformInterpolatesExtremeFiniteSnapshotsSafely()
    {
        const float maximum = std::numeric_limits<float>::max();

        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Extreme transform");
        object.transform.setPosition({maximum, -maximum});
        object.transform.setRotation(maximum);
        object.transform.setScale({maximum, -maximum});
        object.addComponent<ExtremeTransformWriter>();

        scene.fixedUpdate(1.f);

        const l2d::TransformState halfway = object.transform.interpolated(0.5f);

        L2D_REQUIRE(isFinite(halfway.position));
        L2D_REQUIRE(std::isfinite(halfway.rotation));
        L2D_REQUIRE(isFinite(halfway.scale));
        L2D_REQUIRE_EQUAL(halfway.position, sf::Vector2f(0.f, 0.f));
        L2D_REQUIRE_EQUAL(halfway.scale, sf::Vector2f(0.f, 0.f));
    }

    void testCameraMaintainsSafeViewDimensions()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const float denormal = std::numeric_limits<float>::denorm_min();
        const float maximum = std::numeric_limits<float>::max();

        const std::array<sf::Vector2f, 7> constructorSizes = {{{0.f, 180.f},
                                                               {-320.f, 180.f},
                                                               {nan, 180.f},
                                                               {320.f, infinity},
                                                               {320.f, -infinity},
                                                               {denormal, denormal},
                                                               {maximum, maximum}}};

        for (sf::Vector2f size : constructorSizes)
        {
            const l2d::Camera2D camera(size);
            requireSafeCamera(camera);
        }

        l2d::Camera2D camera({320.f, 180.f});
        const std::array<sf::Vector2f, 7> assignedSizes = {{{0.f, 180.f},
                                                            {320.f, -1.f},
                                                            {nan, 180.f},
                                                            {320.f, infinity},
                                                            {320.f, -infinity},
                                                            {denormal, denormal},
                                                            {maximum, maximum}}};

        for (sf::Vector2f size : assignedSizes)
        {
            camera.setSize(size);
            requireSafeCamera(camera);
        }

        camera.setZoom(1.f);
        camera.setSize({0.f, 0.005f});
        L2D_REQUIRE_EQUAL(camera.size(), sf::Vector2f(0.01f, 0.01f));
        camera.setSize({nan, infinity});
        L2D_REQUIRE_EQUAL(camera.size(), sf::Vector2f(0.01f, 0.01f));

        camera.setSize({320.f, 180.f});

        const std::array<float, 8> zooms = {0.f,       -1.f,     nan,     infinity,
                                            -infinity, denormal, maximum, 1.f};

        for (float zoom : zooms)
        {
            camera.setZoom(zoom);
            requireSafeCamera(camera);
        }

        const std::array<float, 6> minimumZoomInputs = {0.f,      -1.f,      nan,
                                                        infinity, -infinity, denormal};

        for (float zoom : minimumZoomInputs)
        {
            camera.setZoom(zoom);
            L2D_REQUIRE_EQUAL(camera.zoom(), 0.01f);
        }

        camera.setSize({maximum, maximum});
        camera.setZoom(maximum);
        requireSafeCamera(camera);
    }

    void testCameraCoordinatesBoundsAndFollowAreTransactional()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const float maximum = std::numeric_limits<float>::max();

        l2d::Camera2D camera({100.f, 100.f});
        camera.setCenter({25.f, 30.f});
        const sf::Vector2f validCenter = camera.center();

        camera.setCenter({nan, 10.f});
        L2D_REQUIRE_EQUAL(camera.center(), validCenter);
        camera.setCenter({10.f, infinity});
        L2D_REQUIRE_EQUAL(camera.center(), validCenter);
        camera.move({-infinity, 1.f});
        L2D_REQUIRE_EQUAL(camera.center(), validCenter);

        camera.setCenter({maximum, 0.f});
        L2D_REQUIRE_EQUAL(camera.center(), validCenter);

        const float safeExtreme = maximumSafeViewExtent() * 0.75f;
        camera.setCenter({safeExtreme, 0.f});
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(safeExtreme, 0.f));
        camera.move({safeExtreme, 0.f});
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(safeExtreme, 0.f));
        camera.setCenter(validCenter);

        l2d::Camera2D unbounded({10.f, 10.f});
        unbounded.setBounds({nan, 0.f}, {10.f, 10.f});
        L2D_REQUIRE(!unbounded.hasBounds());

        camera.setBounds({100.f, 80.f}, {0.f, 0.f});
        L2D_REQUIRE(camera.hasBounds());
        L2D_REQUIRE_EQUAL(camera.boundsMin(), sf::Vector2f(0.f, 0.f));
        L2D_REQUIRE_EQUAL(camera.boundsMax(), sf::Vector2f(100.f, 80.f));

        const sf::Vector2f validBoundsMin = camera.boundsMin();
        const sf::Vector2f validBoundsMax = camera.boundsMax();
        const sf::Vector2f boundedCenter = camera.center();
        camera.setBounds({0.f, nan}, {50.f, 50.f});
        L2D_REQUIRE_EQUAL(camera.boundsMin(), validBoundsMin);
        L2D_REQUIRE_EQUAL(camera.boundsMax(), validBoundsMax);
        L2D_REQUIRE_EQUAL(camera.center(), boundedCenter);
        camera.setBounds({0.f, 0.f}, {maximum, 50.f});
        L2D_REQUIRE_EQUAL(camera.boundsMin(), validBoundsMin);
        L2D_REQUIRE_EQUAL(camera.boundsMax(), validBoundsMax);
        L2D_REQUIRE_EQUAL(camera.center(), boundedCenter);

        camera.setBounds({0.f, 0.f}, {10.f, 10.f});
        L2D_REQUIRE_APPROX(camera.center(), (sf::Vector2f{5.f, 5.f}), kRendererComparisonEpsilon);
        camera.clearBounds();

        camera.setCenter({0.f, 0.f});
        camera.setFollowSmoothness(2.f);

        const std::array<float, 4> invalidDeltas = {0.f, -1.f, nan, infinity};

        for (float deltaTime : invalidDeltas)
        {
            camera.follow({100.f, 50.f}, deltaTime);
            L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));
        }

        camera.follow({nan, 50.f}, 1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));
        camera.follow({50.f, -infinity}, 1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));
        camera.follow({maximum, 50.f}, 1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));

        camera.setFollowSmoothness(nan);
        L2D_REQUIRE_EQUAL(camera.followSmoothness(), 0.f);
        camera.setFollowSmoothness(infinity);
        L2D_REQUIRE_EQUAL(camera.followSmoothness(), 0.f);
        camera.setFollowSmoothness(-1.f);
        L2D_REQUIRE_EQUAL(camera.followSmoothness(), 0.f);
        camera.follow({10.f, 20.f}, 0.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));

        camera.setFollowSmoothness(1.f);
        camera.follow({1.f, 0.f}, 0.00000001f);
        L2D_REQUIRE(camera.center().x > 0.f);
        L2D_REQUIRE(camera.center().x < 0.000001f);
        L2D_REQUIRE_EQUAL(camera.center().y, 0.f);

        const float extreme = maximumSafeViewExtent() * 0.75f;
        camera.setCenter({extreme, -extreme});
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(extreme, -extreme));
        camera.setFollowSmoothness(std::log(2.f));
        camera.follow({-extreme, extreme}, 1.f);
        L2D_REQUIRE(isFinite(camera.center()));
        L2D_REQUIRE(std::fabs(camera.center().x) < extreme);
        L2D_REQUIRE(std::fabs(camera.center().y) < extreme);
        requireSafeCamera(camera);
    }

    void testCameraControllerSanitizesTargetsAndZoomConfiguration()
    {
        static_assert(!std::is_copy_constructible<l2d::OrthographicCameraController2D>::value);
        static_assert(!std::is_copy_assignable<l2d::OrthographicCameraController2D>::value);
        static_assert(!std::is_move_constructible<l2d::OrthographicCameraController2D>::value);
        static_assert(!std::is_move_assignable<l2d::OrthographicCameraController2D>::value);

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::Camera2D camera({100.f, 100.f});
        camera.setCenter({0.f, 0.f});
        l2d::OrthographicCameraController2D controller(camera);

        L2D_REQUIRE_APPROX(controller.zoomInFactor(), 0.90f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.zoomOutFactor(), 1.10f, kRendererComparisonEpsilon);

        controller.setZoomStepFactors(0.75f, 1.25f);
        L2D_REQUIRE_APPROX(controller.zoomInFactor(), 0.75f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.zoomOutFactor(), 1.25f, kRendererComparisonEpsilon);

        controller.setZoomStepFactors(2.f, 0.5f);
        L2D_REQUIRE_APPROX(controller.zoomInFactor(), 2.f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.zoomOutFactor(), 0.5f, kRendererComparisonEpsilon);

        controller.setZoomStepFactors(nan, infinity);
        L2D_REQUIRE_APPROX(controller.zoomInFactor(), 0.90f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.zoomOutFactor(), 1.10f, kRendererComparisonEpsilon);
        controller.setZoomStepFactors(0.f, -1.f);
        L2D_REQUIRE_APPROX(controller.zoomInFactor(), 0.90f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.zoomOutFactor(), 1.10f, kRendererComparisonEpsilon);

        controller.setZoomLimits(2.f, 0.5f);
        L2D_REQUIRE_APPROX(controller.minZoom(), 0.5f, kRendererComparisonEpsilon);
        L2D_REQUIRE_APPROX(controller.maxZoom(), 2.f, kRendererComparisonEpsilon);

        controller.setZoomLimits(nan, infinity);
        L2D_REQUIRE(std::isfinite(controller.minZoom()));
        L2D_REQUIRE(std::isfinite(controller.maxZoom()));
        L2D_REQUIRE(controller.minZoom() > 0.f);
        L2D_REQUIRE(controller.maxZoom() >= controller.minZoom());
        L2D_REQUIRE(camera.zoom() >= controller.minZoom());
        L2D_REQUIRE(camera.zoom() <= controller.maxZoom());

        const float maximum = std::numeric_limits<float>::max();
        controller.setZoomLimits(maximum, maximum);
        L2D_REQUIRE(std::isfinite(controller.minZoom()));
        L2D_REQUIRE_EQUAL(controller.minZoom(), controller.maxZoom());
        L2D_REQUIRE(controller.maxZoom() <= maximumSafeViewExtent() / camera.size().x);
        L2D_REQUIRE(camera.zoom() >= controller.minZoom());
        L2D_REQUIRE(camera.zoom() <= controller.maxZoom());
        requireSafeCamera(camera);

        controller.setFollowTarget({nan, 10.f});
        L2D_REQUIRE(!controller.hasFollowTarget());
        controller.setFollowTarget({40.f, 20.f});
        L2D_REQUIRE(controller.hasFollowTarget());
        controller.setFollowTarget({infinity, 10.f});

        camera.setFollowSmoothness(0.f);
        controller.update(1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(40.f, 20.f));

        camera.setCenter({0.f, 0.f});
        camera.setFollowSmoothness(2.f);
        controller.update(nan);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));
        controller.update(-1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));

        controller.setFollowEnabled(false);
        controller.update(1.f);
        L2D_REQUIRE_EQUAL(camera.center(), sf::Vector2f(0.f, 0.f));
        controller.clearFollowTarget();
        L2D_REQUIRE(!controller.hasFollowTarget());
    }

    void testShapeRenderersSanitizeGeometry()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::CircleRenderer circle(-10.f, sf::Color::Red);
        L2D_REQUIRE_EQUAL(circle.radius(), 0.f);
        L2D_REQUIRE_EQUAL(circle.fillColor(), sf::Color::Red);
        circle.setRadius(nan);
        L2D_REQUIRE_EQUAL(circle.radius(), 0.f);
        circle.setRadius(infinity);
        L2D_REQUIRE_EQUAL(circle.radius(), 0.f);
        circle.setRadius(std::numeric_limits<float>::max());
        L2D_REQUIRE_EQUAL(circle.radius(), std::numeric_limits<float>::max() / 4.f);
        circle.setRadius(12.5f);
        L2D_REQUIRE_APPROX(circle.radius(), 12.5f, kRendererComparisonEpsilon);

        l2d::RectangleRenderer rectangle({-10.f, nan}, sf::Color::Green);
        L2D_REQUIRE_EQUAL(rectangle.size(), sf::Vector2f(0.f, 0.f));
        L2D_REQUIRE_EQUAL(rectangle.fillColor(), sf::Color::Green);
        rectangle.setSize({infinity, 25.f});
        L2D_REQUIRE_EQUAL(rectangle.size(), sf::Vector2f(0.f, 25.f));
        rectangle.setSize({30.f, -infinity});
        L2D_REQUIRE_EQUAL(rectangle.size(), sf::Vector2f(30.f, 0.f));
        rectangle.setSize({15.f, 20.f});
        L2D_REQUIRE_EQUAL(rectangle.size(), sf::Vector2f(15.f, 20.f));
    }

    void testDerivedDrawableBoundsStayInTheSafeDomain()
    {
        const float maximum = std::numeric_limits<float>::max();
        const float safeExtent = maximumSafeViewExtent();
        const sf::FloatRect localBounds({0.f, 0.f}, {20.f, 10.f});

        l2d::TransformState state = {{100.f, -50.f}, 45.f, {2.f, -3.f}};
        L2D_REQUIRE(l2d::renderer_detail::hasSafeTransformedBounds(localBounds, state));

        state.rotation = maximum;
        L2D_REQUIRE(l2d::renderer_detail::hasSafeTransformedBounds(localBounds, state));
        L2D_REQUIRE(std::isfinite(l2d::renderer_detail::normalizedRotationDegrees(state.rotation)));

        state.position = {maximum, 0.f};
        L2D_REQUIRE(!l2d::renderer_detail::hasSafeTransformedBounds(localBounds, state));

        state = {{0.f, 0.f}, 0.f, {maximum, 1.f}};
        const sf::FloatRect spriteLikeBounds({0.f, 0.f}, {2.f, 1.f});
        L2D_REQUIRE(!l2d::renderer_detail::hasSafeTransformedBounds(spriteLikeBounds, state));

        state = {{0.f, 0.f}, 45.f, {1.f, 1.f}};
        const sf::FloatRect rotatedExtremeBounds({0.f, 0.f},
                                                 {safeExtent * 0.75f, safeExtent * 0.75f});
        L2D_REQUIRE(!l2d::renderer_detail::hasSafeTransformedBounds(rotatedExtremeBounds, state));

        L2D_REQUIRE(
            l2d::renderer_detail::hasSafeAxisAlignedBounds({10.f, 20.f}, {30.f, 40.f}, 2.f));
        L2D_REQUIRE(!l2d::renderer_detail::hasSafeAxisAlignedBounds({safeExtent * 0.75f, 0.f},
                                                                    {safeExtent * 0.5f, 1.f}, 0.f));
        L2D_REQUIRE(
            !l2d::renderer_detail::hasSafeAxisAlignedBounds({0.f, 0.f}, {maximum, 1.f}, 1.f));
    }

    void testDebugOverlaySanitizesLayoutValues()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::DebugOverlay overlay;
        L2D_REQUIRE_EQUAL(overlay.position(), sf::Vector2f(10.f, 10.f));
        L2D_REQUIRE_EQUAL(overlay.characterSize(), 18u);

        overlay.setPosition({20.f, 30.f});
        L2D_REQUIRE_EQUAL(overlay.position(), sf::Vector2f(20.f, 30.f));
        overlay.setPosition({nan, 40.f});
        L2D_REQUIRE_EQUAL(overlay.position(), sf::Vector2f(20.f, 30.f));
        overlay.setPosition({40.f, infinity});
        L2D_REQUIRE_EQUAL(overlay.position(), sf::Vector2f(20.f, 30.f));

        overlay.setCharacterSize(0u);
        L2D_REQUIRE_EQUAL(overlay.characterSize(), 1u);
        overlay.setCharacterSize(24u);
        L2D_REQUIRE_EQUAL(overlay.characterSize(), 24u);
    }

    void testPhysicsDebugRendererSanitizesConfiguration()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();

        l2d::PhysicsDebugRenderer2D renderer;
        L2D_REQUIRE(renderer.isEnabled());
        L2D_REQUIRE_APPROX(renderer.outlineThickness(), 2.f, kRendererComparisonEpsilon);

        renderer.setOutlineThickness(-1.f);
        L2D_REQUIRE_EQUAL(renderer.outlineThickness(), 0.f);
        renderer.setOutlineThickness(nan);
        L2D_REQUIRE_EQUAL(renderer.outlineThickness(), 0.f);
        renderer.setOutlineThickness(infinity);
        L2D_REQUIRE_EQUAL(renderer.outlineThickness(), 0.f);
        renderer.setOutlineThickness(3.5f);
        L2D_REQUIRE_APPROX(renderer.outlineThickness(), 3.5f, kRendererComparisonEpsilon);

        renderer.setEnabled(false);
        L2D_REQUIRE(!renderer.isEnabled());
        renderer.setDefaultColor(sf::Color::Blue);
        renderer.setCollidingColor(sf::Color::Magenta);
        renderer.setSensorColor(sf::Color::Yellow);
        L2D_REQUIRE_EQUAL(renderer.defaultColor(), sf::Color::Blue);
        L2D_REQUIRE_EQUAL(renderer.collidingColor(), sf::Color::Magenta);
        L2D_REQUIRE_EQUAL(renderer.sensorColor(), sf::Color::Yellow);
    }

    void testRenderLayerStackRejectsInvalidLayers()
    {
        l2d::RenderLayerStack2D layers;
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::World));
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug));
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::UI));

        const auto beforeCount = l2d::RenderLayer2D::Count;
        const auto negative = static_cast<l2d::RenderLayer2D>(-1);
        const auto tooLarge = static_cast<l2d::RenderLayer2D>(999);
        L2D_REQUIRE(!layers.isLayerEnabled(beforeCount));
        L2D_REQUIRE(!layers.isLayerEnabled(negative));
        L2D_REQUIRE(!layers.isLayerEnabled(tooLarge));

        layers.disableAll();
        layers.setLayerEnabled(beforeCount, true);
        layers.setLayerEnabled(negative, true);
        layers.setLayerEnabled(tooLarge, true);
        L2D_REQUIRE(!layers.isLayerEnabled(l2d::RenderLayer2D::World));
        L2D_REQUIRE(!layers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug));
        L2D_REQUIRE(!layers.isLayerEnabled(l2d::RenderLayer2D::UI));

        layers.enableAll();
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::World));
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug));
        L2D_REQUIRE(layers.isLayerEnabled(l2d::RenderLayer2D::UI));
    }

}

int main()
{
    int failures = 0;

    runTest("transform rejects invalid and overflowing mutations",
            testTransformRejectsInvalidAndOverflowingMutations, failures);
    runTest("transform interpolates extreme finite snapshots safely",
            testTransformInterpolatesExtremeFiniteSnapshotsSafely, failures);
    runTest("camera maintains safe view dimensions", testCameraMaintainsSafeViewDimensions,
            failures);
    runTest("camera coordinates, bounds, and follow are transactional",
            testCameraCoordinatesBoundsAndFollowAreTransactional, failures);
    runTest("camera controller sanitizes targets and zoom configuration",
            testCameraControllerSanitizesTargetsAndZoomConfiguration, failures);
    runTest("shape renderers sanitize geometry", testShapeRenderersSanitizeGeometry, failures);
    runTest("derived drawable bounds stay in the safe domain",
            testDerivedDrawableBoundsStayInTheSafeDomain, failures);
    runTest("debug overlay sanitizes layout values", testDebugOverlaySanitizesLayoutValues,
            failures);
    runTest("physics debug renderer sanitizes configuration",
            testPhysicsDebugRendererSanitizesConfiguration, failures);
    runTest("render layer stack rejects invalid layers", testRenderLayerStackRejectsInvalidLayers,
            failures);

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D renderer tests passed.\n";
    return 0;
}

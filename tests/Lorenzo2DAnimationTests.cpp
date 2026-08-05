#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Animation/Animator.hpp>
#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/Texture.hpp>

#include <limits>
#include <memory>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

    l2d::TextureHandle makeStripTexture()
    {
        return l2d::TextureHandle(std::make_shared<sf::Texture>(sf::Vector2u{96u, 32u}));
    }

    l2d::AnimationClip makeLoopingClip()
    {
        l2d::AnimationClip clip("run");
        L2D_REQUIRE(clip.addGridFrames({0, 0}, {32, 32}, 3u, 0.1f));
        return clip;
    }

    void testAnimationClipBuildsValidatedAtlasFrames()
    {
        l2d::AnimationClip clip = makeLoopingClip();
        L2D_REQUIRE(clip.name() == "run");
        L2D_REQUIRE(clip.isLooping());
        L2D_REQUIRE(clip.frameCount() == 3u);
        L2D_REQUIRE(clip.frames()[0].textureRect == sf::IntRect({0, 0}, {32, 32}));
        L2D_REQUIRE(clip.frames()[1].textureRect == sf::IntRect({32, 0}, {32, 32}));
        L2D_REQUIRE(clip.frames()[2].textureRect == sf::IntRect({64, 0}, {32, 32}));
        L2D_REQUIRE_APPROX(clip.duration(), 0.3f, 0.0001f);

        const std::size_t originalCount = clip.frameCount();
        L2D_REQUIRE(!clip.addFrame({{0, 0}, {0, 32}}, 0.1f));
        L2D_REQUIRE(!clip.addFrame({{0, 0}, {32, 32}}, 0.f));
        L2D_REQUIRE(!clip.addGridFrames({0, 0}, {0, 32}, 3u));
        L2D_REQUIRE(!clip.addGridFrames({-1, 0}, {32, 32}, 3u));
        L2D_REQUIRE(clip.frameCount() == originalCount);
    }

    void testAnimatorDrivesSpriteFramesAndLoops()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("Runner");
        l2d::SpriteRenderer& renderer =
            object.addComponent<l2d::SpriteRenderer>(makeStripTexture());
        l2d::Animator& animator = object.addComponent<l2d::Animator>();

        L2D_REQUIRE(animator.addClip(makeLoopingClip()));
        L2D_REQUIRE(animator.play("run"));
        L2D_REQUIRE(renderer.textureRect() == sf::IntRect({0, 0}, {32, 32}));

        scene.fixedUpdate(0.11f);
        L2D_REQUIRE(animator.currentFrameIndex() == 1u);
        L2D_REQUIRE(renderer.textureRect() == sf::IntRect({32, 0}, {32, 32}));

        scene.fixedUpdate(0.2f);
        L2D_REQUIRE(animator.currentFrameIndex() == 0u);
        L2D_REQUIRE(renderer.textureRect() == sf::IntRect({0, 0}, {32, 32}));

        scene.fixedUpdate(1000000.f);
        L2D_REQUIRE(animator.currentFrameIndex() < 3u);
        L2D_REQUIRE(animator.isPlaying());
        L2D_REQUIRE(!animator.isFinished());
    }

    void testAnimatorPauseSpeedAndNonLoopingCompletion()
    {
        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("One shot");
        object.addComponent<l2d::SpriteRenderer>(makeStripTexture());
        l2d::Animator& animator = object.addComponent<l2d::Animator>();

        l2d::AnimationClip clip("hit", false);
        L2D_REQUIRE(clip.addGridFrames({0, 0}, {32, 32}, 2u, 0.1f));
        L2D_REQUIRE(animator.addClip(std::move(clip)));
        L2D_REQUIRE(!animator.play("missing"));
        L2D_REQUIRE(animator.play("hit"));

        animator.pause();
        scene.fixedUpdate(1.f);
        L2D_REQUIRE(animator.currentFrameIndex() == 0u);

        L2D_REQUIRE(!animator.setPlaybackSpeed(-1.f));
        L2D_REQUIRE(!animator.setPlaybackSpeed(std::numeric_limits<float>::infinity()));
        L2D_REQUIRE(animator.setPlaybackSpeed(2.f));
        L2D_REQUIRE(animator.play("hit"));
        scene.fixedUpdate(0.1f);
        L2D_REQUIRE(!animator.isPlaying());
        L2D_REQUIRE(animator.isFinished());
        L2D_REQUIRE(animator.currentFrameIndex() == 1u);

        L2D_REQUIRE(animator.play("hit"));
        L2D_REQUIRE(animator.currentFrameIndex() == 0u);
        animator.stop();
        L2D_REQUIRE(!animator.isPlaying());
        L2D_REQUIRE(!animator.isFinished());
    }
}

int main()
{
    int failures = 0;
    runTest("animation clips build validated atlas frames",
            testAnimationClipBuildsValidatedAtlasFrames, failures);
    runTest("animator drives and loops sprite frames", testAnimatorDrivesSpriteFramesAndLoops,
            failures);
    runTest("animator handles pause, speed, and completion",
            testAnimatorPauseSpeedAndNonLoopingCompletion, failures);

    if (failures != 0)
    {
        std::cerr << failures << " animation test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D animation tests passed.\n";
    return 0;
}

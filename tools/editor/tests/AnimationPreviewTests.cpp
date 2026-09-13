#include <Lorenzo2DEditor/AnimationPreviewModel.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    bool near(float lhs, float rhs)
    {
        return std::abs(lhs - rhs) < 0.0001f;
    }

    l2d::LevelDocument makeLevel()
    {
        l2d::Prefab animated;
        animated.name = "Animated";
        animated.spriteRenderer = l2d::SpriteRendererPrefab{};
        animated.spriteRenderer->texture = "textures/hero.png";
        animated.animator = l2d::AnimatorPrefab{};
        animated.animator->clips = {"animations/idle", "animations/run"};
        animated.animator->initialClip = "animations/idle";
        animated.animator->playbackSpeed = 1.f;
        animated.animator->playing = true;

        l2d::Prefab plain;
        plain.name = "Plain";

        l2d::LevelDocument level;
        level.name = "Animation preview";
        level.objects = {animated, plain};
        return level;
    }

    l2d::AnimationClip makeLoopingClip()
    {
        l2d::AnimationClip clip("idle", true);
        require(clip.addFrame(sf::IntRect{{0, 0}, {16, 16}}, 0.1f),
                "failed to add preview frame 0");
        require(clip.addFrame(sf::IntRect{{16, 0}, {16, 16}}, 0.2f),
                "failed to add preview frame 1");
        require(clip.addFrame(sf::IntRect{{32, 0}, {16, 16}}, 0.3f),
                "failed to add preview frame 2");
        return clip;
    }

    l2d::AnimationClip makeFiniteClip()
    {
        l2d::AnimationClip clip("run", false);
        require(clip.addFrame(sf::IntRect{{0, 16}, {16, 16}}, 0.25f),
                "failed to add finite frame 0");
        require(clip.addFrame(sf::IntRect{{16, 16}, {16, 16}}, 0.25f),
                "failed to add finite frame 1");
        return clip;
    }

    void testAnimatorSelectionAndBinding()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "animation preview document setup failed");
        require(document.selectObject(1u), "animation preview selection failed");
        l2d_editor::AnimationPreviewModel model(document);

        const auto assets = model.availableClipAssets();
        require(assets.size() == 2u && assets[0] == "animations/idle" &&
                    assets[1] == "animations/run",
                "animator clip assets are not exposed in serialized order");
        require(model.initialClipAsset() == std::optional<l2d::AssetId>("animations/idle"),
                "initial animator clip asset is incorrect");
        require(!model.bindClip("animations/missing", makeLoopingClip()),
                "unreferenced clip asset was accepted");
        require(model.bindInitialClip(makeLoopingClip()), "initial clip binding failed");

        const auto snapshot = model.snapshot();
        require(snapshot.has_value(), "bound preview snapshot is missing");
        require(snapshot->objectId == 1u && snapshot->clipAsset == "animations/idle",
                "preview binding identity is incorrect");
        require(snapshot->frameIndex == 0u && snapshot->frameCount == 3u,
                "preview did not start on frame zero");
        require(snapshot->playing, "animator playing default was not inherited");
    }

    void testDeterministicPlaybackSeekAndSpeed()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "playback document setup failed");
        require(document.selectObject(1u), "playback selection failed");
        l2d_editor::AnimationPreviewModel model(document);
        require(model.bindInitialClip(makeLoopingClip()), "playback clip binding failed");

        model.update(0.15f);
        auto snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 1u && near(snapshot->frameElapsed, 0.05f),
                "preview timing did not advance deterministically into frame 1");

        require(model.setPlaybackSpeed(2.f), "valid preview speed was rejected");
        model.update(0.1f);
        snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 2u && near(snapshot->clipElapsed, 0.35f),
                "preview playback speed did not scale elapsed time");
        require(!model.setPlaybackSpeed(-1.f) &&
                    !model.setPlaybackSpeed(
                        l2d_editor::AnimationPreviewModel::MaximumPlaybackSpeed + 0.1f),
                "invalid preview speed was accepted");

        require(model.seek(0.59f), "valid preview seek failed");
        snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 2u && near(snapshot->frameElapsed, 0.29f),
                "preview seek resolved the wrong frame cursor");
        require(model.stepFrame(1), "looping frame step failed");
        snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 0u && !snapshot->playing,
                "looping frame step did not wrap and pause deterministically");
        require(model.stepFrame(-1), "reverse looping frame step failed");
        snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 2u,
                "reverse looping frame step did not wrap to the final frame");
    }

    void testFiniteCompletionAndSelectionInvalidation()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "finite preview document setup failed");
        require(document.selectObject(1u), "finite preview selection failed");
        l2d_editor::AnimationPreviewModel model(document);
        require(model.bindClip("animations/run", makeFiniteClip()), "finite clip binding failed");
        require(model.play(), "finite preview play failed");

        model.update(2.f);
        auto snapshot = model.snapshot();
        require(snapshot && snapshot->finished && !snapshot->playing,
                "non-looping preview did not stop at completion");
        require(snapshot->frameIndex == 1u && near(snapshot->clipElapsed, 0.5f),
                "non-looping preview did not clamp to its final frame");

        require(model.play(), "replaying a finished preview failed");
        snapshot = model.snapshot();
        require(snapshot && snapshot->frameIndex == 0u && near(snapshot->clipElapsed, 0.f),
                "replaying a finished preview did not restart from frame zero");

        require(document.selectObject(2u), "selection invalidation setup failed");
        require(!model.snapshot().has_value(),
                "preview survived selection change to an object without the bound animator");
        model.update(0.1f);
        require(!model.hasClip(), "stale preview binding was not cleared on update");
    }

    void testEmptyAndOverLimitClipsAreRejected()
    {
        l2d_editor::EditorDocument document;
        require(document.replace(makeLevel()), "preview limit document setup failed");
        require(document.selectObject(1u), "preview limit selection failed");
        l2d_editor::AnimationPreviewModel model(document);

        l2d::AnimationClip empty("empty", true);
        require(!model.bindClip("animations/idle", std::move(empty)),
                "empty preview clip was accepted");

        l2d::AnimationClip oversized("oversized", true);
        for (std::size_t i = 0u; i < l2d_editor::AnimationPreviewModel::MaximumPreviewFrames + 1u;
             ++i)
            require(oversized.addFrame(sf::IntRect{{0, 0}, {1, 1}}, 0.01f),
                    "failed to construct over-limit preview clip");
        require(!model.bindClip("animations/idle", std::move(oversized)),
                "over-limit preview clip was accepted");
        require(!model.hasClip(), "rejected preview clip polluted prior empty state");
    }
}

int main()
{
    try
    {
        testAnimatorSelectionAndBinding();
        testDeterministicPlaybackSeekAndSpeed();
        testFiniteCompletionAndSelectionInvalidation();
        testEmptyAndOverLimitClipsAreRejected();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Animation preview regression failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "Animation preview regressions passed.\n";
    return 0;
}

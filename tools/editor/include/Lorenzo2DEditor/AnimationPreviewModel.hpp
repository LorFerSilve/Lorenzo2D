#pragma once

#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/AssetId.hpp>

#include <SFML/Graphics/Rect.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace l2d_editor
{
    struct AnimationPreviewSnapshot
    {
        EditorObjectId objectId = InvalidEditorObjectId;
        l2d::AssetId clipAsset;
        std::string clipName;
        sf::IntRect textureRect;
        std::size_t frameIndex = 0u;
        std::size_t frameCount = 0u;
        float frameElapsed = 0.f;
        float clipElapsed = 0.f;
        float clipDuration = 0.f;
        float playbackSpeed = 1.f;
        bool looping = false;
        bool playing = false;
        bool finished = false;
    };

    // Editor-only Phase 13.7 animation preview state. The model borrows only the
    // EditorDocument; bound clips are copied so preview lifetime never depends on
    // runtime asset-manager ownership. A clip may be bound only when the currently
    // selected AnimatorPrefab references its AssetId.
    class AnimationPreviewModel
    {
      public:
        static constexpr std::size_t MaximumPreviewFrames = 4096u;
        static constexpr float MaximumPlaybackSpeed = 16.f;

        explicit AnimationPreviewModel(EditorDocument& document) noexcept;

        [[nodiscard]] std::vector<l2d::AssetId> availableClipAssets() const;
        [[nodiscard]] std::optional<l2d::AssetId> initialClipAsset() const;

        [[nodiscard]] bool bindClip(l2d::AssetId asset, l2d::AnimationClip clip);
        [[nodiscard]] bool bindInitialClip(l2d::AnimationClip clip);
        void clear() noexcept;

        [[nodiscard]] bool hasClip() const noexcept;
        [[nodiscard]] bool play();
        void pause() noexcept;
        void stop() noexcept;
        [[nodiscard]] bool seek(float seconds);
        [[nodiscard]] bool stepFrame(int offset);
        [[nodiscard]] bool setPlaybackSpeed(float speed) noexcept;
        [[nodiscard]] float playbackSpeed() const noexcept;
        void update(float deltaTime) noexcept;

        [[nodiscard]] std::optional<AnimationPreviewSnapshot> snapshot() const;

      private:
        [[nodiscard]] const EditorObjectRecord* selectedAnimatorObject() const noexcept;
        [[nodiscard]] bool selectedAnimatorReferences(const l2d::AssetId& asset) const noexcept;
        [[nodiscard]] bool bindingStillValid() const noexcept;
        void resetPlayback() noexcept;
        void resolveCursor() noexcept;

        EditorDocument* m_document = nullptr;
        EditorObjectId m_objectId = InvalidEditorObjectId;
        l2d::AssetId m_clipAsset;
        std::optional<l2d::AnimationClip> m_clip;
        double m_elapsed = 0.0;
        float m_playbackSpeed = 1.f;
        std::size_t m_frameIndex = 0u;
        double m_frameElapsed = 0.0;
        bool m_playing = false;
        bool m_finished = false;
    };
}

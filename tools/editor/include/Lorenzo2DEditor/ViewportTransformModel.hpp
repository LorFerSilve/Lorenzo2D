#pragma once

#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <optional>

namespace l2d_editor
{
    struct EditorViewportRect
    {
        sf::Vector2f position{0.f, 0.f};
        sf::Vector2f size{1.f, 1.f};
    };

    enum class ViewportTransformDragKind : std::uint8_t
    {
        None,
        Translation,
        Rotation,
        ScaleX,
        ScaleY,
        ScaleUniform
    };

    enum class ViewportScaleHandle : std::uint8_t
    {
        X,
        Y,
        Uniform
    };

    struct ViewportTransformSnapshot
    {
        EditorObjectId objectId = InvalidEditorObjectId;
        l2d::TransformState transform;
        sf::Vector2f gizmoPosition{0.f, 0.f};
        sf::Vector2f rotationHandlePosition{0.f, 0.f};
        sf::Vector2f scaleXHandlePosition{0.f, 0.f};
        sf::Vector2f scaleYHandlePosition{0.f, 0.f};
        sf::Vector2f scaleUniformHandlePosition{0.f, 0.f};
        ViewportTransformDragKind dragKind = ViewportTransformDragKind::None;
        bool dragging = false;
    };

    // Editor-only viewport transform interaction. Translation, rotation, and scale
    // gestures all publish through the same exclusive EditorCommandHistory
    // coalesced-command boundary, so a continuous drag produces exactly one undo
    // entry and cancellation restores the complete gesture-start document snapshot.
    class ViewportTransformModel
    {
      public:
        static constexpr float MinimumZoom = 0.05f;
        static constexpr float MaximumZoom = 64.f;
        static constexpr float TranslationHandleRadius = 12.f;
        static constexpr float RotationHandleDistance = 64.f;
        static constexpr float RotationHandleRadius = 10.f;
        static constexpr float ScaleHandleDistance = 52.f;
        static constexpr float ScaleHandleRadius = 9.f;
        static constexpr float MinimumScaleMagnitude = 0.001f;
        static constexpr float MaximumScaleMagnitude = 10000.f;

        ViewportTransformModel(EditorDocument& document, EditorCommandHistory& history) noexcept;

        [[nodiscard]] bool setViewport(EditorViewportRect viewport) noexcept;
        [[nodiscard]] bool setView(sf::Vector2f worldCenter, float zoom) noexcept;

        [[nodiscard]] const EditorViewportRect& viewport() const noexcept;
        [[nodiscard]] sf::Vector2f worldCenter() const noexcept;
        [[nodiscard]] float zoom() const noexcept;

        [[nodiscard]] sf::Vector2f worldToViewport(sf::Vector2f worldPosition) const noexcept;
        [[nodiscard]] sf::Vector2f viewportToWorld(sf::Vector2f viewportPosition) const noexcept;

        [[nodiscard]] std::optional<ViewportTransformSnapshot> snapshot() const;
        [[nodiscard]] bool hitTestSelectedHandle(sf::Vector2f viewportPosition) const noexcept;
        [[nodiscard]] bool hitTestRotationHandle(sf::Vector2f viewportPosition) const noexcept;
        [[nodiscard]] bool hitTestScaleHandle(ViewportScaleHandle handle,
                                              sf::Vector2f viewportPosition) const noexcept;

        [[nodiscard]] bool beginTranslationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool updateTranslationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool endTranslationDrag();
        [[nodiscard]] bool cancelTranslationDrag();

        [[nodiscard]] bool beginRotationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool updateRotationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool endRotationDrag();
        [[nodiscard]] bool cancelRotationDrag();

        [[nodiscard]] bool beginScaleDrag(ViewportScaleHandle handle,
                                          sf::Vector2f pointerPosition);
        [[nodiscard]] bool updateScaleDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool endScaleDrag();
        [[nodiscard]] bool cancelScaleDrag();

        [[nodiscard]] bool cancelActiveDrag();
        [[nodiscard]] bool isDragging() const noexcept;
        [[nodiscard]] ViewportTransformDragKind dragKind() const noexcept;

      private:
        struct DragState
        {
            ViewportTransformDragKind kind = ViewportTransformDragKind::None;
            EditorObjectId objectId = InvalidEditorObjectId;
            l2d::TransformState startTransform;
            sf::Vector2f pointerStartWorld{0.f, 0.f};
            float previousPointerAngle = 0.f;
            float accumulatedRotation = 0.f;
        };

        [[nodiscard]] bool viewportIsValid() const noexcept;
        [[nodiscard]] const EditorObjectRecord* selectedObject() const noexcept;
        [[nodiscard]] sf::Vector2f gizmoCenter(const EditorObjectRecord& object) const noexcept;
        [[nodiscard]] sf::Vector2f rotationHandle(const EditorObjectRecord& object) const noexcept;
        [[nodiscard]] sf::Vector2f scaleHandle(const EditorObjectRecord& object,
                                               ViewportScaleHandle handle) const noexcept;
        [[nodiscard]] bool beginDrag(ViewportTransformDragKind kind, sf::Vector2f pointerPosition,
                                     const char* label);
        [[nodiscard]] bool finishDrag(ViewportTransformDragKind expectedKind);
        [[nodiscard]] bool cancelDrag(ViewportTransformDragKind expectedKind);
        [[nodiscard]] bool selectedDragObject(const EditorObjectRecord*& object);

        EditorDocument* m_document = nullptr;
        EditorCommandHistory* m_history = nullptr;
        EditorViewportRect m_viewport;
        sf::Vector2f m_worldCenter{0.f, 0.f};
        float m_zoom = 1.f;
        std::optional<DragState> m_drag;
    };
}

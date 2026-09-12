#pragma once

#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <SFML/System/Vector2.hpp>

#include <optional>

namespace l2d_editor
{
    struct EditorViewportRect
    {
        sf::Vector2f position{0.f, 0.f};
        sf::Vector2f size{1.f, 1.f};
    };

    struct ViewportTransformSnapshot
    {
        EditorObjectId objectId = InvalidEditorObjectId;
        l2d::TransformState transform;
        sf::Vector2f gizmoPosition{0.f, 0.f};
        bool dragging = false;
    };

    // Editor-only viewport mapping and translation-gizmo interaction. The model
    // never stores pointers into EditorDocument object storage; selected IDs are
    // resolved on every operation. A continuous drag is published through one
    // EditorCommandHistory coalesced command so the gesture produces one undo
    // entry regardless of the number of pointer updates.
    class ViewportTransformModel
    {
      public:
        static constexpr float MinimumZoom = 0.05f;
        static constexpr float MaximumZoom = 64.f;
        static constexpr float TranslationHandleRadius = 12.f;

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

        [[nodiscard]] bool beginTranslationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool updateTranslationDrag(sf::Vector2f pointerPosition);
        [[nodiscard]] bool endTranslationDrag();
        [[nodiscard]] bool cancelTranslationDrag();
        [[nodiscard]] bool isDragging() const noexcept;

      private:
        struct DragState
        {
            EditorObjectId objectId = InvalidEditorObjectId;
            l2d::TransformState startTransform;
            sf::Vector2f pointerStartWorld{0.f, 0.f};
        };

        [[nodiscard]] bool viewportIsValid() const noexcept;
        [[nodiscard]] const EditorObjectRecord* selectedObject() const noexcept;

        EditorDocument* m_document = nullptr;
        EditorCommandHistory* m_history = nullptr;
        EditorViewportRect m_viewport;
        sf::Vector2f m_worldCenter{0.f, 0.f};
        float m_zoom = 1.f;
        std::optional<DragState> m_drag;
    };
}

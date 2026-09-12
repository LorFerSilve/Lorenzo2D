#include <Lorenzo2DEditor/ViewportTransformModel.hpp>

#include <cmath>

namespace l2d_editor
{
    namespace
    {
        bool isFinite(sf::Vector2f value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }
    }

    ViewportTransformModel::ViewportTransformModel(EditorDocument& document,
                                                   EditorCommandHistory& history) noexcept
        : m_document(&document), m_history(&history)
    {
    }

    bool ViewportTransformModel::setViewport(EditorViewportRect viewport) noexcept
    {
        if (m_drag || !isFinite(viewport.position) || !isFinite(viewport.size) ||
            viewport.size.x <= 0.f || viewport.size.y <= 0.f)
        {
            return false;
        }
        m_viewport = viewport;
        return true;
    }

    bool ViewportTransformModel::setView(sf::Vector2f worldCenter, float zoom) noexcept
    {
        if (m_drag || !isFinite(worldCenter) || !std::isfinite(zoom) || zoom < MinimumZoom ||
            zoom > MaximumZoom)
        {
            return false;
        }
        m_worldCenter = worldCenter;
        m_zoom = zoom;
        return true;
    }

    const EditorViewportRect& ViewportTransformModel::viewport() const noexcept
    {
        return m_viewport;
    }

    sf::Vector2f ViewportTransformModel::worldCenter() const noexcept
    {
        return m_worldCenter;
    }

    float ViewportTransformModel::zoom() const noexcept
    {
        return m_zoom;
    }

    sf::Vector2f ViewportTransformModel::worldToViewport(sf::Vector2f worldPosition) const noexcept
    {
        const sf::Vector2f center{m_viewport.position.x + m_viewport.size.x * 0.5f,
                                  m_viewport.position.y + m_viewport.size.y * 0.5f};
        return {center.x + (worldPosition.x - m_worldCenter.x) * m_zoom,
                center.y + (worldPosition.y - m_worldCenter.y) * m_zoom};
    }

    sf::Vector2f ViewportTransformModel::viewportToWorld(
        sf::Vector2f viewportPosition) const noexcept
    {
        const sf::Vector2f center{m_viewport.position.x + m_viewport.size.x * 0.5f,
                                  m_viewport.position.y + m_viewport.size.y * 0.5f};
        return {m_worldCenter.x + (viewportPosition.x - center.x) / m_zoom,
                m_worldCenter.y + (viewportPosition.y - center.y) / m_zoom};
    }

    std::optional<ViewportTransformSnapshot> ViewportTransformModel::snapshot() const
    {
        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return std::nullopt;

        ViewportTransformSnapshot result;
        result.objectId = object->id;
        result.transform = object->prefab.transform;
        result.gizmoPosition = worldToViewport(object->prefab.transform.position);
        result.dragging = m_drag.has_value();
        return result;
    }

    bool ViewportTransformModel::hitTestSelectedHandle(sf::Vector2f viewportPosition) const noexcept
    {
        if (!viewportIsValid() || !isFinite(viewportPosition)) return false;
        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return false;

        const sf::Vector2f gizmoPosition = worldToViewport(object->prefab.transform.position);
        const float deltaX = viewportPosition.x - gizmoPosition.x;
        const float deltaY = viewportPosition.y - gizmoPosition.y;
        const float radiusSquared = TranslationHandleRadius * TranslationHandleRadius;
        return deltaX * deltaX + deltaY * deltaY <= radiusSquared;
    }

    bool ViewportTransformModel::beginTranslationDrag(sf::Vector2f pointerPosition)
    {
        if (m_drag || !viewportIsValid() || !isFinite(pointerPosition) ||
            m_history->hasOpenCoalescedCommand())
        {
            return false;
        }

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr || !hitTestSelectedHandle(pointerPosition)) return false;

        if (!m_history->beginCoalescedCommand(*m_document, "Move object in viewport")) return false;

        m_drag = DragState{object->id, object->prefab.transform, viewportToWorld(pointerPosition)};
        return true;
    }

    bool ViewportTransformModel::updateTranslationDrag(sf::Vector2f pointerPosition)
    {
        if (!m_drag || !isFinite(pointerPosition)) return false;

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr || object->id != m_drag->objectId)
        {
            (void)cancelTranslationDrag();
            return false;
        }

        const sf::Vector2f pointerWorld = viewportToWorld(pointerPosition);
        l2d::TransformState target = m_drag->startTransform;
        target.position = {
            m_drag->startTransform.position.x + (pointerWorld.x - m_drag->pointerStartWorld.x),
            m_drag->startTransform.position.y + (pointerWorld.y - m_drag->pointerStartWorld.y)};
        if (!isFinite(target.position) || object->prefab.transform.position == target.position)
            return false;

        const EditorObjectId objectId = m_drag->objectId;
        return m_history->updateCoalescedCommand(
            *m_document, [objectId, target](EditorDocument& document)
            { return document.setObjectTransform(objectId, target); });
    }

    bool ViewportTransformModel::endTranslationDrag()
    {
        if (!m_drag) return false;

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr || object->id != m_drag->objectId)
        {
            (void)cancelTranslationDrag();
            return false;
        }

        if (object->prefab.transform.position == m_drag->startTransform.position)
        {
            (void)cancelTranslationDrag();
            return false;
        }

        const bool committed = m_history->commitCoalescedCommand(*m_document);
        m_drag.reset();
        return committed;
    }

    bool ViewportTransformModel::cancelTranslationDrag()
    {
        if (!m_drag) return false;
        const bool cancelled = m_history->cancelCoalescedCommand(*m_document);
        m_drag.reset();
        return cancelled;
    }

    bool ViewportTransformModel::isDragging() const noexcept
    {
        return m_drag.has_value();
    }

    bool ViewportTransformModel::viewportIsValid() const noexcept
    {
        return isFinite(m_viewport.position) && isFinite(m_viewport.size) &&
               m_viewport.size.x > 0.f && m_viewport.size.y > 0.f && std::isfinite(m_zoom) &&
               m_zoom >= MinimumZoom && m_zoom <= MaximumZoom;
    }

    const EditorObjectRecord* ViewportTransformModel::selectedObject() const noexcept
    {
        if (m_document == nullptr) return nullptr;
        const EditorObjectId selected = m_document->selectedObject();
        if (selected == InvalidEditorObjectId) return nullptr;
        return m_document->findObject(selected);
    }
}

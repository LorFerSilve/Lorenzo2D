#include <Lorenzo2DEditor/ViewportTransformModel.hpp>

#include <cmath>

namespace l2d_editor
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float RadiansToDegrees = 180.f / Pi;
        constexpr float InverseSqrtTwo = 0.70710678118654752440f;

        bool isFinite(sf::Vector2f value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        float distanceSquared(sf::Vector2f lhs, sf::Vector2f rhs) noexcept
        {
            const float x = lhs.x - rhs.x;
            const float y = lhs.y - rhs.y;
            return x * x + y * y;
        }

        float pointerAngle(sf::Vector2f center, sf::Vector2f pointer) noexcept
        {
            return std::atan2(pointer.y - center.y, pointer.x - center.x);
        }

        float wrappedAngleDelta(float current, float previous) noexcept
        {
            float delta = current - previous;
            while (delta > Pi) delta -= 2.f * Pi;
            while (delta < -Pi) delta += 2.f * Pi;
            return delta;
        }

        bool sameTransform(const l2d::TransformState& lhs, const l2d::TransformState& rhs) noexcept
        {
            return lhs.position == rhs.position && lhs.rotation == rhs.rotation &&
                   lhs.scale == rhs.scale;
        }

        bool scaleComponentAllowed(float start, float target) noexcept
        {
            if (!std::isfinite(target) || std::fabs(target) > ViewportTransformModel::MaximumScaleMagnitude)
                return false;
            if (start != 0.f && std::fabs(target) < ViewportTransformModel::MinimumScaleMagnitude)
                return false;
            return true;
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
        result.gizmoPosition = gizmoCenter(*object);
        result.rotationHandlePosition = rotationHandle(*object);
        result.scaleXHandlePosition = scaleHandle(*object, ViewportScaleHandle::X);
        result.scaleYHandlePosition = scaleHandle(*object, ViewportScaleHandle::Y);
        result.scaleUniformHandlePosition = scaleHandle(*object, ViewportScaleHandle::Uniform);
        result.dragKind = dragKind();
        result.dragging = m_drag.has_value();
        return result;
    }

    bool ViewportTransformModel::hitTestSelectedHandle(sf::Vector2f viewportPosition) const noexcept
    {
        if (!viewportIsValid() || !isFinite(viewportPosition)) return false;
        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return false;

        const float radiusSquared = TranslationHandleRadius * TranslationHandleRadius;
        return distanceSquared(viewportPosition, gizmoCenter(*object)) <= radiusSquared;
    }

    bool ViewportTransformModel::hitTestRotationHandle(sf::Vector2f viewportPosition) const noexcept
    {
        if (!viewportIsValid() || !isFinite(viewportPosition)) return false;
        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return false;

        const float radiusSquared = RotationHandleRadius * RotationHandleRadius;
        return distanceSquared(viewportPosition, rotationHandle(*object)) <= radiusSquared;
    }

    bool ViewportTransformModel::hitTestScaleHandle(ViewportScaleHandle handle,
                                                    sf::Vector2f viewportPosition) const noexcept
    {
        if (!viewportIsValid() || !isFinite(viewportPosition)) return false;
        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return false;

        const float radiusSquared = ScaleHandleRadius * ScaleHandleRadius;
        return distanceSquared(viewportPosition, scaleHandle(*object, handle)) <= radiusSquared;
    }

    bool ViewportTransformModel::beginTranslationDrag(sf::Vector2f pointerPosition)
    {
        if (!hitTestSelectedHandle(pointerPosition)) return false;
        return beginDrag(ViewportTransformDragKind::Translation, pointerPosition,
                         "Move object in viewport");
    }

    bool ViewportTransformModel::updateTranslationDrag(sf::Vector2f pointerPosition)
    {
        if (!m_drag || m_drag->kind != ViewportTransformDragKind::Translation ||
            !isFinite(pointerPosition))
        {
            return false;
        }

        const EditorObjectRecord* object = nullptr;
        if (!selectedDragObject(object)) return false;

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
        return finishDrag(ViewportTransformDragKind::Translation);
    }

    bool ViewportTransformModel::cancelTranslationDrag()
    {
        return cancelDrag(ViewportTransformDragKind::Translation);
    }

    bool ViewportTransformModel::beginRotationDrag(sf::Vector2f pointerPosition)
    {
        if (!hitTestRotationHandle(pointerPosition)) return false;
        if (!beginDrag(ViewportTransformDragKind::Rotation, pointerPosition,
                       "Rotate object in viewport"))
        {
            return false;
        }

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr)
        {
            (void)cancelActiveDrag();
            return false;
        }
        m_drag->previousPointerAngle = pointerAngle(gizmoCenter(*object), pointerPosition);
        return true;
    }

    bool ViewportTransformModel::updateRotationDrag(sf::Vector2f pointerPosition)
    {
        if (!m_drag || m_drag->kind != ViewportTransformDragKind::Rotation ||
            !isFinite(pointerPosition))
        {
            return false;
        }

        const EditorObjectRecord* object = nullptr;
        if (!selectedDragObject(object)) return false;

        const sf::Vector2f center = gizmoCenter(*object);
        const sf::Vector2f offset = pointerPosition - center;
        if (offset.x * offset.x + offset.y * offset.y <= 0.0001f) return false;

        const float currentAngle = pointerAngle(center, pointerPosition);
        const float delta = wrappedAngleDelta(currentAngle, m_drag->previousPointerAngle);
        if (delta == 0.f) return false;

        const float accumulated = m_drag->accumulatedRotation + delta;
        l2d::TransformState target = m_drag->startTransform;
        target.rotation = m_drag->startTransform.rotation + accumulated * RadiansToDegrees;
        if (!std::isfinite(target.rotation) || object->prefab.transform.rotation == target.rotation)
            return false;

        const EditorObjectId objectId = m_drag->objectId;
        const bool updated = m_history->updateCoalescedCommand(
            *m_document, [objectId, target](EditorDocument& document)
            { return document.setObjectTransform(objectId, target); });
        if (updated)
        {
            m_drag->previousPointerAngle = currentAngle;
            m_drag->accumulatedRotation = accumulated;
        }
        return updated;
    }

    bool ViewportTransformModel::endRotationDrag()
    {
        return finishDrag(ViewportTransformDragKind::Rotation);
    }

    bool ViewportTransformModel::cancelRotationDrag()
    {
        return cancelDrag(ViewportTransformDragKind::Rotation);
    }

    bool ViewportTransformModel::beginScaleDrag(ViewportScaleHandle handle,
                                                sf::Vector2f pointerPosition)
    {
        if (!hitTestScaleHandle(handle, pointerPosition)) return false;

        ViewportTransformDragKind kind = ViewportTransformDragKind::ScaleUniform;
        const char* label = "Scale object uniformly in viewport";
        if (handle == ViewportScaleHandle::X)
        {
            kind = ViewportTransformDragKind::ScaleX;
            label = "Scale object X in viewport";
        }
        else if (handle == ViewportScaleHandle::Y)
        {
            kind = ViewportTransformDragKind::ScaleY;
            label = "Scale object Y in viewport";
        }

        if (!beginDrag(kind, pointerPosition, label)) return false;

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr)
        {
            (void)cancelActiveDrag();
            return false;
        }

        const sf::Vector2f center = gizmoCenter(*object);
        const sf::Vector2f offset = pointerPosition - center;
        if (kind == ViewportTransformDragKind::ScaleX)
            m_drag->scaleStartProjection = offset.x;
        else if (kind == ViewportTransformDragKind::ScaleY)
            m_drag->scaleStartProjection = offset.y;
        else
            m_drag->scaleStartProjection = (offset.x + offset.y) * InverseSqrtTwo;

        if (!std::isfinite(m_drag->scaleStartProjection) ||
            std::fabs(m_drag->scaleStartProjection) < 1.f)
        {
            (void)cancelActiveDrag();
            return false;
        }
        return true;
    }

    bool ViewportTransformModel::updateScaleDrag(sf::Vector2f pointerPosition)
    {
        if (!m_drag || !isFinite(pointerPosition)) return false;
        const ViewportTransformDragKind kind = m_drag->kind;
        if (kind != ViewportTransformDragKind::ScaleX && kind != ViewportTransformDragKind::ScaleY &&
            kind != ViewportTransformDragKind::ScaleUniform)
        {
            return false;
        }

        const EditorObjectRecord* object = nullptr;
        if (!selectedDragObject(object)) return false;

        const sf::Vector2f center = gizmoCenter(*object);
        const sf::Vector2f offset = pointerPosition - center;
        float projection = 0.f;
        if (kind == ViewportTransformDragKind::ScaleX)
            projection = offset.x;
        else if (kind == ViewportTransformDragKind::ScaleY)
            projection = offset.y;
        else
            projection = (offset.x + offset.y) * InverseSqrtTwo;

        if (!std::isfinite(projection)) return false;
        const float factor = projection / m_drag->scaleStartProjection;
        if (!std::isfinite(factor)) return false;

        l2d::TransformState target = m_drag->startTransform;
        if (kind == ViewportTransformDragKind::ScaleX ||
            kind == ViewportTransformDragKind::ScaleUniform)
        {
            target.scale.x = m_drag->startTransform.scale.x * factor;
        }
        if (kind == ViewportTransformDragKind::ScaleY ||
            kind == ViewportTransformDragKind::ScaleUniform)
        {
            target.scale.y = m_drag->startTransform.scale.y * factor;
        }

        if (!scaleComponentAllowed(m_drag->startTransform.scale.x, target.scale.x) ||
            !scaleComponentAllowed(m_drag->startTransform.scale.y, target.scale.y) ||
            object->prefab.transform.scale == target.scale)
        {
            return false;
        }

        const EditorObjectId objectId = m_drag->objectId;
        return m_history->updateCoalescedCommand(
            *m_document, [objectId, target](EditorDocument& document)
            { return document.setObjectTransform(objectId, target); });
    }

    bool ViewportTransformModel::endScaleDrag()
    {
        if (!m_drag) return false;
        const ViewportTransformDragKind kind = m_drag->kind;
        if (kind != ViewportTransformDragKind::ScaleX && kind != ViewportTransformDragKind::ScaleY &&
            kind != ViewportTransformDragKind::ScaleUniform)
        {
            return false;
        }
        return finishDrag(kind);
    }

    bool ViewportTransformModel::cancelScaleDrag()
    {
        if (!m_drag) return false;
        const ViewportTransformDragKind kind = m_drag->kind;
        if (kind != ViewportTransformDragKind::ScaleX && kind != ViewportTransformDragKind::ScaleY &&
            kind != ViewportTransformDragKind::ScaleUniform)
        {
            return false;
        }
        return cancelDrag(kind);
    }

    bool ViewportTransformModel::cancelActiveDrag()
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

    ViewportTransformDragKind ViewportTransformModel::dragKind() const noexcept
    {
        return m_drag ? m_drag->kind : ViewportTransformDragKind::None;
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

    sf::Vector2f ViewportTransformModel::gizmoCenter(const EditorObjectRecord& object) const noexcept
    {
        return worldToViewport(object.prefab.transform.position);
    }

    sf::Vector2f ViewportTransformModel::rotationHandle(const EditorObjectRecord& object) const noexcept
    {
        const sf::Vector2f center = gizmoCenter(object);
        return {center.x, center.y - RotationHandleDistance};
    }

    sf::Vector2f ViewportTransformModel::scaleHandle(const EditorObjectRecord& object,
                                                     ViewportScaleHandle handle) const noexcept
    {
        const sf::Vector2f center = gizmoCenter(object);
        if (handle == ViewportScaleHandle::X) return {center.x + ScaleHandleDistance, center.y};
        if (handle == ViewportScaleHandle::Y) return {center.x, center.y + ScaleHandleDistance};
        const float diagonal = ScaleHandleDistance * InverseSqrtTwo;
        return {center.x + diagonal, center.y + diagonal};
    }

    bool ViewportTransformModel::beginDrag(ViewportTransformDragKind kind,
                                           sf::Vector2f pointerPosition, const char* label)
    {
        if (m_drag || !viewportIsValid() || !isFinite(pointerPosition) ||
            m_history->hasOpenCoalescedCommand())
        {
            return false;
        }

        const EditorObjectRecord* object = selectedObject();
        if (object == nullptr) return false;
        if (!m_history->beginCoalescedCommand(*m_document, label)) return false;

        DragState drag;
        drag.kind = kind;
        drag.objectId = object->id;
        drag.startTransform = object->prefab.transform;
        drag.pointerStartWorld = viewportToWorld(pointerPosition);
        m_drag = drag;
        return true;
    }

    bool ViewportTransformModel::finishDrag(ViewportTransformDragKind expectedKind)
    {
        if (!m_drag || m_drag->kind != expectedKind) return false;

        const EditorObjectRecord* object = nullptr;
        if (!selectedDragObject(object)) return false;
        if (sameTransform(object->prefab.transform, m_drag->startTransform))
        {
            (void)cancelActiveDrag();
            return false;
        }

        const bool committed = m_history->commitCoalescedCommand(*m_document);
        m_drag.reset();
        return committed;
    }

    bool ViewportTransformModel::cancelDrag(ViewportTransformDragKind expectedKind)
    {
        if (!m_drag || m_drag->kind != expectedKind) return false;
        return cancelActiveDrag();
    }

    bool ViewportTransformModel::selectedDragObject(const EditorObjectRecord*& object)
    {
        object = selectedObject();
        if (object == nullptr || !m_drag || object->id != m_drag->objectId)
        {
            (void)cancelActiveDrag();
            object = nullptr;
            return false;
        }
        return true;
    }
}

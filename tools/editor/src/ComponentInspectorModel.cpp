#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>

#include <utility>

namespace l2d_editor
{
    namespace
    {
        void appendBuiltIn(std::vector<InspectorComponentEntry>& entries,
                           InspectorComponentKind kind)
        {
            entries.push_back({kind, std::string(componentDisplayName(kind)),
                               kind != InspectorComponentKind::Transform, std::nullopt});
        }
    }

    std::string_view componentDisplayName(InspectorComponentKind kind) noexcept
    {
        switch (kind)
        {
        case InspectorComponentKind::Transform:
            return "Transform";
        case InspectorComponentKind::RectangleRenderer:
            return "Rectangle Renderer";
        case InspectorComponentKind::CircleRenderer:
            return "Circle Renderer";
        case InspectorComponentKind::SpriteRenderer:
            return "Sprite Renderer";
        case InspectorComponentKind::Animator:
            return "Animator";
        case InspectorComponentKind::RigidBody:
            return "Rigid Body 2D";
        case InspectorComponentKind::CharacterMotor:
            return "Character Motor 2D";
        case InspectorComponentKind::TopDownController:
            return "Top-Down Controller 2D";
        case InspectorComponentKind::GridStepController:
            return "Grid-Step Controller 2D";
        case InspectorComponentKind::PlatformerController:
            return "Platformer Controller 2D";
        case InspectorComponentKind::PathFollower:
            return "Path Follower 2D";
        case InspectorComponentKind::BoxCollider:
            return "Box Collider 2D";
        case InspectorComponentKind::CircleCollider:
            return "Circle Collider 2D";
        case InspectorComponentKind::CapsuleCollider:
            return "Capsule Collider 2D";
        case InspectorComponentKind::ConvexPolygonCollider:
            return "Convex Polygon Collider 2D";
        case InspectorComponentKind::Custom:
            return "Custom Component";
        }
        return "Unknown Component";
    }

    ComponentInspectorModel::ComponentInspectorModel(EditorDocument& document,
                                                     EditorCommandHistory& history) noexcept
        : m_document(&document), m_history(&history)
    {
    }

    std::optional<ComponentInspectorSnapshot> ComponentInspectorModel::snapshot() const
    {
        const EditorObjectId id = selectedId();
        if (id == InvalidEditorObjectId) return std::nullopt;

        const EditorObjectRecord* object = m_document->findObject(id);
        if (object == nullptr) return std::nullopt;

        const l2d::Prefab& prefab = object->prefab;
        ComponentInspectorSnapshot result;
        result.objectId = id;
        result.name = prefab.name;
        result.tag = prefab.tag;
        result.active = prefab.active;
        result.zOrder = prefab.zOrder;
        result.transform = prefab.transform;

        result.components.reserve(15u + prefab.customComponents.size());
        appendBuiltIn(result.components, InspectorComponentKind::Transform);
        if (prefab.rectangleRenderer)
            appendBuiltIn(result.components, InspectorComponentKind::RectangleRenderer);
        if (prefab.circleRenderer)
            appendBuiltIn(result.components, InspectorComponentKind::CircleRenderer);
        if (prefab.spriteRenderer)
            appendBuiltIn(result.components, InspectorComponentKind::SpriteRenderer);
        if (prefab.animator) appendBuiltIn(result.components, InspectorComponentKind::Animator);
        if (prefab.rigidBody) appendBuiltIn(result.components, InspectorComponentKind::RigidBody);
        if (prefab.characterMotor)
            appendBuiltIn(result.components, InspectorComponentKind::CharacterMotor);
        if (prefab.topDownController)
            appendBuiltIn(result.components, InspectorComponentKind::TopDownController);
        if (prefab.gridStepController)
            appendBuiltIn(result.components, InspectorComponentKind::GridStepController);
        if (prefab.platformerController)
            appendBuiltIn(result.components, InspectorComponentKind::PlatformerController);
        if (prefab.pathFollower)
            appendBuiltIn(result.components, InspectorComponentKind::PathFollower);
        if (prefab.boxCollider) appendBuiltIn(result.components, InspectorComponentKind::BoxCollider);
        if (prefab.circleCollider)
            appendBuiltIn(result.components, InspectorComponentKind::CircleCollider);
        if (prefab.capsuleCollider)
            appendBuiltIn(result.components, InspectorComponentKind::CapsuleCollider);
        if (prefab.convexPolygonCollider)
            appendBuiltIn(result.components, InspectorComponentKind::ConvexPolygonCollider);

        for (std::size_t index = 0u; index < prefab.customComponents.size(); ++index)
        {
            const l2d::SerializedComponentPrefab& component = prefab.customComponents[index];
            result.components.push_back({InspectorComponentKind::Custom,
                                         "Custom: " + component.type, true, index});
        }

        return result;
    }

    bool ComponentInspectorModel::setName(std::string name)
    {
        return editSelected("Rename object", [name = std::move(name)](l2d::Prefab& prefab) mutable
                            {
                                if (prefab.name == name) return false;
                                prefab.name = std::move(name);
                                return true;
                            });
    }

    bool ComponentInspectorModel::setTag(std::string tag)
    {
        return editSelected("Set object tag", [tag = std::move(tag)](l2d::Prefab& prefab) mutable
                            {
                                if (prefab.tag == tag) return false;
                                prefab.tag = std::move(tag);
                                return true;
                            });
    }

    bool ComponentInspectorModel::setActive(bool active)
    {
        return editSelected("Set object active", [active](l2d::Prefab& prefab)
                            {
                                if (prefab.active == active) return false;
                                prefab.active = active;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setZOrder(std::int32_t zOrder)
    {
        return editSelected("Set object z-order", [zOrder](l2d::Prefab& prefab)
                            {
                                if (prefab.zOrder == zOrder) return false;
                                prefab.zOrder = zOrder;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setTransform(l2d::TransformState transform)
    {
        return editSelected("Set object transform", [transform](l2d::Prefab& prefab)
                            {
                                if (prefab.transform.position == transform.position &&
                                    prefab.transform.rotation == transform.rotation &&
                                    prefab.transform.scale == transform.scale)
                                {
                                    return false;
                                }
                                prefab.transform = transform;
                                return true;
                            });
    }

    bool ComponentInspectorModel::editSelected(std::string label, const PrefabMutation& mutation)
    {
        if (!mutation) return false;

        const EditorObjectId id = selectedId();
        if (id == InvalidEditorObjectId) return false;

        return m_history->execute(
            *m_document, std::move(label),
            [id, &mutation](EditorDocument& document)
            {
                const EditorObjectRecord* object = document.findObject(id);
                if (object == nullptr) return false;

                l2d::Prefab replacement = object->prefab;
                if (!mutation(replacement)) return false;
                return document.replaceObject(id, std::move(replacement));
            });
    }

    bool ComponentInspectorModel::addComponent(InspectorComponentKind kind)
    {
        const std::string label = "Add " + std::string(componentDisplayName(kind));
        return editSelected(label, [kind](l2d::Prefab& prefab)
                            {
                                switch (kind)
                                {
                                case InspectorComponentKind::Transform:
                                case InspectorComponentKind::Custom:
                                    return false;
                                case InspectorComponentKind::RectangleRenderer:
                                    if (prefab.rectangleRenderer) return false;
                                    prefab.rectangleRenderer.emplace();
                                    return true;
                                case InspectorComponentKind::CircleRenderer:
                                    if (prefab.circleRenderer) return false;
                                    prefab.circleRenderer.emplace();
                                    return true;
                                case InspectorComponentKind::SpriteRenderer:
                                    if (prefab.spriteRenderer) return false;
                                    prefab.spriteRenderer.emplace();
                                    return true;
                                case InspectorComponentKind::Animator:
                                    if (prefab.animator) return false;
                                    prefab.animator.emplace();
                                    return true;
                                case InspectorComponentKind::RigidBody:
                                    if (prefab.rigidBody) return false;
                                    prefab.rigidBody.emplace();
                                    return true;
                                case InspectorComponentKind::CharacterMotor:
                                    if (prefab.characterMotor) return false;
                                    prefab.characterMotor.emplace();
                                    return true;
                                case InspectorComponentKind::TopDownController:
                                    if (prefab.topDownController) return false;
                                    prefab.topDownController.emplace();
                                    return true;
                                case InspectorComponentKind::GridStepController:
                                    if (prefab.gridStepController) return false;
                                    prefab.gridStepController.emplace();
                                    return true;
                                case InspectorComponentKind::PlatformerController:
                                    if (prefab.platformerController) return false;
                                    prefab.platformerController.emplace();
                                    return true;
                                case InspectorComponentKind::PathFollower:
                                    if (prefab.pathFollower) return false;
                                    prefab.pathFollower.emplace();
                                    return true;
                                case InspectorComponentKind::BoxCollider:
                                    if (prefab.boxCollider) return false;
                                    prefab.boxCollider.emplace();
                                    return true;
                                case InspectorComponentKind::CircleCollider:
                                    if (prefab.circleCollider) return false;
                                    prefab.circleCollider.emplace();
                                    return true;
                                case InspectorComponentKind::CapsuleCollider:
                                    if (prefab.capsuleCollider) return false;
                                    prefab.capsuleCollider.emplace();
                                    return true;
                                case InspectorComponentKind::ConvexPolygonCollider:
                                    if (prefab.convexPolygonCollider) return false;
                                    prefab.convexPolygonCollider.emplace();
                                    return true;
                                }
                                return false;
                            });
    }

    bool ComponentInspectorModel::removeComponent(InspectorComponentKind kind)
    {
        const std::string label = "Remove " + std::string(componentDisplayName(kind));
        return editSelected(label, [kind](l2d::Prefab& prefab)
                            {
                                switch (kind)
                                {
                                case InspectorComponentKind::Transform:
                                case InspectorComponentKind::Custom:
                                    return false;
                                case InspectorComponentKind::RectangleRenderer:
                                    if (!prefab.rectangleRenderer) return false;
                                    prefab.rectangleRenderer.reset();
                                    return true;
                                case InspectorComponentKind::CircleRenderer:
                                    if (!prefab.circleRenderer) return false;
                                    prefab.circleRenderer.reset();
                                    return true;
                                case InspectorComponentKind::SpriteRenderer:
                                    if (!prefab.spriteRenderer) return false;
                                    prefab.spriteRenderer.reset();
                                    return true;
                                case InspectorComponentKind::Animator:
                                    if (!prefab.animator) return false;
                                    prefab.animator.reset();
                                    return true;
                                case InspectorComponentKind::RigidBody:
                                    if (!prefab.rigidBody) return false;
                                    prefab.rigidBody.reset();
                                    return true;
                                case InspectorComponentKind::CharacterMotor:
                                    if (!prefab.characterMotor) return false;
                                    prefab.characterMotor.reset();
                                    return true;
                                case InspectorComponentKind::TopDownController:
                                    if (!prefab.topDownController) return false;
                                    prefab.topDownController.reset();
                                    return true;
                                case InspectorComponentKind::GridStepController:
                                    if (!prefab.gridStepController) return false;
                                    prefab.gridStepController.reset();
                                    return true;
                                case InspectorComponentKind::PlatformerController:
                                    if (!prefab.platformerController) return false;
                                    prefab.platformerController.reset();
                                    return true;
                                case InspectorComponentKind::PathFollower:
                                    if (!prefab.pathFollower) return false;
                                    prefab.pathFollower.reset();
                                    return true;
                                case InspectorComponentKind::BoxCollider:
                                    if (!prefab.boxCollider) return false;
                                    prefab.boxCollider.reset();
                                    return true;
                                case InspectorComponentKind::CircleCollider:
                                    if (!prefab.circleCollider) return false;
                                    prefab.circleCollider.reset();
                                    return true;
                                case InspectorComponentKind::CapsuleCollider:
                                    if (!prefab.capsuleCollider) return false;
                                    prefab.capsuleCollider.reset();
                                    return true;
                                case InspectorComponentKind::ConvexPolygonCollider:
                                    if (!prefab.convexPolygonCollider) return false;
                                    prefab.convexPolygonCollider.reset();
                                    return true;
                                }
                                return false;
                            });
    }

    bool ComponentInspectorModel::addCustomComponent(l2d::SerializedComponentPrefab component)
    {
        return editSelected(
            "Add custom component",
            [component = std::move(component)](l2d::Prefab& prefab) mutable
            {
                prefab.customComponents.push_back(std::move(component));
                return true;
            });
    }

    bool ComponentInspectorModel::updateCustomComponent(
        std::size_t index, l2d::SerializedComponentPrefab component)
    {
        return editSelected(
            "Update custom component",
            [index, component = std::move(component)](l2d::Prefab& prefab) mutable
            {
                if (index >= prefab.customComponents.size()) return false;
                const l2d::SerializedComponentPrefab& current = prefab.customComponents[index];
                if (current.type == component.type && current.version == component.version &&
                    current.required == component.required && current.data == component.data)
                {
                    return false;
                }
                prefab.customComponents[index] = std::move(component);
                return true;
            });
    }

    bool ComponentInspectorModel::removeCustomComponent(std::size_t index)
    {
        return editSelected("Remove custom component", [index](l2d::Prefab& prefab)
                            {
                                if (index >= prefab.customComponents.size()) return false;
                                prefab.customComponents.erase(prefab.customComponents.begin() +
                                                              static_cast<std::ptrdiff_t>(index));
                                return true;
                            });
    }

    EditorObjectId ComponentInspectorModel::selectedId() const noexcept
    {
        return m_document == nullptr ? InvalidEditorObjectId : m_document->selectedObject();
    }
}

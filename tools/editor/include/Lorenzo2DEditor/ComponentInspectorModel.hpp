#pragma once

#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace l2d_editor
{
    enum class InspectorComponentKind : std::uint8_t
    {
        Transform,
        RectangleRenderer,
        CircleRenderer,
        SpriteRenderer,
        Animator,
        RigidBody,
        CharacterMotor,
        TopDownController,
        GridStepController,
        PlatformerController,
        PathFollower,
        BoxCollider,
        CircleCollider,
        CapsuleCollider,
        ConvexPolygonCollider,
        Custom
    };

    [[nodiscard]] std::string_view componentDisplayName(InspectorComponentKind kind) noexcept;

    struct InspectorComponentEntry
    {
        InspectorComponentKind kind = InspectorComponentKind::Transform;
        std::string displayName;
        bool removable = false;
        std::optional<std::size_t> customIndex;
    };

    struct ComponentInspectorSnapshot
    {
        EditorObjectId objectId = InvalidEditorObjectId;
        std::string name;
        std::string tag;
        bool active = true;
        std::int32_t zOrder = 0;
        l2d::TransformState transform;
        std::vector<InspectorComponentEntry> components;
    };

    class ComponentInspectorModel
    {
      public:
        using PrefabMutation = std::function<bool(l2d::Prefab&)>;

        ComponentInspectorModel(EditorDocument& document, EditorCommandHistory& history) noexcept;

        [[nodiscard]] std::optional<ComponentInspectorSnapshot> snapshot() const;

        [[nodiscard]] bool setName(std::string name);
        [[nodiscard]] bool setTag(std::string tag);
        [[nodiscard]] bool setActive(bool active);
        [[nodiscard]] bool setZOrder(std::int32_t zOrder);
        [[nodiscard]] bool setTransform(l2d::TransformState transform);
        [[nodiscard]] bool setSpriteTextureAsset(l2d::AssetId texture);
        [[nodiscard]] bool addAnimatorClipAsset(l2d::AssetId clip);
        [[nodiscard]] bool setAnimatorInitialClipAsset(l2d::AssetId clip);

        [[nodiscard]] bool editSelected(std::string label, const PrefabMutation& mutation);
        [[nodiscard]] bool addComponent(InspectorComponentKind kind);
        [[nodiscard]] bool removeComponent(InspectorComponentKind kind);

        [[nodiscard]] bool addCustomComponent(l2d::SerializedComponentPrefab component);
        [[nodiscard]] bool updateCustomComponent(std::size_t index,
                                                 l2d::SerializedComponentPrefab component);
        [[nodiscard]] bool removeCustomComponent(std::size_t index);

      private:
        [[nodiscard]] EditorObjectId selectedId() const noexcept;

        EditorDocument* m_document = nullptr;
        EditorCommandHistory* m_history = nullptr;
    };
}

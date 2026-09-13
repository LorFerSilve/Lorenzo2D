#pragma once

#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace l2d_editor
{
    using EditorObjectId = std::uint64_t;

    constexpr EditorObjectId InvalidEditorObjectId = 0u;

    struct EditorObjectRecord
    {
        EditorObjectId id = InvalidEditorObjectId;
        l2d::Prefab prefab;
    };

    class EditorCommandHistory;

    // Editor-only representation of a runtime LevelDocument. Editor IDs are
    // allocated from a monotonically increasing document-local high-water mark.
    // Successful replacement/load and command-history restoration never rewind
    // that allocator, preventing stale IDs from aliasing later objects. IDs are
    // deliberately not serialized into Lorenzo2D's runtime level format.
    class EditorDocument
    {
      public:
        static constexpr std::size_t MaximumObjectCount = l2d::LevelSerializer::MaximumObjectCount;

        EditorDocument() = default;
        EditorDocument(const EditorDocument&) = default;
        EditorDocument(EditorDocument&&) noexcept = default;

        // Assignment is intentionally disabled: replacing an identity-bearing
        // document through implicit memberwise assignment could rewind its
        // object-ID allocator. Use replace()/load() for content replacement.
        EditorDocument& operator=(const EditorDocument&) = delete;
        EditorDocument& operator=(EditorDocument&&) = delete;

        [[nodiscard]] bool replace(l2d::LevelDocument level);
        [[nodiscard]] bool load(std::istream& input);
        [[nodiscard]] bool save(std::ostream& output) const;
        [[nodiscard]] bool loadFromFile(const std::string& filepath);
        [[nodiscard]] bool saveToFile(const std::string& filepath) const;

        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] bool setName(std::string name);

        [[nodiscard]] std::size_t objectCount() const noexcept;
        [[nodiscard]] const std::vector<EditorObjectRecord>& objects() const noexcept;
        [[nodiscard]] const EditorObjectRecord* findObject(EditorObjectId id) const noexcept;
        [[nodiscard]] std::optional<std::size_t> indexOf(EditorObjectId id) const noexcept;

        [[nodiscard]] std::optional<EditorObjectId> addObject(l2d::Prefab prefab);
        [[nodiscard]] bool removeObject(EditorObjectId id);
        [[nodiscard]] bool replaceObject(EditorObjectId id, l2d::Prefab prefab);
        [[nodiscard]] bool renameObject(EditorObjectId id, std::string name);
        [[nodiscard]] bool setObjectTransform(EditorObjectId id, l2d::TransformState transform);
        [[nodiscard]] bool moveObject(EditorObjectId id, std::size_t newIndex);

        [[nodiscard]] EditorObjectId selectedObject() const noexcept;
        [[nodiscard]] bool selectObject(EditorObjectId id) noexcept;
        void clearSelection() noexcept;

        [[nodiscard]] l2d::LevelDocument toLevelDocument() const;

      private:
        struct Snapshot
        {
            std::string name;
            std::vector<EditorObjectRecord> objects;
            EditorObjectId selectedObject = InvalidEditorObjectId;
            EditorObjectId nextObjectId = 1u;
        };

        [[nodiscard]] Snapshot snapshot() const;
        void restore(const Snapshot& snapshot);

        [[nodiscard]] EditorObjectRecord* findObjectMutable(EditorObjectId id) noexcept;

        std::string m_name = "Level";
        std::vector<EditorObjectRecord> m_objects;
        EditorObjectId m_selectedObject = InvalidEditorObjectId;
        EditorObjectId m_nextObjectId = 1u;

        friend class EditorCommandHistory;
    };
}

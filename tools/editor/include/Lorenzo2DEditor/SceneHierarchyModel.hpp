#pragma once

#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace l2d_editor
{
    struct SceneHierarchyItem
    {
        EditorObjectId id = InvalidEditorObjectId;
        std::size_t index = 0u;
        std::string name;
        bool active = true;
        std::int32_t zOrder = 0;
        bool selected = false;
    };

    // Flat hierarchy view over the current public LevelDocument model. Parent /
    // child scene relationships are intentionally not invented by editor-only
    // state while the runtime format remains flat.
    class SceneHierarchyModel
    {
      public:
        explicit SceneHierarchyModel(EditorDocument& document) noexcept;

        [[nodiscard]] std::vector<SceneHierarchyItem> items() const;
        [[nodiscard]] bool select(EditorObjectId id) noexcept;
        [[nodiscard]] bool selectFirst() noexcept;
        [[nodiscard]] bool selectNext() noexcept;
        [[nodiscard]] bool selectPrevious() noexcept;
        void clearSelection() noexcept;

      private:
        EditorDocument* m_document = nullptr;
    };
}

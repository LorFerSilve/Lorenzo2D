#include <Lorenzo2DEditor/SceneHierarchyModel.hpp>

namespace l2d_editor
{
    SceneHierarchyModel::SceneHierarchyModel(EditorDocument& document) noexcept
        : m_document(&document)
    {
    }

    std::vector<SceneHierarchyItem> SceneHierarchyModel::items() const
    {
        std::vector<SceneHierarchyItem> result;
        result.reserve(m_document->objectCount());

        const EditorObjectId selected = m_document->selectedObject();
        const std::vector<EditorObjectRecord>& objects = m_document->objects();
        for (std::size_t index = 0u; index < objects.size(); ++index)
        {
            const EditorObjectRecord& object = objects[index];
            result.push_back(
                {object.id, index, object.prefab.name, object.prefab.active, object.prefab.zOrder,
                 object.id == selected});
        }

        return result;
    }

    bool SceneHierarchyModel::select(EditorObjectId id) noexcept
    {
        return m_document->selectObject(id);
    }

    bool SceneHierarchyModel::selectFirst() noexcept
    {
        if (m_document->objects().empty()) return false;
        return m_document->selectObject(m_document->objects().front().id);
    }

    bool SceneHierarchyModel::selectNext() noexcept
    {
        const std::vector<EditorObjectRecord>& objects = m_document->objects();
        if (objects.empty()) return false;

        const EditorObjectId selected = m_document->selectedObject();
        if (selected == InvalidEditorObjectId) return m_document->selectObject(objects.front().id);

        const std::optional<std::size_t> index = m_document->indexOf(selected);
        if (!index || *index + 1u >= objects.size()) return false;
        return m_document->selectObject(objects[*index + 1u].id);
    }

    bool SceneHierarchyModel::selectPrevious() noexcept
    {
        const std::vector<EditorObjectRecord>& objects = m_document->objects();
        if (objects.empty()) return false;

        const EditorObjectId selected = m_document->selectedObject();
        if (selected == InvalidEditorObjectId) return m_document->selectObject(objects.back().id);

        const std::optional<std::size_t> index = m_document->indexOf(selected);
        if (!index || *index == 0u) return false;
        return m_document->selectObject(objects[*index - 1u].id);
    }

    void SceneHierarchyModel::clearSelection() noexcept
    {
        m_document->clearSelection();
    }
}

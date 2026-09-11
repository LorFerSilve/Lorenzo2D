#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        bool isValidLevelName(const std::string& name) noexcept
        {
            return name.find('\n') == std::string::npos && name.find('\r') == std::string::npos;
        }

        EditorObjectId mergeAllocatorHighWater(EditorObjectId current,
                                               EditorObjectId restored) noexcept
        {
            if (current == InvalidEditorObjectId || restored == InvalidEditorObjectId)
                return InvalidEditorObjectId;
            return std::max(current, restored);
        }
    }

    bool EditorDocument::replace(l2d::LevelDocument level)
    {
        if (!isValidLevelName(level.name) || level.objects.size() > MaximumObjectCount)
            return false;

        std::vector<EditorObjectRecord> replacement;
        replacement.reserve(level.objects.size());

        EditorObjectId nextId = m_nextObjectId;
        for (l2d::Prefab& prefab : level.objects)
        {
            if (!l2d::isValidPrefab(prefab) || nextId == InvalidEditorObjectId) return false;

            replacement.push_back({nextId, std::move(prefab)});
            if (nextId == std::numeric_limits<EditorObjectId>::max())
                nextId = InvalidEditorObjectId;
            else
                ++nextId;
        }

        m_name = std::move(level.name);
        m_objects = std::move(replacement);
        m_selectedObject = InvalidEditorObjectId;
        m_nextObjectId = nextId;
        return true;
    }

    bool EditorDocument::load(std::istream& input)
    {
        l2d::LevelDocument level;
        if (!l2d::LevelSerializer::load(input, level)) return false;
        return replace(std::move(level));
    }

    bool EditorDocument::save(std::ostream& output) const
    {
        return l2d::LevelSerializer::save(output, toLevelDocument());
    }

    bool EditorDocument::loadFromFile(const std::string& filepath)
    {
        l2d::LevelDocument level;
        if (!l2d::LevelSerializer::loadFromFile(filepath, level)) return false;
        return replace(std::move(level));
    }

    bool EditorDocument::saveToFile(const std::string& filepath) const
    {
        return l2d::LevelSerializer::saveToFile(filepath, toLevelDocument());
    }

    const std::string& EditorDocument::name() const noexcept
    {
        return m_name;
    }

    bool EditorDocument::setName(std::string name)
    {
        if (!isValidLevelName(name) || name == m_name) return false;
        m_name = std::move(name);
        return true;
    }

    std::size_t EditorDocument::objectCount() const noexcept
    {
        return m_objects.size();
    }

    const std::vector<EditorObjectRecord>& EditorDocument::objects() const noexcept
    {
        return m_objects;
    }

    const EditorObjectRecord* EditorDocument::findObject(EditorObjectId id) const noexcept
    {
        const auto found =
            std::find_if(m_objects.begin(), m_objects.end(),
                         [id](const EditorObjectRecord& object) { return object.id == id; });
        return found == m_objects.end() ? nullptr : &(*found);
    }

    std::optional<std::size_t> EditorDocument::indexOf(EditorObjectId id) const noexcept
    {
        const auto found =
            std::find_if(m_objects.begin(), m_objects.end(),
                         [id](const EditorObjectRecord& object) { return object.id == id; });
        if (found == m_objects.end()) return std::nullopt;
        return static_cast<std::size_t>(std::distance(m_objects.begin(), found));
    }

    std::optional<EditorObjectId> EditorDocument::addObject(l2d::Prefab prefab)
    {
        if (m_objects.size() >= MaximumObjectCount || !l2d::isValidPrefab(prefab) ||
            m_nextObjectId == InvalidEditorObjectId)
        {
            return std::nullopt;
        }

        const EditorObjectId id = m_nextObjectId;
        if (m_nextObjectId == std::numeric_limits<EditorObjectId>::max())
            m_nextObjectId = InvalidEditorObjectId;
        else
            ++m_nextObjectId;

        m_objects.push_back({id, std::move(prefab)});
        return id;
    }

    bool EditorDocument::removeObject(EditorObjectId id)
    {
        const auto found =
            std::find_if(m_objects.begin(), m_objects.end(),
                         [id](const EditorObjectRecord& object) { return object.id == id; });
        if (found == m_objects.end()) return false;

        m_objects.erase(found);
        if (m_selectedObject == id) m_selectedObject = InvalidEditorObjectId;
        return true;
    }

    bool EditorDocument::replaceObject(EditorObjectId id, l2d::Prefab prefab)
    {
        if (!l2d::isValidPrefab(prefab)) return false;

        EditorObjectRecord* object = findObjectMutable(id);
        if (object == nullptr) return false;

        object->prefab = std::move(prefab);
        return true;
    }

    bool EditorDocument::renameObject(EditorObjectId id, std::string name)
    {
        EditorObjectRecord* object = findObjectMutable(id);
        if (object == nullptr || object->prefab.name == name) return false;

        l2d::Prefab replacement = object->prefab;
        replacement.name = std::move(name);
        if (!l2d::isValidPrefab(replacement)) return false;

        object->prefab = std::move(replacement);
        return true;
    }

    bool EditorDocument::setObjectTransform(EditorObjectId id, l2d::TransformState transform)
    {
        EditorObjectRecord* object = findObjectMutable(id);
        if (object == nullptr) return false;

        const l2d::TransformState& current = object->prefab.transform;
        if (current.position == transform.position && current.rotation == transform.rotation &&
            current.scale == transform.scale)
        {
            return false;
        }

        l2d::Prefab replacement = object->prefab;
        replacement.transform = transform;
        if (!l2d::isValidPrefab(replacement)) return false;

        object->prefab = std::move(replacement);
        return true;
    }

    bool EditorDocument::moveObject(EditorObjectId id, std::size_t newIndex)
    {
        if (newIndex >= m_objects.size()) return false;

        const std::optional<std::size_t> oldIndex = indexOf(id);
        if (!oldIndex || *oldIndex == newIndex) return false;

        EditorObjectRecord object = std::move(m_objects[*oldIndex]);
        m_objects.erase(m_objects.begin() + static_cast<std::ptrdiff_t>(*oldIndex));
        m_objects.insert(m_objects.begin() + static_cast<std::ptrdiff_t>(newIndex),
                         std::move(object));
        return true;
    }

    EditorObjectId EditorDocument::selectedObject() const noexcept
    {
        return m_selectedObject;
    }

    bool EditorDocument::selectObject(EditorObjectId id) noexcept
    {
        if (findObject(id) == nullptr) return false;
        m_selectedObject = id;
        return true;
    }

    void EditorDocument::clearSelection() noexcept
    {
        m_selectedObject = InvalidEditorObjectId;
    }

    l2d::LevelDocument EditorDocument::toLevelDocument() const
    {
        l2d::LevelDocument level;
        level.name = m_name;
        level.objects.reserve(m_objects.size());
        for (const EditorObjectRecord& object : m_objects)
            level.objects.push_back(object.prefab);
        return level;
    }

    EditorDocument::Snapshot EditorDocument::snapshot() const
    {
        return {m_name, m_objects, m_selectedObject, m_nextObjectId};
    }

    void EditorDocument::restore(const Snapshot& snapshot)
    {
        const EditorObjectId mergedHighWater =
            mergeAllocatorHighWater(m_nextObjectId, snapshot.nextObjectId);
        Snapshot replacement = snapshot;
        m_name = std::move(replacement.name);
        m_objects = std::move(replacement.objects);
        m_selectedObject = replacement.selectedObject;
        m_nextObjectId = mergedHighWater;
    }

    EditorObjectRecord* EditorDocument::findObjectMutable(EditorObjectId id) noexcept
    {
        const auto found =
            std::find_if(m_objects.begin(), m_objects.end(),
                         [id](const EditorObjectRecord& object) { return object.id == id; });
        return found == m_objects.end() ? nullptr : &(*found);
    }
}

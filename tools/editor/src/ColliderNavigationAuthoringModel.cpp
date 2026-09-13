#include <Lorenzo2DEditor/ColliderNavigationAuthoringModel.hpp>

#include <algorithm>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        bool sameCell(const l2d::NavigationCell2D& lhs, const l2d::NavigationCell2D& rhs) noexcept
        {
            return lhs.walkable == rhs.walkable && lhs.traversalCost == rhs.traversalCost;
        }
    }

    ColliderNavigationAuthoringModel::ColliderNavigationAuthoringModel(
        EditorDocument& document, EditorCommandHistory& history) noexcept
        : m_document(&document), m_history(&history), m_inspector(document, history)
    {
    }

    std::optional<ColliderAuthoringSnapshot> ColliderNavigationAuthoringModel::colliderSnapshot()
        const
    {
        const EditorObjectRecord* object = m_document->findObject(m_document->selectedObject());
        if (!object) return std::nullopt;

        ColliderAuthoringSnapshot snapshot;
        snapshot.objectId = object->id;
        snapshot.transform = object->prefab.transform;

        if (object->prefab.boxCollider)
        {
            ColliderAuthoringEntry entry;
            entry.kind = ColliderAuthoringKind::Box;
            entry.properties = object->prefab.boxCollider->properties;
            entry.boxSize = object->prefab.boxCollider->size;
            snapshot.colliders.push_back(std::move(entry));
        }
        if (object->prefab.circleCollider)
        {
            ColliderAuthoringEntry entry;
            entry.kind = ColliderAuthoringKind::Circle;
            entry.properties = object->prefab.circleCollider->properties;
            entry.radius = object->prefab.circleCollider->radius;
            snapshot.colliders.push_back(std::move(entry));
        }
        if (object->prefab.capsuleCollider)
        {
            ColliderAuthoringEntry entry;
            entry.kind = ColliderAuthoringKind::Capsule;
            entry.properties = object->prefab.capsuleCollider->properties;
            entry.radius = object->prefab.capsuleCollider->radius;
            entry.height = object->prefab.capsuleCollider->height;
            snapshot.colliders.push_back(std::move(entry));
        }
        if (object->prefab.convexPolygonCollider)
        {
            ColliderAuthoringEntry entry;
            entry.kind = ColliderAuthoringKind::ConvexPolygon;
            entry.properties = object->prefab.convexPolygonCollider->properties;
            entry.vertices = object->prefab.convexPolygonCollider->vertices;
            snapshot.colliders.push_back(std::move(entry));
        }

        return snapshot;
    }

    bool ColliderNavigationAuthoringModel::editColliderProperties(ColliderAuthoringKind kind,
                                                                   sf::Vector2f offset)
    {
        return m_inspector.editSelected(
            "Edit collider offset",
            [kind, offset](l2d::Prefab& prefab)
            {
                switch (kind)
                {
                case ColliderAuthoringKind::Box:
                    if (!prefab.boxCollider || prefab.boxCollider->properties.offset == offset)
                        return false;
                    prefab.boxCollider->properties.offset = offset;
                    return true;
                case ColliderAuthoringKind::Circle:
                    if (!prefab.circleCollider ||
                        prefab.circleCollider->properties.offset == offset)
                        return false;
                    prefab.circleCollider->properties.offset = offset;
                    return true;
                case ColliderAuthoringKind::Capsule:
                    if (!prefab.capsuleCollider ||
                        prefab.capsuleCollider->properties.offset == offset)
                        return false;
                    prefab.capsuleCollider->properties.offset = offset;
                    return true;
                case ColliderAuthoringKind::ConvexPolygon:
                    if (!prefab.convexPolygonCollider ||
                        prefab.convexPolygonCollider->properties.offset == offset)
                        return false;
                    prefab.convexPolygonCollider->properties.offset = offset;
                    return true;
                }
                return false;
            });
    }

    bool ColliderNavigationAuthoringModel::setColliderOffset(ColliderAuthoringKind kind,
                                                              sf::Vector2f offset)
    {
        return editColliderProperties(kind, offset);
    }

    bool ColliderNavigationAuthoringModel::setBoxSize(sf::Vector2f size)
    {
        return m_inspector.editSelected("Resize box collider",
                                        [size](l2d::Prefab& prefab)
                                        {
                                            if (!prefab.boxCollider ||
                                                prefab.boxCollider->size == size)
                                                return false;
                                            prefab.boxCollider->size = size;
                                            return true;
                                        });
    }

    bool ColliderNavigationAuthoringModel::setCircleRadius(float radius)
    {
        return m_inspector.editSelected("Resize circle collider",
                                        [radius](l2d::Prefab& prefab)
                                        {
                                            if (!prefab.circleCollider ||
                                                prefab.circleCollider->radius == radius)
                                                return false;
                                            prefab.circleCollider->radius = radius;
                                            return true;
                                        });
    }

    bool ColliderNavigationAuthoringModel::setCapsule(float radius, float height)
    {
        return m_inspector.editSelected("Resize capsule collider",
                                        [radius, height](l2d::Prefab& prefab)
                                        {
                                            if (!prefab.capsuleCollider ||
                                                (prefab.capsuleCollider->radius == radius &&
                                                 prefab.capsuleCollider->height == height))
                                                return false;
                                            prefab.capsuleCollider->radius = radius;
                                            prefab.capsuleCollider->height = height;
                                            return true;
                                        });
    }

    bool ColliderNavigationAuthoringModel::setConvexPolygonVertices(
        std::vector<sf::Vector2f> vertices)
    {
        return m_inspector.editSelected(
            "Edit convex collider",
            [vertices = std::move(vertices)](l2d::Prefab& prefab) mutable
            {
                if (!prefab.convexPolygonCollider ||
                    prefab.convexPolygonCollider->vertices == vertices)
                    return false;
                prefab.convexPolygonCollider->vertices = std::move(vertices);
                return true;
            });
    }

    bool ColliderNavigationAuthoringModel::setNavigationGrid(l2d::NavigationGrid2D grid)
    {
        if (grid.cellCount() == 0u || grid.cellCount() > MaximumNavigationCells) return false;
        m_navigationGrid = std::move(grid);
        m_navigationUndo.clear();
        m_navigationRedo.clear();
        return true;
    }

    bool ColliderNavigationAuthoringModel::buildNavigationGridFromTileMap(
        const l2d::TileMapData& tileMap, const l2d::NavigationTileMapOptions2D& options)
    {
        auto candidate = l2d::navigationGridFromTileMap(tileMap, options);
        if (!candidate) return false;
        return setNavigationGrid(std::move(*candidate));
    }

    void ColliderNavigationAuthoringModel::clearNavigationGrid() noexcept
    {
        m_navigationGrid.reset();
        m_navigationUndo.clear();
        m_navigationRedo.clear();
    }

    bool ColliderNavigationAuthoringModel::hasNavigationGrid() const noexcept
    {
        return m_navigationGrid.has_value();
    }

    const l2d::NavigationGrid2D* ColliderNavigationAuthoringModel::navigationGrid() const noexcept
    {
        return m_navigationGrid ? &*m_navigationGrid : nullptr;
    }

    std::optional<NavigationOverlaySnapshot> ColliderNavigationAuthoringModel::navigationOverlay()
        const
    {
        if (!m_navigationGrid) return std::nullopt;

        NavigationOverlaySnapshot snapshot;
        snapshot.config = m_navigationGrid->config();
        snapshot.revision = m_navigationGrid->revision();
        snapshot.cells.reserve(m_navigationGrid->cellCount());
        for (std::size_t index = 0u; index < m_navigationGrid->cellCount(); ++index)
        {
            const auto position = m_navigationGrid->cellAt(index);
            if (!position) return std::nullopt;
            const auto cell = m_navigationGrid->cell(*position);
            if (!cell) return std::nullopt;
            snapshot.cells.push_back({*position, m_navigationGrid->cellCenter(*position), *cell});
        }
        return snapshot;
    }

    bool ColliderNavigationAuthoringModel::publishNavigationCellEdit(sf::Vector2i position,
                                                                      l2d::NavigationCell2D cell)
    {
        if (!m_navigationGrid || !l2d::NavigationGrid2D::isValidCell(cell)) return false;
        const auto before = m_navigationGrid->cell(position);
        if (!before || sameCell(*before, cell)) return false;
        if (!m_navigationGrid->setCell(position, cell)) return false;

        pushNavigationUndo({position, *before, cell});
        m_navigationRedo.clear();
        return true;
    }

    bool ColliderNavigationAuthoringModel::setNavigationCell(sf::Vector2i position,
                                                              l2d::NavigationCell2D cell)
    {
        return publishNavigationCellEdit(position, cell);
    }

    bool ColliderNavigationAuthoringModel::setNavigationWalkable(sf::Vector2i position,
                                                                  bool walkable)
    {
        if (!m_navigationGrid) return false;
        const auto cell = m_navigationGrid->cell(position);
        if (!cell) return false;
        l2d::NavigationCell2D candidate = *cell;
        candidate.walkable = walkable;
        return publishNavigationCellEdit(position, candidate);
    }

    bool ColliderNavigationAuthoringModel::setNavigationTraversalCost(sf::Vector2i position,
                                                                       float traversalCost)
    {
        if (!m_navigationGrid) return false;
        const auto cell = m_navigationGrid->cell(position);
        if (!cell) return false;
        l2d::NavigationCell2D candidate = *cell;
        candidate.traversalCost = traversalCost;
        return publishNavigationCellEdit(position, candidate);
    }

    bool ColliderNavigationAuthoringModel::undoNavigation()
    {
        if (!m_navigationGrid || m_navigationUndo.empty()) return false;
        const NavigationCellEdit edit = m_navigationUndo.back();
        if (!m_navigationGrid->setCell(edit.position, edit.before)) return false;
        m_navigationUndo.pop_back();
        m_navigationRedo.push_back(edit);
        return true;
    }

    bool ColliderNavigationAuthoringModel::redoNavigation()
    {
        if (!m_navigationGrid || m_navigationRedo.empty()) return false;
        const NavigationCellEdit edit = m_navigationRedo.back();
        if (!m_navigationGrid->setCell(edit.position, edit.after)) return false;
        m_navigationRedo.pop_back();
        pushNavigationUndo(edit);
        return true;
    }

    std::size_t ColliderNavigationAuthoringModel::navigationUndoCount() const noexcept
    {
        return m_navigationUndo.size();
    }

    std::size_t ColliderNavigationAuthoringModel::navigationRedoCount() const noexcept
    {
        return m_navigationRedo.size();
    }

    void ColliderNavigationAuthoringModel::pushNavigationUndo(NavigationCellEdit edit)
    {
        if (m_navigationUndo.size() == MaximumNavigationHistory) m_navigationUndo.pop_front();
        m_navigationUndo.push_back(std::move(edit));
    }
}

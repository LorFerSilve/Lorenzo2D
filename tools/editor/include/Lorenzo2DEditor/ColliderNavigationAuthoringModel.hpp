#pragma once

#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>
#include <Lorenzo2D/Tilemap/TileMapData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace l2d_editor
{
    enum class ColliderAuthoringKind : std::uint8_t
    {
        Box,
        Circle,
        Capsule,
        ConvexPolygon
    };

    struct ColliderAuthoringEntry
    {
        ColliderAuthoringKind kind = ColliderAuthoringKind::Box;
        l2d::ColliderPrefabProperties properties;
        sf::Vector2f boxSize{0.f, 0.f};
        float radius = 0.f;
        float height = 0.f;
        std::vector<sf::Vector2f> vertices;
    };

    struct ColliderAuthoringSnapshot
    {
        EditorObjectId objectId = InvalidEditorObjectId;
        l2d::TransformState transform;
        std::vector<ColliderAuthoringEntry> colliders;
    };

    struct NavigationOverlayCell
    {
        sf::Vector2i position{0, 0};
        sf::Vector2f worldCenter{0.f, 0.f};
        l2d::NavigationCell2D cell;
    };

    struct NavigationOverlaySnapshot
    {
        l2d::NavigationGridConfig2D config;
        std::uint64_t revision = 0u;
        std::vector<NavigationOverlayCell> cells;
    };

    // Editor-only authoring boundary for Phase 13.6. Collider edits are published
    // through ComponentInspectorModel so runtime Prefab validation and the shared
    // document command history remain authoritative. Navigation authoring owns a
    // bounded NavigationGrid2D snapshot and stores only per-cell deltas for undo/redo.
    class ColliderNavigationAuthoringModel
    {
      public:
        static constexpr std::size_t MaximumNavigationCells = 262144u;
        static constexpr std::size_t MaximumNavigationHistory = 256u;

        ColliderNavigationAuthoringModel(EditorDocument& document,
                                         EditorCommandHistory& history) noexcept;

        [[nodiscard]] std::optional<ColliderAuthoringSnapshot> colliderSnapshot() const;
        [[nodiscard]] bool setColliderOffset(ColliderAuthoringKind kind, sf::Vector2f offset);
        [[nodiscard]] bool setBoxSize(sf::Vector2f size);
        [[nodiscard]] bool setCircleRadius(float radius);
        [[nodiscard]] bool setCapsule(float radius, float height);
        [[nodiscard]] bool setConvexPolygonVertices(std::vector<sf::Vector2f> vertices);

        // Publication is transactional: rejected or over-limit grids leave the
        // previously published grid and navigation edit history unchanged.
        [[nodiscard]] bool setNavigationGrid(l2d::NavigationGrid2D grid);
        [[nodiscard]] bool buildNavigationGridFromTileMap(
            const l2d::TileMapData& tileMap,
            const l2d::NavigationTileMapOptions2D& options = {});
        void clearNavigationGrid() noexcept;

        [[nodiscard]] bool hasNavigationGrid() const noexcept;
        [[nodiscard]] const l2d::NavigationGrid2D* navigationGrid() const noexcept;
        [[nodiscard]] std::optional<NavigationOverlaySnapshot> navigationOverlay() const;

        [[nodiscard]] bool setNavigationCell(sf::Vector2i position,
                                             l2d::NavigationCell2D cell);
        [[nodiscard]] bool setNavigationWalkable(sf::Vector2i position, bool walkable);
        [[nodiscard]] bool setNavigationTraversalCost(sf::Vector2i position, float traversalCost);
        [[nodiscard]] bool undoNavigation();
        [[nodiscard]] bool redoNavigation();
        [[nodiscard]] std::size_t navigationUndoCount() const noexcept;
        [[nodiscard]] std::size_t navigationRedoCount() const noexcept;

      private:
        struct NavigationCellEdit
        {
            sf::Vector2i position{0, 0};
            l2d::NavigationCell2D before;
            l2d::NavigationCell2D after;
        };

        [[nodiscard]] bool editColliderProperties(ColliderAuthoringKind kind,
                                                  sf::Vector2f offset);
        [[nodiscard]] bool publishNavigationCellEdit(sf::Vector2i position,
                                                     l2d::NavigationCell2D cell);
        void pushNavigationUndo(NavigationCellEdit edit);

        EditorDocument* m_document = nullptr;
        EditorCommandHistory* m_history = nullptr;
        ComponentInspectorModel m_inspector;
        std::optional<l2d::NavigationGrid2D> m_navigationGrid;
        std::deque<NavigationCellEdit> m_navigationUndo;
        std::deque<NavigationCellEdit> m_navigationRedo;
    };
}

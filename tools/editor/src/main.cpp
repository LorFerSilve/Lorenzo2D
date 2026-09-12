#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/SceneHierarchyModel.hpp>
#include <Lorenzo2DEditor/ViewportTransformModel.hpp>

#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    constexpr float HierarchyWidth = 330.f;
    constexpr float InspectorWidth = 500.f;
    constexpr float ViewportMargin = 12.f;

    bool loadEditorFont(sf::Font& font)
    {
        std::vector<std::filesystem::path> candidates;
#if defined(_WIN32)
        candidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
        candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
#elif defined(__APPLE__)
        candidates.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
        candidates.emplace_back("/System/Library/Fonts/SFNS.ttf");
#else
        candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
#endif

        for (const std::filesystem::path& candidate : candidates)
        {
            std::error_code error;
            if (!std::filesystem::exists(candidate, error) || error) continue;
            if (font.openFromFile(candidate.string())) return true;
        }
        return false;
    }

    std::string transformSummary(const l2d::TransformState& transform)
    {
        std::ostringstream output;
        output << "Position: " << transform.position.x << ", " << transform.position.y
               << "   Rotation: " << transform.rotation << "   Scale: " << transform.scale.x << ", "
               << transform.scale.y;
        return output.str();
    }

    sf::Vector2f pointerPosition(const sf::Vector2i& position)
    {
        return {static_cast<float>(position.x), static_cast<float>(position.y)};
    }

    class EditorApp final : public l2d::Application
    {
      public:
        explicit EditorApp(l2d_editor::EditorDocument document)
            : l2d::Application(1280u, 720u, "Lorenzo2D Editor"), m_document(std::move(document)),
              m_hierarchy(m_document), m_history(), m_inspector(m_document, m_history),
              m_viewport(m_document, m_history), m_hasFont(loadEditorFont(m_font))
        {
            if (!m_hasFont)
            {
                std::cerr << "Editor warning: no supported system font was found; panel text is "
                             "disabled.\n";
            }
            updateViewportBounds();
            updateWindowTitle();
        }

      protected:
        void onUpdate(float deltaTime) override
        {
            (void)deltaTime;
            updateViewportBounds();

            bool stateChanged = updateViewportInteraction();
            const bool dragging = m_viewport.isDragging();

            if (!dragging)
            {
                if (l2d::Input::wasKeyPressed(l2d::Key::Down))
                    stateChanged = m_hierarchy.selectNext() || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::Up))
                    stateChanged = m_hierarchy.selectPrevious() || stateChanged;

                if (l2d::Input::wasKeyPressed(l2d::Key::Space))
                {
                    const auto snapshot = m_inspector.snapshot();
                    if (snapshot)
                        stateChanged = m_inspector.setActive(!snapshot->active) || stateChanged;
                }

                if (l2d::Input::wasKeyPressed(l2d::Key::Left))
                    stateChanged = adjustZOrder(-1) || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::Right))
                    stateChanged = adjustZOrder(1) || stateChanged;

                if (l2d::Input::wasKeyPressed(l2d::Key::A))
                    stateChanged = nudgeSelected({-8.f, 0.f}) || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::D))
                    stateChanged = nudgeSelected({8.f, 0.f}) || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::W))
                    stateChanged = nudgeSelected({0.f, -8.f}) || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::S))
                    stateChanged = nudgeSelected({0.f, 8.f}) || stateChanged;

                if (l2d::Input::wasKeyPressed(l2d::Key::F1))
                    stateChanged = toggleRectangleRenderer() || stateChanged;

                if (l2d::Input::wasKeyPressed(l2d::Key::Z))
                    stateChanged = m_history.undo(m_document) || stateChanged;
                if (l2d::Input::wasKeyPressed(l2d::Key::Q))
                    stateChanged = m_history.redo(m_document) || stateChanged;
            }

            if (l2d::Input::wasKeyPressed(l2d::Key::Escape))
            {
                if (m_viewport.isDragging())
                    stateChanged = m_viewport.cancelTranslationDrag() || stateChanged;
                else
                    requestClose();
            }
            if (stateChanged) updateWindowTitle();
        }

        void onRender(sf::RenderWindow& window, float interpolationAlpha) override
        {
            (void)interpolationAlpha;
            drawViewport(window);
            drawPanels(window);
        }

      private:
        bool updateViewportInteraction()
        {
            const l2d::PointerState& pointer = l2d::Pointer::primary();
            bool changed = false;

            if (pointer.dragStarted && !m_viewport.isDragging())
            {
                if (m_viewport.beginTranslationDrag(pointerPosition(pointer.dragOrigin)))
                {
                    changed =
                        m_viewport.updateTranslationDrag(pointerPosition(pointer.screenPosition)) ||
                        changed;
                }
            }
            else if (pointer.dragging && m_viewport.isDragging())
            {
                changed = m_viewport.updateTranslationDrag(pointerPosition(pointer.screenPosition)) ||
                          changed;
            }

            if (pointer.dragEnded && m_viewport.isDragging())
                changed = m_viewport.endTranslationDrag() || changed;

            return changed;
        }

        void updateViewportBounds()
        {
            const sf::Vector2u windowSize = getWindow().getSize();
            const float width = static_cast<float>(windowSize.x);
            const float height = static_cast<float>(windowSize.y);
            const float viewportWidth =
                std::max(1.f, width - HierarchyWidth - InspectorWidth - ViewportMargin * 2.f);
            const float viewportHeight = std::max(1.f, height - ViewportMargin * 2.f);
            (void)m_viewport.setViewport({{HierarchyWidth + ViewportMargin, ViewportMargin},
                                          {viewportWidth, viewportHeight}});
        }

        bool adjustZOrder(std::int32_t delta)
        {
            const auto snapshot = m_inspector.snapshot();
            if (!snapshot) return false;

            if (delta < 0 && snapshot->zOrder == std::numeric_limits<std::int32_t>::min())
                return false;
            if (delta > 0 && snapshot->zOrder == std::numeric_limits<std::int32_t>::max())
                return false;
            return m_inspector.setZOrder(snapshot->zOrder + delta);
        }

        bool nudgeSelected(sf::Vector2f delta)
        {
            const auto snapshot = m_inspector.snapshot();
            if (!snapshot) return false;

            l2d::TransformState transform = snapshot->transform;
            transform.position += delta;
            return m_inspector.setTransform(transform);
        }

        bool toggleRectangleRenderer()
        {
            const auto snapshot = m_inspector.snapshot();
            if (!snapshot) return false;

            bool present = false;
            for (const l2d_editor::InspectorComponentEntry& component : snapshot->components)
            {
                if (component.kind == l2d_editor::InspectorComponentKind::RectangleRenderer)
                {
                    present = true;
                    break;
                }
            }

            if (present)
                return m_inspector.removeComponent(
                    l2d_editor::InspectorComponentKind::RectangleRenderer);
            return m_inspector.addComponent(l2d_editor::InspectorComponentKind::RectangleRenderer);
        }

        void drawViewport(sf::RenderWindow& window) const
        {
            const l2d_editor::EditorViewportRect& viewport = m_viewport.viewport();
            sf::RectangleShape background(viewport.size);
            background.setPosition(viewport.position);
            background.setFillColor(sf::Color(22, 24, 29));
            background.setOutlineThickness(1.f);
            background.setOutlineColor(sf::Color(55, 59, 68));
            window.draw(background);

            const sf::Vector2f origin = m_viewport.worldToViewport({0.f, 0.f});
            if (origin.x >= viewport.position.x &&
                origin.x <= viewport.position.x + viewport.size.x)
            {
                sf::RectangleShape vertical({1.f, viewport.size.y});
                vertical.setPosition({origin.x, viewport.position.y});
                vertical.setFillColor(sf::Color(50, 54, 63));
                window.draw(vertical);
            }
            if (origin.y >= viewport.position.y &&
                origin.y <= viewport.position.y + viewport.size.y)
            {
                sf::RectangleShape horizontal({viewport.size.x, 1.f});
                horizontal.setPosition({viewport.position.x, origin.y});
                horizontal.setFillColor(sf::Color(50, 54, 63));
                window.draw(horizontal);
            }

            for (const l2d_editor::EditorObjectRecord& object : m_document.objects())
            {
                const sf::Vector2f position =
                    m_viewport.worldToViewport(object.prefab.transform.position);
                if (!insideViewport(position)) continue;

                const bool selected = object.id == m_document.selectedObject();
                const float radius = selected ? 7.f : 4.f;
                sf::CircleShape marker(radius);
                marker.setOrigin({radius, radius});
                marker.setPosition(position);
                marker.setFillColor(selected ? sf::Color(220, 224, 235) : sf::Color(120, 126, 140));
                window.draw(marker);
            }

            const auto snapshot = m_viewport.snapshot();
            if (snapshot && insideViewport(snapshot->gizmoPosition))
            {
                drawTranslationGizmo(window, snapshot->gizmoPosition, snapshot->dragging);
            }

            if (m_hasFont)
            {
                drawText(window, "Viewport",
                         {viewport.position.x + 12.f, viewport.position.y + 10.f}, 18u,
                         sf::Color::White);
                drawText(window, "Drag the selected center handle to translate",
                         {viewport.position.x + 12.f, viewport.position.y + 34.f}, 12u,
                         sf::Color(155, 160, 171));
            }
        }

        void drawTranslationGizmo(sf::RenderWindow& window, sf::Vector2f center,
                                  bool dragging) const
        {
            sf::RectangleShape xAxis({42.f, 3.f});
            xAxis.setOrigin({0.f, 1.5f});
            xAxis.setPosition(center);
            xAxis.setFillColor(sf::Color(205, 92, 92));
            window.draw(xAxis);

            sf::RectangleShape yAxis({3.f, 42.f});
            yAxis.setOrigin({1.5f, 42.f});
            yAxis.setPosition(center);
            yAxis.setFillColor(sf::Color(96, 184, 118));
            window.draw(yAxis);

            constexpr float handleRadius = 8.f;
            sf::CircleShape handle(handleRadius);
            handle.setOrigin({handleRadius, handleRadius});
            handle.setPosition(center);
            handle.setFillColor(dragging ? sf::Color(245, 210, 105) : sf::Color(105, 160, 235));
            window.draw(handle);
        }

        bool insideViewport(sf::Vector2f position) const noexcept
        {
            const l2d_editor::EditorViewportRect& viewport = m_viewport.viewport();
            return position.x >= viewport.position.x && position.y >= viewport.position.y &&
                   position.x <= viewport.position.x + viewport.size.x &&
                   position.y <= viewport.position.y + viewport.size.y;
        }

        void drawPanels(sf::RenderWindow& window) const
        {
            const sf::Vector2u windowSize = window.getSize();
            const float height = static_cast<float>(windowSize.y);

            sf::RectangleShape hierarchyPanel({HierarchyWidth, height});
            hierarchyPanel.setPosition({0.f, 0.f});
            hierarchyPanel.setFillColor(sf::Color(30, 32, 38));
            window.draw(hierarchyPanel);

            sf::RectangleShape inspectorPanel({InspectorWidth, height});
            inspectorPanel.setPosition({static_cast<float>(windowSize.x) - InspectorWidth, 0.f});
            inspectorPanel.setFillColor(sf::Color(35, 37, 44));
            window.draw(inspectorPanel);

            if (!m_hasFont) return;

            drawText(window, "Scene Hierarchy", {18.f, 14.f}, 20u, sf::Color::White);
            float hierarchyY = 52.f;
            for (const l2d_editor::SceneHierarchyItem& item : m_hierarchy.items())
            {
                if (hierarchyY > height - 28.f) break;
                if (item.selected)
                {
                    sf::RectangleShape selection({HierarchyWidth - 20.f, 25.f});
                    selection.setPosition({10.f, hierarchyY - 3.f});
                    selection.setFillColor(sf::Color(65, 83, 115));
                    window.draw(selection);
                }
                drawText(window, item.name, {18.f, hierarchyY}, 16u,
                         item.selected ? sf::Color::White : sf::Color(205, 208, 215));
                hierarchyY += 28.f;
            }

            const float inspectorX = static_cast<float>(windowSize.x) - InspectorWidth + 18.f;
            drawText(window, "Component Inspector", {inspectorX, 14.f}, 20u, sf::Color::White);

            const auto snapshot = m_inspector.snapshot();
            if (!snapshot)
            {
                drawText(window, "No object selected", {inspectorX, 54.f}, 16u,
                         sf::Color(185, 188, 196));
                drawHelp(window, inspectorX, height);
                return;
            }

            float y = 52.f;
            drawText(window, "Name: " + snapshot->name, {inspectorX, y}, 16u, sf::Color::White);
            y += 25.f;
            drawText(window, "Tag: " + snapshot->tag, {inspectorX, y}, 15u,
                     sf::Color(210, 212, 218));
            y += 23.f;
            drawText(window, std::string("Active: ") + (snapshot->active ? "true" : "false"),
                     {inspectorX, y}, 15u, sf::Color(210, 212, 218));
            y += 23.f;
            drawText(window, "Z-order: " + std::to_string(snapshot->zOrder), {inspectorX, y}, 15u,
                     sf::Color(210, 212, 218));
            y += 28.f;
            drawText(window, transformSummary(snapshot->transform), {inspectorX, y}, 14u,
                     sf::Color(190, 194, 204));
            y += 34.f;
            drawText(window, "Components", {inspectorX, y}, 17u, sf::Color::White);
            y += 28.f;

            for (const l2d_editor::InspectorComponentEntry& component : snapshot->components)
            {
                if (y > height - 130.f) break;
                drawText(window, "- " + component.displayName, {inspectorX + 8.f, y}, 14u,
                         component.removable ? sf::Color(205, 208, 215)
                                             : sf::Color(160, 178, 210));
                y += 22.f;
            }

            drawHelp(window, inspectorX, height);
        }

        void drawHelp(sf::RenderWindow& window, float x, float height) const
        {
            const float y = height - 128.f;
            drawText(window, "Viewport: drag selected center handle to move", {x, y}, 13u,
                     sf::Color(160, 164, 174));
            drawText(window, "Up/Down select | A/D/W/S nudge | Left/Right z-order", {x, y + 22.f},
                     13u, sf::Color(160, 164, 174));
            drawText(window, "Space active | F1 rectangle | Z undo | Q redo", {x, y + 44.f}, 13u,
                     sf::Color(160, 164, 174));
            drawText(window, "Escape cancels drag, otherwise closes", {x, y + 66.f}, 13u,
                     sf::Color(160, 164, 174));
        }

        void drawText(sf::RenderWindow& window, const std::string& value, sf::Vector2f position,
                      unsigned int size, sf::Color color) const
        {
            sf::Text text(m_font, value, size);
            text.setPosition(position);
            text.setFillColor(color);
            window.draw(text);
        }

        void updateWindowTitle()
        {
            std::string title = "Lorenzo2D Editor - " + m_document.name();
            const auto snapshot = m_inspector.snapshot();
            if (snapshot)
            {
                title += " - selected: " + snapshot->name + " [" +
                         std::to_string(snapshot->components.size()) + " components]";
            }
            if (m_viewport.isDragging()) title += " - translating";
            getWindow().setTitle(title);
        }

        l2d_editor::EditorDocument m_document;
        l2d_editor::SceneHierarchyModel m_hierarchy;
        l2d_editor::EditorCommandHistory m_history;
        l2d_editor::ComponentInspectorModel m_inspector;
        l2d_editor::ViewportTransformModel m_viewport;
        sf::Font m_font;
        bool m_hasFont = false;
    };
}

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: Lorenzo2DEditor [level.l2dlevel]\n";
        return 2;
    }

    l2d_editor::EditorDocument document;
    if (argc == 2 && !document.loadFromFile(argv[1]))
    {
        std::cerr << "Unable to open level: " << argv[1] << '\n';
        return 3;
    }

    std::cout << "Lorenzo2D Editor Phase 13.3 viewport transform foundation\n"
              << "Up/Down select; drag the selected viewport handle to translate;\n"
              << "A/D/W/S nudge; Left/Right z-order; Space active; F1 rectangle;\n"
              << "Z undo; Q redo; Escape cancels an active drag or closes.\n";

    EditorApp app(std::move(document));
    app.run();
    return 0;
}

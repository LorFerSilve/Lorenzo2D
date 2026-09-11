#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/SceneHierarchyModel.hpp>

#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>

#include <SFML/Graphics.hpp>

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
               << "   Rotation: " << transform.rotation << "   Scale: " << transform.scale.x
               << ", " << transform.scale.y;
        return output.str();
    }

    class EditorApp final : public l2d::Application
    {
      public:
        explicit EditorApp(l2d_editor::EditorDocument document)
            : l2d::Application(1280u, 720u, "Lorenzo2D Editor"), m_document(std::move(document)),
              m_hierarchy(m_document), m_history(), m_inspector(m_document, m_history),
              m_hasFont(loadEditorFont(m_font))
        {
            if (!m_hasFont)
            {
                std::cerr << "Editor warning: no supported system font was found; panel text is "
                             "disabled.\n";
            }
            updateWindowTitle();
        }

      protected:
        void onUpdate(float deltaTime) override
        {
            (void)deltaTime;

            bool stateChanged = false;
            if (l2d::Input::wasKeyPressed(l2d::Key::Down))
                stateChanged = m_hierarchy.selectNext() || stateChanged;
            if (l2d::Input::wasKeyPressed(l2d::Key::Up))
                stateChanged = m_hierarchy.selectPrevious() || stateChanged;

            if (l2d::Input::wasKeyPressed(l2d::Key::Space))
            {
                const auto snapshot = m_inspector.snapshot();
                if (snapshot) stateChanged = m_inspector.setActive(!snapshot->active) || stateChanged;
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

            if (l2d::Input::wasKeyPressed(l2d::Key::Escape)) requestClose();
            if (stateChanged) updateWindowTitle();
        }

        void onRender(sf::RenderWindow& window, float interpolationAlpha) override
        {
            (void)interpolationAlpha;
            drawPanels(window);
        }

      private:
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

        void drawPanels(sf::RenderWindow& window) const
        {
            const sf::Vector2u windowSize = window.getSize();
            const float height = static_cast<float>(windowSize.y);
            constexpr float hierarchyWidth = 330.f;
            constexpr float inspectorWidth = 500.f;

            sf::RectangleShape hierarchyPanel({hierarchyWidth, height});
            hierarchyPanel.setPosition({0.f, 0.f});
            hierarchyPanel.setFillColor(sf::Color(30, 32, 38));
            window.draw(hierarchyPanel);

            sf::RectangleShape inspectorPanel({inspectorWidth, height});
            inspectorPanel.setPosition({static_cast<float>(windowSize.x) - inspectorWidth, 0.f});
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
                    sf::RectangleShape selection({hierarchyWidth - 20.f, 25.f});
                    selection.setPosition({10.f, hierarchyY - 3.f});
                    selection.setFillColor(sf::Color(65, 83, 115));
                    window.draw(selection);
                }
                drawText(window, item.name, {18.f, hierarchyY}, 16u,
                         item.selected ? sf::Color::White : sf::Color(205, 208, 215));
                hierarchyY += 28.f;
            }

            const float inspectorX = static_cast<float>(windowSize.x) - inspectorWidth + 18.f;
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
            const float y = height - 112.f;
            drawText(window, "Up/Down select | A/D/W/S move | Left/Right z-order", {x, y}, 13u,
                     sf::Color(160, 164, 174));
            drawText(window, "Space active | F1 rectangle component | Z undo | Q redo", {x, y + 22.f},
                     13u, sf::Color(160, 164, 174));
            drawText(window, "Escape close", {x, y + 44.f}, 13u, sf::Color(160, 164, 174));
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
            getWindow().setTitle(title);
        }

        l2d_editor::EditorDocument m_document;
        l2d_editor::SceneHierarchyModel m_hierarchy;
        l2d_editor::EditorCommandHistory m_history;
        l2d_editor::ComponentInspectorModel m_inspector;
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

    std::cout << "Lorenzo2D Editor Phase 13.2 component inspector\n"
              << "Up/Down select; A/D/W/S move; Left/Right z-order; Space active;\n"
              << "F1 toggles RectangleRenderer; Z undo; Q redo; Escape closes.\n";

    EditorApp app(std::move(document));
    app.run();
    return 0;
}

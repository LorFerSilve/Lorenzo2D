#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/SceneHierarchyModel.hpp>

#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

#include <iostream>
#include <string>
#include <utility>

namespace
{
    class EditorApp final : public l2d::Application
    {
      public:
        explicit EditorApp(l2d_editor::EditorDocument document)
            : l2d::Application(1280u, 720u, "Lorenzo2D Editor"),
              m_document(std::move(document)), m_hierarchy(m_document)
        {
            updateWindowTitle();
        }

      protected:
        void onUpdate(float deltaTime) override
        {
            (void)deltaTime;

            bool selectionChanged = false;
            if (l2d::Input::wasKeyPressed(l2d::Key::Down))
                selectionChanged = m_hierarchy.selectNext();
            if (l2d::Input::wasKeyPressed(l2d::Key::Up))
                selectionChanged = m_hierarchy.selectPrevious() || selectionChanged;
            if (l2d::Input::wasKeyPressed(l2d::Key::Escape)) requestClose();

            if (selectionChanged) updateWindowTitle();
        }

      private:
        void updateWindowTitle()
        {
            std::string title = "Lorenzo2D Editor - " + m_document.name();
            const l2d_editor::EditorObjectRecord* selected =
                m_document.findObject(m_document.selectedObject());
            if (selected != nullptr) title += " - selected: " + selected->prefab.name;
            getWindow().setTitle(title);
        }

        l2d_editor::EditorDocument m_document;
        l2d_editor::SceneHierarchyModel m_hierarchy;
        l2d_editor::EditorCommandHistory m_history;
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

    std::cout << "Lorenzo2D Editor 13.1 foundation\n"
              << "Up/Down: change hierarchy selection, Escape: close\n";

    EditorApp app(std::move(document));
    app.run();
    return 0;
}

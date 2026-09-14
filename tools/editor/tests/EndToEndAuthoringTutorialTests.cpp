#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>
#include <Lorenzo2DEditor/PlayTestWorkflowModel.hpp>

#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    std::filesystem::path makeTempDirectory()
    {
        std::error_code error;
        std::filesystem::path root = std::filesystem::temp_directory_path(error);
        require(!error, "temporary directory is unavailable");
        root /= "lorenzo2d-editor-authoring-tutorial";
        std::filesystem::remove_all(root, error);
        error.clear();
        std::filesystem::create_directories(root, error);
        require(!error, "unable to create tutorial temporary directory");
        return root;
    }

    l2d::Prefab makeObject(std::string name, sf::Vector2f position)
    {
        l2d::Prefab prefab;
        prefab.name = std::move(name);
        prefab.transform.position = position;
        return prefab;
    }

    void tutorialCreatesSavesAndRunsWithoutJsonEditing()
    {
        const std::filesystem::path root = makeTempDirectory();
        const std::filesystem::path authoredLevel = root / "tutorial.l2dlevel";

        l2d_editor::EditorDocument document;
        l2d_editor::EditorCommandHistory history;
        require(document.setName("Phase 13 Tutorial"), "tutorial level name should be valid");

        l2d_editor::EditorObjectId playerId = l2d_editor::InvalidEditorObjectId;
        const auto createPlayer = [&playerId](l2d_editor::EditorDocument& editor)
        {
            const auto created = editor.addObject(makeObject("Player", {64.f, 96.f}));
            if (!created) return false;
            playerId = *created;
            return editor.selectObject(playerId);
        };
        require(history.execute(document, "Create Player", createPlayer),
                "tutorial should create and select the Player through editor APIs");

        const auto createGoal = [](l2d_editor::EditorDocument& editor)
        { return editor.addObject(makeObject("Goal", {256.f, 96.f})).has_value(); };
        require(history.execute(document, "Create Goal", createGoal),
                "tutorial should create the Goal through editor APIs");

        const auto movePlayer = [playerId](l2d_editor::EditorDocument& editor)
        {
            const l2d_editor::EditorObjectRecord* player = editor.findObject(playerId);
            if (player == nullptr) return false;
            l2d::TransformState transform = player->prefab.transform;
            transform.position = {96.f, 128.f};
            return editor.setObjectTransform(playerId, transform);
        };
        require(history.execute(document, "Move Player", movePlayer),
                "tutorial should edit the Player transform through command history");

        require(document.saveToFile(authoredLevel.string()),
                "tutorial should save through the runtime level serializer");

        l2d::LevelDocument runtimeLevel;
        require(l2d::LevelSerializer::loadFromFile(authoredLevel.string(), runtimeLevel),
                "saved tutorial level should be readable by a runtime-only LevelSerializer path");
        require(runtimeLevel.name == "Phase 13 Tutorial",
                "runtime level name should match authoring state");
        require(runtimeLevel.objects.size() == 2u,
                "runtime level should contain both authored objects");
        require(runtimeLevel.objects[0].name == "Player" && runtimeLevel.objects[1].name == "Goal",
                "runtime level should preserve authored object order and names");
        require(runtimeLevel.objects[0].transform.position == sf::Vector2f{96.f, 128.f},
                "runtime level should contain the edited Player transform");

        l2d_editor::PlayTestWorkflowModel workflow;
        require(workflow.configure("tutorial-game", root, {"--level", "{level}"}),
                "tutorial play/test configuration should be valid");

        bool launchObserved = false;
        bool stopObserved = false;
        l2d_editor::PlayTestProcessHooks hooks;
        hooks.launch = [&launchObserved](const l2d_editor::PlayTestLaunchRequest& request)
        {
            l2d::LevelDocument launchedLevel;
            const bool loaded = l2d::LevelSerializer::loadFromFile(
                request.levelSnapshotPath.string(), launchedLevel);
            if (!loaded) return false;
            if (launchedLevel.name != "Phase 13 Tutorial" || launchedLevel.objects.size() != 2u)
                return false;
            if (launchedLevel.objects[0].name != "Player" ||
                launchedLevel.objects[0].transform.position != sf::Vector2f{96.f, 128.f})
                return false;
            if (launchedLevel.objects[1].name != "Goal") return false;
            launchObserved = true;
            return true;
        };
        hooks.stop = [&stopObserved]()
        {
            stopObserved = true;
            return true;
        };

        require(workflow.start(document, root / "playtest-snapshots", std::move(hooks)),
                "tutorial should publish the current authoring state to play/test");
        require(launchObserved,
                "play/test launcher should observe a runtime-readable authored level");
        require(workflow.isActive(), "tutorial play/test session should become active");
        require(workflow.stop(), "tutorial play/test session should stop cleanly");
        require(stopObserved, "tutorial stop hook should be called");
        require(!workflow.isActive(),
                "tutorial play/test session should become inactive after stop");

        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
}

int main()
{
    try
    {
        tutorialCreatesSavesAndRunsWithoutJsonEditing();
        std::cout << "End-to-end authoring tutorial regression passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

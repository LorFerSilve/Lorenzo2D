#include <Lorenzo2DEditor/PlayTestWorkflowModel.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    int failures = 0;

    void expect(bool condition, const char* message)
    {
        if (condition) return;
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }

    std::filesystem::path makeTempDirectory(const char* suffix)
    {
        std::error_code error;
        std::filesystem::path root = std::filesystem::temp_directory_path(error);
        if (error) return {};
        root /= std::string("lorenzo2d-editor-playtest-") + suffix;
        std::filesystem::remove_all(root, error);
        error.clear();
        std::filesystem::create_directories(root, error);
        return error ? std::filesystem::path{} : root;
    }

    l2d_editor::EditorDocument makeDocument()
    {
        l2d_editor::EditorDocument document;
        const bool renamed = document.setName("PlayTestLevel");
        expect(renamed, "test document should accept a valid name");
        return document;
    }

    void configurationValidationIsBounded()
    {
        l2d_editor::PlayTestWorkflowModel workflow;
        expect(!workflow.configure({}, ".", {"{level}"}), "empty executable must be rejected");
        expect(workflow.lastError() == l2d_editor::PlayTestError::InvalidExecutable,
               "empty executable should report InvalidExecutable");

        expect(!workflow.configure("game", ".", {"--level"}),
               "configuration without level placeholder must be rejected");
        expect(workflow.lastError() == l2d_editor::PlayTestError::MissingLevelPlaceholder,
               "missing placeholder should report MissingLevelPlaceholder");

        std::vector<std::string> tooMany(
            l2d_editor::PlayTestWorkflowModel::MaximumArgumentCount + 1u, "x");
        tooMany.front() = "{level}";
        expect(!workflow.configure("game", ".", std::move(tooMany)),
               "argument-count limit must be enforced");
        expect(workflow.lastError() == l2d_editor::PlayTestError::TooManyArguments,
               "argument-count overflow should report TooManyArguments");
    }

    void successfulSessionUsesSerializedSnapshot()
    {
        const std::filesystem::path directory = makeTempDirectory("success");
        expect(!directory.empty(), "temporary directory should be available");

        l2d_editor::EditorDocument document = makeDocument();
        l2d_editor::PlayTestWorkflowModel workflow;
        expect(workflow.configure("sample-game", directory, {"--level={level}", "--playtest"}),
               "valid play/test configuration should be accepted");

        l2d_editor::PlayTestLaunchRequest captured;
        bool stopCalled = false;
        l2d_editor::PlayTestProcessHooks hooks;
        hooks.launch = [&captured](const l2d_editor::PlayTestLaunchRequest& request)
        {
            captured = request;
            return true;
        };
        hooks.stop = [&stopCalled]()
        {
            stopCalled = true;
            return true;
        };

        expect(workflow.start(document, directory, std::move(hooks)),
               "valid play/test session should start");
        expect(workflow.isActive(), "workflow should report an active session after launch");
        expect(captured.executable == std::filesystem::path("sample-game"),
               "launch request should preserve configured executable");
        expect(captured.arguments.size() == 2u, "launch request should preserve argument count");
        expect(captured.arguments.front().find("{level}") == std::string::npos,
               "launch request should expand the level placeholder");
        expect(captured.arguments.front().find(captured.levelSnapshotPath.string()) !=
                   std::string::npos,
               "expanded argument should contain the serialized snapshot path");
        expect(std::filesystem::is_regular_file(captured.levelSnapshotPath),
               "play/test start should publish a serialized level snapshot");

        l2d_editor::EditorDocument restored;
        expect(
            restored.loadFromFile(captured.levelSnapshotPath.string()),
            "runtime level snapshot should remain readable by EditorDocument/runtime serializer");
        expect(restored.name() == "PlayTestLevel", "snapshot should reflect current editor state");

        expect(workflow.stop(), "active play/test session should stop cleanly");
        expect(stopCalled, "stop hook should be invoked exactly through workflow stop");
        expect(!workflow.isActive(), "workflow should become inactive after stop");
        expect(!std::filesystem::exists(captured.levelSnapshotPath),
               "successful stop should remove the temporary snapshot");

        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    void failedLaunchRollsBackSnapshotAndState()
    {
        const std::filesystem::path directory = makeTempDirectory("launch-failure");
        l2d_editor::EditorDocument document = makeDocument();
        l2d_editor::PlayTestWorkflowModel workflow;
        expect(workflow.configure("sample-game", directory, {"{level}"}),
               "valid configuration should be accepted before launch failure test");

        std::filesystem::path attemptedSnapshot;
        l2d_editor::PlayTestProcessHooks hooks;
        hooks.launch = [&attemptedSnapshot](const l2d_editor::PlayTestLaunchRequest& request)
        {
            attemptedSnapshot = request.levelSnapshotPath;
            return false;
        };
        hooks.stop = []() { return true; };

        expect(!workflow.start(document, directory, std::move(hooks)),
               "launcher rejection should fail the session transactionally");
        expect(workflow.lastError() == l2d_editor::PlayTestError::LaunchRejected,
               "launcher rejection should report LaunchRejected");
        expect(!workflow.isActive(), "failed launch must not publish active session state");
        expect(!attemptedSnapshot.empty(), "launcher should receive the attempted snapshot path");
        expect(!std::filesystem::exists(attemptedSnapshot),
               "failed launch should remove its serialized snapshot");

        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    void activeSessionRejectsMutationAndSecondLaunch()
    {
        const std::filesystem::path directory = makeTempDirectory("exclusive");
        l2d_editor::EditorDocument document = makeDocument();
        l2d_editor::PlayTestWorkflowModel workflow;
        expect(workflow.configure("sample-game", directory, {"{level}"}),
               "valid configuration should be accepted before exclusivity test");

        l2d_editor::PlayTestProcessHooks hooks;
        hooks.launch = [](const l2d_editor::PlayTestLaunchRequest&) { return true; };
        hooks.stop = []() { return true; };
        expect(workflow.start(document, directory, hooks), "first session should start");
        expect(!workflow.configure("other-game", directory, {"{level}"}),
               "configuration mutation must be rejected while active");
        expect(workflow.lastError() == l2d_editor::PlayTestError::SessionAlreadyActive,
               "active configuration mutation should report SessionAlreadyActive");
        expect(!workflow.start(document, directory, std::move(hooks)),
               "second launch must be rejected while active");
        expect(workflow.stop(), "exclusive session should still stop cleanly");

        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
}

int main()
{
    configurationValidationIsBounded();
    successfulSessionUsesSerializedSnapshot();
    failedLaunchRollsBackSnapshotAndState();
    activeSessionRejectsMutationAndSecondLaunch();

    if (failures != 0)
    {
        std::cerr << failures << " play/test workflow regression(s) failed\n";
        return 1;
    }

    std::cout << "Play/test workflow regressions passed\n";
    return 0;
}

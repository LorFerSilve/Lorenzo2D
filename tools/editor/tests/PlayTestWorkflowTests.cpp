#include <Lorenzo2DEditor/PlayTestWorkflowModel.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    static_assert(!std::is_copy_constructible_v<l2d_editor::PlayTestWorkflowModel>);
    static_assert(!std::is_copy_assignable_v<l2d_editor::PlayTestWorkflowModel>);
    static_assert(!std::is_move_constructible_v<l2d_editor::PlayTestWorkflowModel>);
    static_assert(!std::is_move_assignable_v<l2d_editor::PlayTestWorkflowModel>);

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

    l2d_editor::PlayTestProcessHooks acceptingHooks(l2d_editor::PlayTestLaunchRequest& captured,
                                                     bool& stopCalled)
    {
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
        return hooks;
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

        expect(!workflow.configure("game", ".", {"--levels={level},{level}"}),
               "configuration with repeated placeholder occurrences must be rejected");
        expect(workflow.lastError() == l2d_editor::PlayTestError::MissingLevelPlaceholder,
               "repeated placeholders should report MissingLevelPlaceholder");

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
        expect(workflow.start(document, directory, acceptingHooks(captured, stopCalled)),
               "valid play/test session should start");
        expect(workflow.isActive(), "workflow should report an active session after launch");
        expect(captured.executable == std::filesystem::path("sample-game"),
               "launch request should preserve configured executable");
        expect(captured.arguments.size() == 2u, "launch request should preserve argument count");
        expect(captured.arguments.front().find("{level}") == std::string::npos,
               "launch request should expand the level placeholder");
        expect(captured.levelSnapshotPath.is_absolute(),
               "launch request should publish an absolute snapshot path");
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

        const std::filesystem::path sessionDirectory = captured.levelSnapshotPath.parent_path();
        expect(workflow.stop(), "active play/test session should stop cleanly");
        expect(stopCalled, "stop hook should be invoked exactly through workflow stop");
        expect(!workflow.isActive(), "workflow should become inactive after stop");
        expect(!std::filesystem::exists(captured.levelSnapshotPath),
               "successful stop should remove the temporary snapshot");
        expect(!std::filesystem::exists(sessionDirectory),
               "successful stop should remove the workflow-owned session directory");

        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    void concurrentWorkflowsOwnDistinctSnapshots()
    {
        const std::filesystem::path directory = makeTempDirectory("distinct");
        l2d_editor::EditorDocument document = makeDocument();
        l2d_editor::PlayTestWorkflowModel first;
        l2d_editor::PlayTestWorkflowModel second;
        expect(first.configure("sample-game", directory, {"{level}"}),
               "first workflow should accept valid configuration");
        expect(second.configure("sample-game", directory, {"{level}"}),
               "second workflow should accept valid configuration");

        l2d_editor::PlayTestLaunchRequest firstRequest;
        l2d_editor::PlayTestLaunchRequest secondRequest;
        bool firstStopped = false;
        bool secondStopped = false;
        expect(first.start(document, directory, acceptingHooks(firstRequest, firstStopped)),
               "first workflow should start");
        expect(second.start(document, directory, acceptingHooks(secondRequest, secondStopped)),
               "second workflow should start in the same snapshot root");
        expect(firstRequest.levelSnapshotPath != secondRequest.levelSnapshotPath,
               "concurrent workflows must own distinct snapshot paths");
        expect(std::filesystem::is_regular_file(firstRequest.levelSnapshotPath),
               "first workflow snapshot should exist");
        expect(std::filesystem::is_regular_file(secondRequest.levelSnapshotPath),
               "second workflow snapshot should exist");

        expect(first.stop(), "first workflow should stop cleanly");
        expect(firstStopped, "first stop hook should run");
        expect(!std::filesystem::exists(firstRequest.levelSnapshotPath),
               "first stop should remove only the first workflow snapshot");
        expect(std::filesystem::is_regular_file(secondRequest.levelSnapshotPath),
               "first stop must not remove the second workflow snapshot");

        expect(second.stop(), "second workflow should stop cleanly");
        expect(secondStopped, "second stop hook should run");
        expect(!std::filesystem::exists(secondRequest.levelSnapshotPath),
               "second stop should remove the second workflow snapshot");

        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    void relativeSnapshotRootPublishesAbsolutePath()
    {
        std::error_code error;
        const std::filesystem::path originalDirectory = std::filesystem::current_path(error);
        expect(!error, "current directory should be readable");
        const std::filesystem::path directory = makeTempDirectory("relative-root");
        if (error || directory.empty()) return;

        std::filesystem::current_path(directory, error);
        expect(!error, "test should be able to enter its temporary directory");
        if (error) return;

        l2d_editor::EditorDocument document = makeDocument();
        l2d_editor::PlayTestWorkflowModel workflow;
        expect(workflow.configure("sample-game", "different-working-directory", {"{level}"}),
               "relative-root test configuration should be accepted");

        l2d_editor::PlayTestLaunchRequest captured;
        bool stopCalled = false;
        expect(workflow.start(document, "snapshots", acceptingHooks(captured, stopCalled)),
               "workflow should start from a relative snapshot root");
        expect(captured.levelSnapshotPath.is_absolute(),
               "relative snapshot roots must be normalized to an absolute launch path");
        expect(workflow.stop(), "relative-root workflow should stop cleanly");

        std::filesystem::current_path(originalDirectory, error);
        expect(!error, "test should restore the original current directory");
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
        expect(!std::filesystem::exists(attemptedSnapshot.parent_path()),
               "failed launch should remove its workflow-owned session directory");

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
    concurrentWorkflowsOwnDistinctSnapshots();
    relativeSnapshotRootPublishesAbsolutePath();
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

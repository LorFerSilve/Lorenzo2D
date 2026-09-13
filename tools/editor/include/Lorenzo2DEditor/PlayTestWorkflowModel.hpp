#pragma once

#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace l2d_editor
{
    enum class PlayTestError
    {
        None,
        InvalidExecutable,
        InvalidWorkingDirectory,
        TooManyArguments,
        ArgumentDataTooLarge,
        MissingLevelPlaceholder,
        SessionAlreadyActive,
        SnapshotDirectoryUnavailable,
        SnapshotWriteFailed,
        LaunchRejected,
        StopRejected,
        SnapshotCleanupFailed
    };

    struct PlayTestLaunchRequest
    {
        std::filesystem::path executable;
        std::filesystem::path workingDirectory;
        std::filesystem::path levelSnapshotPath;
        std::vector<std::string> arguments;
    };

    struct PlayTestProcessHooks
    {
        std::function<bool(const PlayTestLaunchRequest&)> launch;
        std::function<bool()> stop;
    };

    // Editor-only play/test orchestration. The model snapshots the current
    // EditorDocument through the public runtime level serializer, substitutes
    // that snapshot path into a caller-configured command line, and delegates
    // process ownership to injected platform/application hooks. No engine code
    // depends on this type and no editor-private persistence format is created.
    class PlayTestWorkflowModel
    {
      public:
        static constexpr std::size_t MaximumArgumentCount = 32u;
        static constexpr std::size_t MaximumArgumentBytes = 4096u;
        static constexpr const char* LevelPlaceholder = "{level}";

        [[nodiscard]] bool configure(std::filesystem::path executable,
                                     std::filesystem::path workingDirectory,
                                     std::vector<std::string> arguments);

        [[nodiscard]] bool start(const EditorDocument& document,
                                 const std::filesystem::path& snapshotDirectory,
                                 PlayTestProcessHooks hooks);
        [[nodiscard]] bool stop();

        [[nodiscard]] bool isActive() const noexcept;
        [[nodiscard]] const PlayTestLaunchRequest* activeRequest() const noexcept;
        [[nodiscard]] PlayTestError lastError() const noexcept;

      private:
        [[nodiscard]] bool validateConfiguration(const std::filesystem::path& executable,
                                                 const std::filesystem::path& workingDirectory,
                                                 const std::vector<std::string>& arguments);
        [[nodiscard]] bool cleanupSnapshot();
        [[nodiscard]] std::vector<std::string> expandArguments(
            const std::filesystem::path& snapshotPath) const;

        std::filesystem::path m_executable;
        std::filesystem::path m_workingDirectory;
        std::vector<std::string> m_arguments;
        PlayTestLaunchRequest m_activeRequest;
        std::function<bool()> m_stopHook;
        bool m_active = false;
        PlayTestError m_lastError = PlayTestError::None;
    };

    [[nodiscard]] const char* playTestErrorMessage(PlayTestError error) noexcept;
}

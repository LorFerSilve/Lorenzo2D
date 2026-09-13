#include <Lorenzo2DEditor/PlayTestWorkflowModel.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace l2d_editor
{
    namespace
    {
        constexpr const char* SnapshotFilename = "lorenzo2d-editor-playtest.level.json";

        bool containsNull(const std::string& value) noexcept
        {
            return value.find('\0') != std::string::npos;
        }

        std::size_t argumentBytes(const std::vector<std::string>& arguments) noexcept
        {
            std::size_t total = 0u;
            for (const std::string& argument : arguments)
            {
                if (argument.size() > PlayTestWorkflowModel::MaximumArgumentBytes - total)
                    return PlayTestWorkflowModel::MaximumArgumentBytes + 1u;
                total += argument.size();
            }
            return total;
        }
    }

    bool PlayTestWorkflowModel::configure(std::filesystem::path executable,
                                          std::filesystem::path workingDirectory,
                                          std::vector<std::string> arguments)
    {
        if (m_active)
        {
            m_lastError = PlayTestError::SessionAlreadyActive;
            return false;
        }
        if (!validateConfiguration(executable, workingDirectory, arguments)) return false;

        m_executable = std::move(executable);
        m_workingDirectory = std::move(workingDirectory);
        m_arguments = std::move(arguments);
        m_lastError = PlayTestError::None;
        return true;
    }

    bool PlayTestWorkflowModel::start(const EditorDocument& document,
                                      const std::filesystem::path& snapshotDirectory,
                                      PlayTestProcessHooks hooks)
    {
        if (m_active)
        {
            m_lastError = PlayTestError::SessionAlreadyActive;
            return false;
        }
        if (!hooks.launch || !hooks.stop ||
            !validateConfiguration(m_executable, m_workingDirectory, m_arguments))
        {
            if (m_lastError == PlayTestError::None) m_lastError = PlayTestError::LaunchRejected;
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(snapshotDirectory, error);
        if (error || !std::filesystem::is_directory(snapshotDirectory, error) || error)
        {
            m_lastError = PlayTestError::SnapshotDirectoryUnavailable;
            return false;
        }

        const std::filesystem::path snapshotPath = snapshotDirectory / SnapshotFilename;
        if (!document.saveToFile(snapshotPath.string()))
        {
            m_lastError = PlayTestError::SnapshotWriteFailed;
            return false;
        }

        PlayTestLaunchRequest request;
        request.executable = m_executable;
        request.workingDirectory = m_workingDirectory;
        request.levelSnapshotPath = snapshotPath;
        request.arguments = expandArguments(snapshotPath);

        if (!hooks.launch(request))
        {
            std::filesystem::remove(snapshotPath, error);
            m_lastError = PlayTestError::LaunchRejected;
            return false;
        }

        m_activeRequest = std::move(request);
        m_stopHook = std::move(hooks.stop);
        m_active = true;
        m_lastError = PlayTestError::None;
        return true;
    }

    bool PlayTestWorkflowModel::stop()
    {
        if (!m_active)
        {
            m_lastError = PlayTestError::None;
            return true;
        }
        if (!m_stopHook || !m_stopHook())
        {
            m_lastError = PlayTestError::StopRejected;
            return false;
        }

        m_active = false;
        m_stopHook = {};
        if (!cleanupSnapshot()) return false;

        m_activeRequest = {};
        m_lastError = PlayTestError::None;
        return true;
    }

    bool PlayTestWorkflowModel::isActive() const noexcept
    {
        return m_active;
    }

    const PlayTestLaunchRequest* PlayTestWorkflowModel::activeRequest() const noexcept
    {
        return m_active ? &m_activeRequest : nullptr;
    }

    PlayTestError PlayTestWorkflowModel::lastError() const noexcept
    {
        return m_lastError;
    }

    bool PlayTestWorkflowModel::validateConfiguration(const std::filesystem::path& executable,
                                                       const std::filesystem::path& workingDirectory,
                                                       const std::vector<std::string>& arguments)
    {
        if (executable.empty() || containsNull(executable.string()))
        {
            m_lastError = PlayTestError::InvalidExecutable;
            return false;
        }
        if (workingDirectory.empty() || containsNull(workingDirectory.string()))
        {
            m_lastError = PlayTestError::InvalidWorkingDirectory;
            return false;
        }
        if (arguments.size() > MaximumArgumentCount)
        {
            m_lastError = PlayTestError::TooManyArguments;
            return false;
        }
        if (argumentBytes(arguments) > MaximumArgumentBytes ||
            std::any_of(arguments.begin(), arguments.end(), containsNull))
        {
            m_lastError = PlayTestError::ArgumentDataTooLarge;
            return false;
        }

        const std::size_t placeholderCount = static_cast<std::size_t>(
            std::count_if(arguments.begin(), arguments.end(), [](const std::string& argument)
                          { return argument.find(LevelPlaceholder) != std::string::npos; }));
        if (placeholderCount != 1u)
        {
            m_lastError = PlayTestError::MissingLevelPlaceholder;
            return false;
        }

        m_lastError = PlayTestError::None;
        return true;
    }

    bool PlayTestWorkflowModel::cleanupSnapshot()
    {
        if (m_activeRequest.levelSnapshotPath.empty()) return true;

        std::error_code error;
        const bool removed = std::filesystem::remove(m_activeRequest.levelSnapshotPath, error);
        if (error ||
            (!removed && std::filesystem::exists(m_activeRequest.levelSnapshotPath, error)))
        {
            m_lastError = PlayTestError::SnapshotCleanupFailed;
            return false;
        }
        return true;
    }

    std::vector<std::string> PlayTestWorkflowModel::expandArguments(
        const std::filesystem::path& snapshotPath) const
    {
        std::vector<std::string> expanded = m_arguments;
        const std::string replacement = snapshotPath.string();
        for (std::string& argument : expanded)
        {
            const std::size_t position = argument.find(LevelPlaceholder);
            if (position != std::string::npos)
                argument.replace(position, std::char_traits<char>::length(LevelPlaceholder),
                                 replacement);
        }
        return expanded;
    }

    const char* playTestErrorMessage(PlayTestError error) noexcept
    {
        switch (error)
        {
        case PlayTestError::None:
            return "no error";
        case PlayTestError::InvalidExecutable:
            return "invalid play/test executable";
        case PlayTestError::InvalidWorkingDirectory:
            return "invalid play/test working directory";
        case PlayTestError::TooManyArguments:
            return "too many play/test arguments";
        case PlayTestError::ArgumentDataTooLarge:
            return "play/test argument data is invalid or too large";
        case PlayTestError::MissingLevelPlaceholder:
            return "play/test arguments must contain exactly one {level} placeholder";
        case PlayTestError::SessionAlreadyActive:
            return "a play/test session is already active";
        case PlayTestError::SnapshotDirectoryUnavailable:
            return "play/test snapshot directory is unavailable";
        case PlayTestError::SnapshotWriteFailed:
            return "failed to serialize play/test level snapshot";
        case PlayTestError::LaunchRejected:
            return "play/test launcher rejected the request";
        case PlayTestError::StopRejected:
            return "play/test process could not be stopped";
        case PlayTestError::SnapshotCleanupFailed:
            return "play/test snapshot cleanup failed";
        }
        return "unknown play/test error";
    }
}

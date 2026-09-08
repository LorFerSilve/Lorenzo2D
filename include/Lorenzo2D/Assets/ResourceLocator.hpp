#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace l2d
{
    // Resolves portable, relative resource names against an ordered set of
    // runtime search roots. Existing absolute paths are accepted directly.
    class ResourceLocator
    {
      public:
        using Path = std::filesystem::path;

        // Adds a normalized absolute search root. Duplicate roots are ignored.
        // The root does not have to exist yet, which supports generated assets.
        bool addRoot(Path root);
        bool removeRoot(const Path& root);
        void clear();

        const std::vector<Path>& roots() const;

        // Returns the first existing match without changing the locator. Relative
        // resources are confined to configured roots and cannot escape with "..".
        std::optional<Path> locate(const Path& resource) const;
        bool contains(const Path& resource) const;

        // Returns the normalized parent directory of argv[0], or the current
        // working directory when argv[0] cannot be resolved.
        static Path executableDirectory(const Path& executablePath);

      private:
        static std::optional<Path> normalizeAbsolute(const Path& path);

      private:
        std::vector<Path> m_roots;
    };
}

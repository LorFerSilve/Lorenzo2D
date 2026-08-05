#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <algorithm>
#include <system_error>

namespace l2d
{
    bool ResourceLocator::addRoot(Path root)
    {
        const std::optional<Path> normalized = normalizeAbsolute(root);

        if (!normalized) return false;

        if (std::find(m_roots.begin(), m_roots.end(), *normalized) == m_roots.end())
        {
            m_roots.push_back(*normalized);
        }

        return true;
    }

    bool ResourceLocator::removeRoot(const Path& root)
    {
        const std::optional<Path> normalized = normalizeAbsolute(root);

        if (!normalized) return false;

        const auto iterator = std::find(m_roots.begin(), m_roots.end(), *normalized);

        if (iterator == m_roots.end()) return false;

        m_roots.erase(iterator);
        return true;
    }

    void ResourceLocator::clear()
    {
        m_roots.clear();
    }

    const std::vector<ResourceLocator::Path>& ResourceLocator::roots() const
    {
        return m_roots;
    }

    std::optional<ResourceLocator::Path> ResourceLocator::locate(const Path& resource) const
    {
        if (resource.empty()) return std::nullopt;

        std::error_code error;

        if (resource.is_absolute())
        {
            const std::optional<Path> normalized = normalizeAbsolute(resource);

            if (!normalized || !std::filesystem::exists(*normalized, error) || error)
            {
                return std::nullopt;
            }

            return normalized;
        }

        for (const Path& root : m_roots)
        {
            const Path candidate = (root / resource).lexically_normal();
            error.clear();

            if (std::filesystem::exists(candidate, error) && !error)
            {
                return candidate;
            }
        }

        return std::nullopt;
    }

    bool ResourceLocator::contains(const Path& resource) const
    {
        return locate(resource).has_value();
    }

    ResourceLocator::Path ResourceLocator::executableDirectory(const Path& executablePath)
    {
        if (!executablePath.empty())
        {
            const std::optional<Path> normalized = normalizeAbsolute(executablePath);

            if (normalized && normalized->has_parent_path()) return normalized->parent_path();
        }

        std::error_code error;
        const Path current = std::filesystem::current_path(error);
        return error ? Path{} : current.lexically_normal();
    }

    std::optional<ResourceLocator::Path> ResourceLocator::normalizeAbsolute(const Path& path)
    {
        if (path.empty()) return std::nullopt;

        std::error_code error;
        Path absolutePath = path;

        if (!absolutePath.is_absolute())
        {
            absolutePath = std::filesystem::absolute(absolutePath, error);

            if (error) return std::nullopt;
        }

        return absolutePath.lexically_normal();
    }
}

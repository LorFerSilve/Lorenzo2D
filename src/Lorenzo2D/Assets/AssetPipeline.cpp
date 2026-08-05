#include <Lorenzo2D/Assets/AssetPipeline.hpp>

#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <queue>
#include <system_error>
#include <utility>

namespace l2d
{
    namespace
    {
        std::vector<std::string> sorted(std::unordered_set<std::string> values)
        {
            std::vector<std::string> result(values.begin(), values.end());
            std::sort(result.begin(), result.end());
            return result;
        }
    }

    bool AssetDependencyGraph::addDependency(const std::string& asset,
                                             const std::string& dependency)
    {
        if (asset.empty() || dependency.empty() || asset == dependency ||
            reaches(dependency, asset))
        {
            return false;
        }

        const bool inserted = m_dependencies[asset].insert(dependency).second;

        if (inserted)
        {
            m_dependents[dependency].insert(asset);
        }

        return inserted;
    }

    bool AssetDependencyGraph::removeDependency(const std::string& asset,
                                                const std::string& dependency)
    {
        const auto dependencies = m_dependencies.find(asset);

        if (dependencies == m_dependencies.end() || dependencies->second.erase(dependency) == 0u)
        {
            return false;
        }

        if (dependencies->second.empty()) m_dependencies.erase(dependencies);

        const auto dependents = m_dependents.find(dependency);

        if (dependents != m_dependents.end())
        {
            dependents->second.erase(asset);
            if (dependents->second.empty()) m_dependents.erase(dependents);
        }

        return true;
    }

    void AssetDependencyGraph::removeAsset(const std::string& asset)
    {
        const auto dependencies = dependenciesOf(asset);
        const auto dependents = dependentsOf(asset, false);

        for (const std::string& dependency : dependencies)
        {
            removeDependency(asset, dependency);
        }

        for (const std::string& dependent : dependents)
        {
            removeDependency(dependent, asset);
        }
    }

    void AssetDependencyGraph::clear()
    {
        m_dependencies.clear();
        m_dependents.clear();
    }

    std::vector<std::string> AssetDependencyGraph::dependenciesOf(const std::string& asset) const
    {
        const auto iterator = m_dependencies.find(asset);
        return iterator == m_dependencies.end() ? std::vector<std::string>{}
                                                : sorted(iterator->second);
    }

    std::vector<std::string> AssetDependencyGraph::dependentsOf(const std::string& asset,
                                                                bool transitive) const
    {
        std::unordered_set<std::string> discovered;
        std::queue<std::string> pending;
        pending.push(asset);

        while (!pending.empty())
        {
            const std::string current = std::move(pending.front());
            pending.pop();
            const auto iterator = m_dependents.find(current);

            if (iterator == m_dependents.end()) continue;

            for (const std::string& dependent : iterator->second)
            {
                if (!discovered.insert(dependent).second) continue;
                if (transitive) pending.push(dependent);
            }

            if (!transitive) break;
        }

        return sorted(std::move(discovered));
    }

    bool AssetDependencyGraph::reaches(const std::string& start, const std::string& target) const
    {
        if (start == target) return true;

        std::unordered_set<std::string> visited;
        std::queue<std::string> pending;
        pending.push(start);

        while (!pending.empty())
        {
            const std::string current = std::move(pending.front());
            pending.pop();

            if (!visited.insert(current).second) continue;

            const auto iterator = m_dependencies.find(current);
            if (iterator == m_dependencies.end()) continue;

            for (const std::string& dependency : iterator->second)
            {
                if (dependency == target) return true;
                pending.push(dependency);
            }
        }

        return false;
    }

    AssetPipeline::AssetPipeline(AssetManager& assets) : m_assets(&assets) {}

    AssetPipeline::~AssetPipeline() = default;

    bool AssetPipeline::requestTexture(const std::string& name,
                                       const std::filesystem::path& filepath, bool smooth)
    {
        if (name.empty() || filepath.empty()) return false;

        const auto duplicate =
            std::find_if(m_pendingTextures.begin(), m_pendingTextures.end(),
                         [&name](const PendingTexture& pending) { return pending.name == name; });

        if (duplicate != m_pendingTextures.end()) return false;

        PendingTexture pending;
        pending.name = name;
        pending.filepath = filepath;
        pending.smooth = smooth;

        try
        {
            pending.image = std::async(std::launch::async,
                                       [filepath]()
                                       {
                                           auto image = std::make_shared<sf::Image>();
                                           return image->loadFromFile(filepath)
                                                      ? image
                                                      : std::shared_ptr<sf::Image>{};
                                       });
        }
        catch (...)
        {
            return false;
        }

        m_pendingTextures.push_back(std::move(pending));
        return true;
    }

    bool AssetPipeline::watchTexture(const std::string& name, const std::filesystem::path& filepath,
                                     bool smooth)
    {
        if (name.empty() || filepath.empty()) return false;

        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(filepath, error);

        if (error) return false;

        m_watchedTextures.insert_or_assign(name, WatchedTexture{filepath, writeTime, smooth});
        return true;
    }

    bool AssetPipeline::unwatchTexture(const std::string& name)
    {
        return m_watchedTextures.erase(name) > 0u;
    }

    void AssetPipeline::scanForChanges()
    {
        for (auto& entry : m_watchedTextures)
        {
            std::error_code error;
            const auto writeTime = std::filesystem::last_write_time(entry.second.filepath, error);

            if (error || writeTime == entry.second.writeTime) continue;

            if (requestTexture(entry.first, entry.second.filepath, entry.second.smooth))
            {
                entry.second.writeTime = writeTime;
            }
        }
    }

    std::size_t AssetPipeline::poll()
    {
        std::size_t completed = 0;

        for (auto iterator = m_pendingTextures.begin(); iterator != m_pendingTextures.end();)
        {
            if (iterator->image.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++iterator;
                continue;
            }

            std::shared_ptr<sf::Image> image;

            try
            {
                image = iterator->image.get();
            }
            catch (...)
            {
                image.reset();
            }

            bool succeeded = false;

            if (image)
            {
                std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();

                if (texture->loadFromImage(*image))
                {
                    texture->setSmooth(iterator->smooth);
                    succeeded =
                        m_assets->storeTexture(iterator->name, TextureHandle(std::move(texture)));
                }
            }

            m_events.push_back({iterator->name, succeeded,
                                succeeded ? m_dependencies.dependentsOf(iterator->name)
                                          : std::vector<std::string>{}});
            iterator = m_pendingTextures.erase(iterator);
            ++completed;
        }

        return completed;
    }

    std::size_t AssetPipeline::pendingCount() const
    {
        return m_pendingTextures.size();
    }

    const std::vector<AssetReloadEvent>& AssetPipeline::events() const
    {
        return m_events;
    }

    void AssetPipeline::clearEvents()
    {
        m_events.clear();
    }

    AssetDependencyGraph& AssetPipeline::dependencies()
    {
        return m_dependencies;
    }

    const AssetDependencyGraph& AssetPipeline::dependencies() const
    {
        return m_dependencies;
    }
}

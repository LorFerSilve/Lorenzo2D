#pragma once

#include <cstddef>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sf
{
    class Image;
}

namespace l2d
{
    class AssetManager;

    class AssetDependencyGraph
    {
      public:
        bool addDependency(const std::string& asset, const std::string& dependency);
        bool removeDependency(const std::string& asset, const std::string& dependency);
        void removeAsset(const std::string& asset);
        void clear();

        std::vector<std::string> dependenciesOf(const std::string& asset) const;
        std::vector<std::string> dependentsOf(const std::string& asset,
                                              bool transitive = true) const;

      private:
        bool reaches(const std::string& start, const std::string& target) const;

        std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencies;
        std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;
    };

    struct AssetReloadEvent
    {
        std::string asset;
        bool succeeded = false;
        std::vector<std::string> invalidatedDependents;
    };

    // File watching is explicit and polling-based. Image decoding happens on
    // worker threads; GPU texture publication happens in poll() on the caller's
    // thread, keeping SFML context ownership predictable.
    // AssetPipeline borrows AssetManager; the manager must outlive the
    // pipeline. Request decoding may execute on worker threads, but pipeline
    // state and poll()/watch/scan operations are caller-thread APIs and are
    // not safe for concurrent mutation.
    class AssetPipeline
    {
      public:
        explicit AssetPipeline(AssetManager& assets);
        ~AssetPipeline();

        AssetPipeline(const AssetPipeline&) = delete;
        AssetPipeline& operator=(const AssetPipeline&) = delete;

        bool requestTexture(const std::string& name, const std::filesystem::path& filepath,
                            bool smooth = true);
        bool watchTexture(const std::string& name, const std::filesystem::path& filepath,
                          bool smooth = true);
        bool unwatchTexture(const std::string& name);
        void scanForChanges();

        std::size_t poll();
        std::size_t pendingCount() const;
        const std::vector<AssetReloadEvent>& events() const;
        void clearEvents();

        AssetDependencyGraph& dependencies();
        const AssetDependencyGraph& dependencies() const;

      private:
        struct WatchedTexture
        {
            std::filesystem::path filepath;
            std::filesystem::file_time_type writeTime;
            bool smooth = true;
        };

        struct PendingTexture
        {
            std::string name;
            std::filesystem::path filepath;
            bool smooth = true;
            std::future<std::shared_ptr<sf::Image>> image;
        };

        AssetManager* m_assets = nullptr;
        std::unordered_map<std::string, WatchedTexture> m_watchedTextures;
        std::vector<PendingTexture> m_pendingTextures;
        std::vector<AssetReloadEvent> m_events;
        AssetDependencyGraph m_dependencies;
    };
}

#pragma once

#include <Lorenzo2D/Assets/AssetManifest.hpp>
#include <Lorenzo2D/Assets/ResourceLocator.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace l2d
{
    struct AssetManifestResource
    {
        AssetId id;
        AssetSourceKind kind = AssetSourceKind::Unknown;
        std::filesystem::path path;
        std::uint64_t cookKey = 0;
    };

    // Runtime-facing adapter from stable AssetId values to cooked manifest paths.
    //
    // The locator owns a manifest snapshot and delegates filesystem containment,
    // ordered-root precedence, and symlink escape rejection to ResourceLocator.
    // It does not own loaded resources and has no dependency on cooker/editor state.
    class AssetManifestResourceLocator
    {
      public:
        using Path = std::filesystem::path;
        static constexpr std::size_t MaxRoots = 64u;

        AssetManifestResourceLocator() = default;
        explicit AssetManifestResourceLocator(AssetManifest manifest);

        void setManifest(AssetManifest manifest);
        const AssetManifest& manifest() const noexcept;

        bool addRoot(Path root, std::string* error = nullptr);
        bool removeRoot(const Path& root);
        void clearRoots();
        const std::vector<Path>& roots() const noexcept;

        bool resolve(const AssetId& id, AssetSourceKind expectedKind, AssetManifestResource& output,
                     std::string* error = nullptr) const;
        bool resolve(const AssetId& id, AssetManifestResource& output,
                     std::string* error = nullptr) const;

        std::optional<Path> locate(const AssetId& id,
                                   AssetSourceKind expectedKind = AssetSourceKind::Unknown) const;
        bool contains(const AssetId& id,
                      AssetSourceKind expectedKind = AssetSourceKind::Unknown) const;

      private:
        static void clearError(std::string* error);
        static bool fail(std::string* error, const std::string& message);

        AssetManifest m_manifest;
        ResourceLocator m_resources;
    };
}

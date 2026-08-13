#pragma once

#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cstdint>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace l2d
{
    class AssetManager;
    class ComponentCodecRegistry;
    class Scene;

    struct LevelDocument
    {
        std::string name = "Level";
        std::vector<Prefab> objects;
    };

    // Reads and writes Lorenzo2D's deterministic, versioned .l2dlevel format.
    // Loading is transactional: malformed input never changes the destination.
    class LevelSerializer
    {
      public:
        static constexpr std::uint32_t MinimumSupportedVersion = 1u;
        static constexpr std::uint32_t CurrentVersion = 7u;
        static constexpr std::size_t MaximumObjectCount = 100000u;

        static bool save(std::ostream& output, const LevelDocument& level);
        static bool load(std::istream& input, LevelDocument& level);

        static bool saveJson(std::ostream& output, const LevelDocument& level);
        static bool loadJson(std::istream& input, LevelDocument& level);

        static bool saveToFile(const std::string& filepath, const LevelDocument& level);
        static bool loadFromFile(const std::string& filepath, LevelDocument& level);

        static std::vector<GameObjectHandle> instantiate(Scene& scene, const LevelDocument& level);
        static std::vector<GameObjectHandle> instantiate(
            Scene& scene, const LevelDocument& level, AssetManager& assets,
            const ComponentCodecRegistry* codecs = nullptr);
    };
}

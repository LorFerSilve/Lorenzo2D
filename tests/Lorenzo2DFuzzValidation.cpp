#include <Lorenzo2D/Assets/ResourceLocator.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsQueries2D.hpp>
#include <Lorenzo2D/Save/SaveGame.hpp>
#include <Lorenzo2D/Scene/LevelSerializer.hpp>
#include <Lorenzo2D/Scene/Prefab.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/TiledJsonImporter.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint64_t DefaultSeed = 0x4c324446555a5a31ull;
    constexpr std::size_t DefaultCases = 96u;
    constexpr std::size_t MaximumCases = 4096u;
    constexpr std::size_t MaximumMutationBytes = 64u * 1024u;

    class DeterministicRandom final
    {
      public:
        explicit DeterministicRandom(std::uint64_t seed) : m_state(seed) {}

        std::uint64_t next()
        {
            m_state += 0x9e3779b97f4a7c15ull;
            std::uint64_t value = m_state;
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
            return value ^ (value >> 31u);
        }

        std::size_t index(std::size_t upperExclusive)
        {
            if (upperExclusive == 0u) return 0u;
            return static_cast<std::size_t>(next() % upperExclusive);
        }

        bool bit()
        {
            return (next() & 1u) != 0u;
        }

        int coordinate()
        {
            constexpr std::uint64_t Span = 2000001u;
            return static_cast<int>(next() % Span) - 1000000;
        }

      private:
        std::uint64_t m_state;
    };

    std::uint64_t scenarioSeed(std::uint64_t seed, std::string_view scenario)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (const char character : scenario)
        {
            hash ^= static_cast<std::uint8_t>(character);
            hash *= 1099511628211ull;
        }
        return seed ^ hash;
    }

    [[noreturn]] void failCase(std::string_view scenario, std::uint64_t seed, std::size_t caseIndex,
                               std::string_view message)
    {
        std::ostringstream output;
        output << scenario << " failed at case " << caseIndex << " with seed " << seed << ": "
               << message;
        throw std::runtime_error(output.str());
    }

    void requireCase(bool condition, std::string_view scenario, std::uint64_t seed,
                     std::size_t caseIndex, std::string_view message)
    {
        if (!condition) failCase(scenario, seed, caseIndex, message);
    }

    std::string mutateText(std::string input, DeterministicRandom& random)
    {
        static constexpr std::array<std::string_view, 10u> tokens = {
            "{", "}", "[", "]", "null", "true", "\"\"", ":", ",", "999999999999999999999999"};

        switch (random.index(8u))
        {
        case 0u:
            input.resize(random.index(input.size() + 1u));
            break;
        case 1u:
            if (!input.empty())
            {
                const std::size_t begin = random.index(input.size());
                const std::size_t maximum = std::min<std::size_t>(32u, input.size() - begin);
                input.erase(begin, 1u + random.index(maximum));
            }
            break;
        case 2u:
            if (!input.empty())
            {
                static constexpr std::array<char, 8u> replacements = {
                    '\0', '{', '}', '[', ']', '"', '\\', static_cast<char>(0x7f)};
                input[random.index(input.size())] = replacements[random.index(replacements.size())];
            }
            break;
        case 3u:
        {
            const std::string_view token = tokens[random.index(tokens.size())];
            input.insert(random.index(input.size() + 1u), token.data(), token.size());
            break;
        }
        case 4u:
            if (!input.empty())
            {
                const std::size_t begin = random.index(input.size());
                const std::size_t count =
                    1u + random.index(std::min<std::size_t>(32u, input.size() - begin));
                const std::string duplicate = input.substr(begin, count);
                input.insert(random.index(input.size() + 1u), duplicate);
            }
            break;
        case 5u:
        {
            const std::string_view token = tokens[random.index(tokens.size())];
            input.append(token.data(), token.size());
            break;
        }
        case 6u:
        {
            const std::size_t depth = 1u + random.index(32u);
            input.insert(0u, depth, '[');
            input.append(depth, ']');
            break;
        }
        case 7u:
            if (!input.empty())
            {
                const std::size_t index = random.index(input.size());
                input[index] =
                    static_cast<char>(static_cast<unsigned char>(input[index]) ^
                                      static_cast<unsigned char>(1u << random.index(7u)));
            }
            break;
        default:
            break;
        }

        if (input.size() > MaximumMutationBytes) input.resize(MaximumMutationBytes);
        return input;
    }

    bool parseUnsigned(std::string_view text, std::uint64_t& value)
    {
        int base = 10;
        if (text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        {
            text.remove_prefix(2u);
            base = 16;
        }
        if (text.empty()) return false;

        const char* first = text.data();
        const char* last = text.data() + text.size();
        const auto result = std::from_chars(first, last, value, base);
        return result.ec == std::errc{} && result.ptr == last;
    }

    bool sameSentinelLevel(const l2d::LevelDocument& level)
    {
        return level.name == "sentinel-level" && level.objects.size() == 1u &&
               level.objects[0].name == "sentinel-object";
    }

    l2d::LevelDocument sentinelLevel()
    {
        l2d::LevelDocument level;
        level.name = "sentinel-level";
        l2d::Prefab object;
        object.name = "sentinel-object";
        level.objects.push_back(std::move(object));
        return level;
    }

    bool sameSentinelTileMap(const l2d::TileMapData& data)
    {
        const auto iterator = data.properties().find("sentinel");
        return data.width() == 1u && data.height() == 1u && iterator != data.properties().end() &&
               std::get_if<std::int64_t>(&iterator->second) != nullptr &&
               *std::get_if<std::int64_t>(&iterator->second) == 7;
    }

    l2d::TileMapData sentinelTileMap()
    {
        l2d::TileMapData data;
        if (!data.setDimensions(1u, 1u) || !data.setTileSize({8.f, 8.f}))
            throw std::runtime_error("Unable to build Tiled sentinel");
        data.properties()["sentinel"] = std::int64_t{7};
        return data;
    }

    bool sameNavigationConfig(const l2d::NavigationGridConfig2D& left,
                              const l2d::NavigationGridConfig2D& right)
    {
        return left.size == right.size && left.cellSize == right.cellSize &&
               left.origin == right.origin && left.connectivity == right.connectivity &&
               left.allowDiagonalCornerCutting == right.allowDiagonalCornerCutting;
    }

    bool sameNavigationCell(const std::optional<l2d::NavigationCell2D>& left,
                            const std::optional<l2d::NavigationCell2D>& right)
    {
        if (left.has_value() != right.has_value()) return false;
        return !left ||
               (left->walkable == right->walkable && left->traversalCost == right->traversalCost);
    }

    bool pathWithin(const std::filesystem::path& candidate, const std::filesystem::path& root)
    {
        auto candidateIterator = candidate.begin();
        for (auto rootIterator = root.begin(); rootIterator != root.end();
             ++rootIterator, ++candidateIterator)
        {
            if (candidateIterator == candidate.end() || *candidateIterator != *rootIterator)
                return false;
        }
        return true;
    }

    class TemporaryResourceTree final
    {
      public:
        explicit TemporaryResourceTree(std::uint64_t seed)
        {
            m_base = std::filesystem::temp_directory_path() /
                     ("lorenzo2d_phase11_fuzz_" + std::to_string(seed));
            std::error_code error;
            std::filesystem::remove_all(m_base, error);
            std::filesystem::create_directories(m_base / "root" / "assets");
            m_root = (m_base / "root").lexically_normal();
            m_outside = (m_base / "escape.txt").lexically_normal();

            {
                std::ofstream file(m_root / "assets" / "ok.txt");
                file << "inside";
            }
            {
                std::ofstream file(m_outside);
                file << "outside";
            }

            error.clear();
            std::filesystem::create_directory_symlink(m_base, m_root / "escape-link", error);
            m_symlinkCreated = !error;
        }

        ~TemporaryResourceTree()
        {
            std::error_code error;
            std::filesystem::remove_all(m_base, error);
        }

        const std::filesystem::path& root() const
        {
            return m_root;
        }

        const std::filesystem::path& outside() const
        {
            return m_outside;
        }

        bool symlinkCreated() const
        {
            return m_symlinkCreated;
        }

      private:
        std::filesystem::path m_base;
        std::filesystem::path m_root;
        std::filesystem::path m_outside;
        bool m_symlinkCreated = false;
    };

    void runSaveJson(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "save-json";
        DeterministicRandom random(scenarioSeed(seed, Scenario));

        l2d::SaveDocument source("phase11.fuzz", 3u);
        if (!source.setBool("door.open", true) || !source.setInteger("coins", 42) ||
            !source.setNumber("health", 87.5) || !source.setString("checkpoint", "harbor"))
            throw std::runtime_error("Unable to build save fuzz seed");

        std::ostringstream serialized;
        if (!l2d::SaveGameSerializer::save(serialized, source))
            throw std::runtime_error("Unable to serialize save fuzz seed");
        const std::string seedDocument = serialized.str();

        {
            l2d::SaveDocument destination("sentinel", 9u);
            destination.setString("kept", "yes");
            const l2d::SaveDocument before = destination;
            l2d::SaveGameLimits limits;
            limits.maxTotalBytes = seedDocument.size() - 1u;
            std::istringstream input(seedDocument);
            requireCase(!l2d::SaveGameSerializer::load(input, destination, limits), Scenario, seed,
                        0u, "oversized valid input was not rejected");
            requireCase(destination == before, Scenario, seed, 0u,
                        "oversized rejection changed destination");
        }

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            const std::string mutated = mutateText(seedDocument, random);
            l2d::SaveDocument destination("sentinel", 9u);
            destination.setString("kept", "yes");
            const l2d::SaveDocument before = destination;

            l2d::SaveGameLimits limits;
            limits.maxTotalBytes = MaximumMutationBytes;
            if (caseIndex % 19u == 0u && mutated.size() > 1u)
                limits.maxTotalBytes = mutated.size() - 1u;

            std::istringstream input(mutated);
            const bool loaded = l2d::SaveGameSerializer::load(input, destination, limits);
            if (!loaded)
            {
                requireCase(destination == before, Scenario, seed, caseIndex,
                            "failed load was not transactional");
                continue;
            }

            std::ostringstream normalized;
            requireCase(l2d::SaveGameSerializer::save(normalized, destination), Scenario, seed,
                        caseIndex, "accepted document could not be serialized");

            l2d::SaveDocument roundTrip;
            std::istringstream normalizedInput(normalized.str());
            requireCase(l2d::SaveGameSerializer::load(normalizedInput, roundTrip), Scenario, seed,
                        caseIndex, "accepted document could not be reloaded");
            requireCase(roundTrip == destination, Scenario, seed, caseIndex,
                        "accepted document was not stable after round trip");
        }
    }

    void runLevelJson(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "level-json";
        DeterministicRandom random(scenarioSeed(seed, Scenario));

        l2d::LevelDocument source;
        source.name = "phase11-fuzz";
        l2d::Prefab object;
        object.name = "player";
        object.tag = "fuzz";
        source.objects.push_back(std::move(object));

        std::ostringstream serialized;
        if (!l2d::LevelSerializer::saveJson(serialized, source))
            throw std::runtime_error("Unable to serialize level fuzz seed");
        const std::string seedDocument = serialized.str();

        {
            l2d::LevelDocument destination = sentinelLevel();
            l2d::LevelLoadLimits limits;
            limits.maxInputBytes = seedDocument.size() - 1u;
            std::istringstream input(seedDocument);
            requireCase(!l2d::LevelSerializer::loadJson(input, destination, limits), Scenario, seed,
                        0u, "oversized valid level was not rejected");
            requireCase(sameSentinelLevel(destination), Scenario, seed, 0u,
                        "oversized level rejection changed destination");
        }

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            const std::string mutated = mutateText(seedDocument, random);
            l2d::LevelLoadLimits limits;
            limits.maxInputBytes = MaximumMutationBytes;
            if (caseIndex % 17u == 0u && mutated.size() > 1u)
                limits.maxInputBytes = mutated.size() - 1u;

            l2d::LevelDocument jsonDestination = sentinelLevel();
            std::istringstream jsonInput(mutated);
            const bool jsonLoaded =
                l2d::LevelSerializer::loadJson(jsonInput, jsonDestination, limits);
            if (!jsonLoaded)
            {
                requireCase(sameSentinelLevel(jsonDestination), Scenario, seed, caseIndex,
                            "failed JSON level load was not transactional");
            }
            else
            {
                for (const l2d::Prefab& prefab : jsonDestination.objects)
                    requireCase(l2d::isValidPrefab(prefab), Scenario, seed, caseIndex,
                                "accepted JSON level contains an invalid prefab");

                std::ostringstream normalized;
                requireCase(l2d::LevelSerializer::saveJson(normalized, jsonDestination), Scenario,
                            seed, caseIndex, "accepted JSON level could not be serialized");
                l2d::LevelDocument roundTrip;
                std::istringstream normalizedInput(normalized.str());
                requireCase(l2d::LevelSerializer::loadJson(normalizedInput, roundTrip), Scenario,
                            seed, caseIndex, "accepted JSON level could not be reloaded");
                requireCase(roundTrip.name == jsonDestination.name &&
                                roundTrip.objects.size() == jsonDestination.objects.size(),
                            Scenario, seed, caseIndex,
                            "accepted JSON level changed structural identity after round trip");
            }

            l2d::LevelDocument autoDestination = sentinelLevel();
            std::istringstream autoInput(mutated);
            const bool autoLoaded = l2d::LevelSerializer::load(autoInput, autoDestination, limits);
            if (!autoLoaded)
                requireCase(sameSentinelLevel(autoDestination), Scenario, seed, caseIndex,
                            "failed auto-detected level load was not transactional");
        }
    }

    void runTiledJson(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "tiled-json";
        DeterministicRandom random(scenarioSeed(seed, Scenario));
        const std::string seedDocument =
            R"({"width":2,"height":2,"tilewidth":16,"tileheight":16,"orientation":"orthogonal","infinite":false,"tilesets":[{"firstgid":1,"tilecount":1,"columns":1,"tilewidth":16,"tileheight":16,"image":"tiles.png"}],"layers":[{"type":"tilelayer","name":"ground","width":2,"height":2,"data":[1,0,0,1]}]})";

        {
            l2d::TileMapData destination = sentinelTileMap();
            l2d::TiledJsonImportLimits limits;
            limits.maxInputBytes = seedDocument.size() - 1u;
            std::istringstream input(seedDocument);
            requireCase(!l2d::TiledJsonImporter::load(input, destination, limits), Scenario, seed,
                        0u, "oversized valid Tiled input was not rejected");
            requireCase(sameSentinelTileMap(destination), Scenario, seed, 0u,
                        "oversized Tiled rejection changed destination");
        }

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            const std::string mutated = mutateText(seedDocument, random);
            l2d::TiledJsonImportLimits limits;
            limits.maxInputBytes = MaximumMutationBytes;
            if (caseIndex % 17u == 0u && mutated.size() > 1u)
                limits.maxInputBytes = mutated.size() - 1u;

            l2d::TileMapData destination = sentinelTileMap();
            std::istringstream input(mutated);
            const bool loaded = l2d::TiledJsonImporter::load(input, destination, limits);
            if (!loaded)
            {
                requireCase(sameSentinelTileMap(destination), Scenario, seed, caseIndex,
                            "failed Tiled import was not transactional");
                continue;
            }

            requireCase(destination.isValid(), Scenario, seed, caseIndex,
                        "accepted Tiled document produced invalid tile-map data");
            requireCase(destination.cellCount() <= l2d::TileMapData::MaximumCellCount, Scenario,
                        seed, caseIndex, "accepted Tiled document exceeded cell bound");
            requireCase(destination.layers().size() <= l2d::TileMapData::MaximumLayerCount,
                        Scenario, seed, caseIndex, "accepted Tiled document exceeded layer bound");
            requireCase(destination.objects().size() <= l2d::TileMapData::MaximumObjectCount,
                        Scenario, seed, caseIndex, "accepted Tiled document exceeded object bound");
        }
    }

    void runPhysicsQueries(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "physics-queries";
        DeterministicRandom random(scenarioSeed(seed, Scenario));

        l2d::Scene scene;
        l2d::GameObject& object = scene.createGameObject("fuzz-box");
        object.transform.setPosition({0.f, 0.f});
        object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{20.f, 20.f});
        const l2d::PhysicsQueryContext2D queries(scene);

        requireCase(queries.raycast({-50.f, 0.f}, {50.f, 0.f}).has_value(), Scenario, seed, 0u,
                    "valid control raycast did not hit");

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const std::array<float, 3u> nonFinite = {nan, infinity, -infinity};
        const std::array<float, 3u> invalidRadii = {nan, infinity, -1.f};
        const std::array<float, 4u> invalidDimensions = {nan, infinity, 0.f, -1.f};

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            const float badCoordinate = nonFinite[random.index(nonFinite.size())];
            const float badRadius = invalidRadii[random.index(invalidRadii.size())];
            const float badDimension = invalidDimensions[random.index(invalidDimensions.size())];
            const sf::Vector2f badPoint =
                random.bit() ? sf::Vector2f{badCoordinate, 0.f} : sf::Vector2f{0.f, badCoordinate};

            requireCase(queries.raycastAll(badPoint, {0.f, 0.f}).empty(), Scenario, seed, caseIndex,
                        "invalid raycast coordinate did not fail closed");
            requireCase(queries.pointQuery(badPoint).empty(), Scenario, seed, caseIndex,
                        "invalid point query did not fail closed");
            requireCase(queries.overlapCircle({0.f, 0.f}, badRadius).empty(), Scenario, seed,
                        caseIndex, "invalid circle overlap did not fail closed");
            requireCase(!queries.castCircle({-30.f, 0.f}, {30.f, 0.f}, badRadius).has_value(),
                        Scenario, seed, caseIndex, "invalid circle cast did not fail closed");
            requireCase(queries.castCircleAll({-30.f, 0.f}, {30.f, 0.f}, badRadius).empty(),
                        Scenario, seed, caseIndex, "invalid circle cast-all did not fail closed");
            requireCase(queries.overlapBox({0.f, 0.f}, {badDimension, 10.f}).empty(), Scenario,
                        seed, caseIndex, "invalid box overlap did not fail closed");
            requireCase(
                !queries.castBox({-30.f, 0.f}, {30.f, 0.f}, {10.f, badDimension}).has_value(),
                Scenario, seed, caseIndex, "invalid box cast did not fail closed");
            requireCase(queries.overlapCapsule({0.f, 0.f}, 5.f, 9.f).empty(), Scenario, seed,
                        caseIndex, "invalid capsule geometry did not fail closed");
            requireCase(!queries.castCapsule({-30.f, 0.f}, {30.f, 0.f}, 5.f, 9.f).has_value(),
                        Scenario, seed, caseIndex, "invalid capsule cast did not fail closed");
            requireCase(queries.overlapBox({0.f, 0.f}, {10.f, 10.f}, badCoordinate).empty(),
                        Scenario, seed, caseIndex, "invalid query rotation did not fail closed");
        }
    }

    void runNavigationGrid(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "navigation-grid";
        DeterministicRandom random(scenarioSeed(seed, Scenario));

        l2d::NavigationGridConfig2D config;
        config.size = {4u, 4u};
        config.cellSize = {16.f, 16.f};
        config.origin = {-8.f, 12.f};
        l2d::NavigationGrid2D grid(config);
        requireCase(grid.cellCount() == 16u, Scenario, seed, 0u,
                    "valid control grid was not initialized");

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const std::array<float, 4u> invalidCosts = {nan, infinity, 0.f, -1.f};

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            l2d::NavigationGridConfig2D invalid = config;
            switch (random.index(8u))
            {
            case 0u:
                invalid.size.x = 0u;
                break;
            case 1u:
                invalid.size.y = 0u;
                break;
            case 2u:
                invalid.cellSize.x = 0.f;
                break;
            case 3u:
                invalid.cellSize.y = -1.f;
                break;
            case 4u:
                invalid.cellSize.x = nan;
                break;
            case 5u:
                invalid.origin.y = infinity;
                break;
            case 6u:
                invalid.connectivity = static_cast<l2d::NavigationConnectivity2D>(255);
                break;
            case 7u:
                invalid.size = {std::numeric_limits<unsigned int>::max(),
                                std::numeric_limits<unsigned int>::max()};
                break;
            default:
                break;
            }

            const l2d::NavigationGridConfig2D beforeConfig = grid.config();
            const std::size_t beforeCount = grid.cellCount();
            const std::uint64_t beforeRevision = grid.revision();
            const auto beforeCell = grid.cell({0, 0});

            requireCase(!l2d::NavigationGrid2D::isValidConfig(invalid), Scenario, seed, caseIndex,
                        "generated navigation config was unexpectedly valid");
            requireCase(!grid.reset(invalid), Scenario, seed, caseIndex,
                        "invalid navigation reset unexpectedly succeeded");
            requireCase(sameNavigationConfig(grid.config(), beforeConfig) &&
                            grid.cellCount() == beforeCount && grid.revision() == beforeRevision &&
                            sameNavigationCell(grid.cell({0, 0}), beforeCell),
                        Scenario, seed, caseIndex,
                        "failed navigation reset changed existing grid state");

            const float invalidCost = invalidCosts[random.index(invalidCosts.size())];
            const std::uint64_t cellRevision = grid.revision();
            requireCase(!grid.setTraversalCost({0, 0}, invalidCost), Scenario, seed, caseIndex,
                        "invalid traversal cost unexpectedly succeeded");
            requireCase(grid.revision() == cellRevision, Scenario, seed, caseIndex,
                        "invalid traversal cost changed grid revision");

            const float badWorld = random.bit() ? nan : infinity;
            requireCase(!grid.worldToCell({badWorld, 0.f}).has_value(), Scenario, seed, caseIndex,
                        "non-finite world coordinate mapped to a cell");
            requireCase(
                !grid.contains({std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}),
                Scenario, seed, caseIndex, "extreme cell coordinate was accepted");
        }
    }

    void runUiInput(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "ui-input";
        DeterministicRandom random(scenarioSeed(seed, Scenario));

        l2d::UiCanvas2D canvas;
        l2d::UiButton2D button;
        button.id = "fuzz-button";
        button.label = "Fuzz";
        button.position = {10.f, 10.f};
        button.size = {100.f, 40.f};
        requireCase(canvas.addButton(button), Scenario, seed, 0u,
                    "valid control button could not be added");

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const std::array<float, 3u> nonFinite = {nan, infinity, -infinity};
        const std::array<float, 4u> invalidDimensions = {nan, infinity, 0.f, -1.f};

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            sf::Vector2f position{10.f, 10.f};
            sf::Vector2f size{100.f, 40.f};
            if (random.bit())
                position.x = nonFinite[random.index(nonFinite.size())];
            else
                size.x = invalidDimensions[random.index(invalidDimensions.size())];

            requireCase(!canvas.setButtonBounds("fuzz-button", position, size), Scenario, seed,
                        caseIndex, "invalid button bounds unexpectedly succeeded");
            requireCase(canvas.buttonCount() == 1u, Scenario, seed, caseIndex,
                        "invalid bounds changed button count");

            l2d::UiButton2D invalidButton = button;
            invalidButton.id = "invalid-" + std::to_string(caseIndex);
            invalidButton.style.characterSize =
                random.bit() ? 0u : invalidButton.style.characterSize;
            if (invalidButton.style.characterSize != 0u) invalidButton.style.textOffset.x = nan;
            requireCase(!l2d::UiCanvas2D::isValidButton(invalidButton), Scenario, seed, caseIndex,
                        "generated invalid button was considered valid");
            requireCase(!canvas.addButton(std::move(invalidButton)), Scenario, seed, caseIndex,
                        "invalid button was added");

            canvas.cancelInteraction();
            for (std::size_t step = 0u; step < 8u; ++step)
            {
                l2d::PointerState pointer;
                pointer.screenPosition = {random.coordinate(), random.coordinate()};
                pointer.down = random.bit();
                pointer.pressed = random.bit();
                pointer.released = random.bit();
                canvas.update(pointer);

                const auto hovered = canvas.hoveredButton();
                const auto pressed = canvas.pressedButton();
                requireCase(!hovered || *hovered == "fuzz-button", Scenario, seed, caseIndex,
                            "pointer sequence produced an unknown hovered button");
                requireCase(!pressed || *pressed == "fuzz-button", Scenario, seed, caseIndex,
                            "pointer sequence produced an unknown pressed button");
                requireCase(canvas.buttonCount() == 1u, Scenario, seed, caseIndex,
                            "pointer sequence changed button count");
            }

            canvas.cancelInteraction();
            l2d::PointerState control;
            control.screenPosition = {20, 20};
            canvas.update(control);
            requireCase(canvas.hoveredButton() == std::optional<std::string>("fuzz-button"),
                        Scenario, seed, caseIndex,
                        "failed bounds mutation corrupted original button bounds");
        }
    }

    void runResourceLookup(std::uint64_t seed, std::size_t cases)
    {
        constexpr std::string_view Scenario = "resource-lookup";
        DeterministicRandom random(scenarioSeed(seed, Scenario));
        TemporaryResourceTree tree(scenarioSeed(seed, Scenario));

        l2d::ResourceLocator locator;
        requireCase(locator.addRoot(tree.root()), Scenario, seed, 0u,
                    "resource root could not be added");
        requireCase(locator.locate("assets/ok.txt").has_value(), Scenario, seed, 0u,
                    "valid control resource did not resolve");
        requireCase(!locator.locate("../escape.txt").has_value(), Scenario, seed, 0u,
                    "relative resource escaped configured root");
        requireCase(locator.locate(tree.outside()).has_value(), Scenario, seed, 0u,
                    "documented absolute-path lookup stopped working");
        if (tree.symlinkCreated())
            requireCase(!locator.locate("escape-link/escape.txt").has_value(), Scenario, seed, 0u,
                        "symlink escaped configured resource root");

        const std::array<std::string, 7u> fixed = {"",
                                                   ".",
                                                   "..",
                                                   "../escape.txt",
                                                   "assets/../assets/ok.txt",
                                                   "assets//ok.txt",
                                                   "assets/../../escape.txt"};

        for (std::size_t caseIndex = 0u; caseIndex < cases; ++caseIndex)
        {
            std::string resource;
            if (caseIndex < fixed.size())
                resource = fixed[caseIndex];
            else
                resource = mutateText("assets/ok.txt", random);

            const std::size_t rootsBefore = locator.roots().size();
            const auto located = locator.locate(std::filesystem::path(resource));
            requireCase(locator.roots().size() == rootsBefore, Scenario, seed, caseIndex,
                        "resource lookup mutated configured roots");

            if (located && !std::filesystem::path(resource).is_absolute())
                requireCase(pathWithin(located->lexically_normal(), tree.root()), Scenario, seed,
                            caseIndex, "relative resource resolved outside configured root");
        }
    }

    using ScenarioFunction = void (*)(std::uint64_t, std::size_t);

    struct Scenario
    {
        std::string_view name;
        ScenarioFunction run;
    };

    constexpr std::array<Scenario, 7u> Scenarios = {{
        {"save-json", runSaveJson},
        {"level-json", runLevelJson},
        {"tiled-json", runTiledJson},
        {"physics-queries", runPhysicsQueries},
        {"navigation-grid", runNavigationGrid},
        {"ui-input", runUiInput},
        {"resource-lookup", runResourceLookup},
    }};

    bool knownScenario(std::string_view name)
    {
        return std::any_of(Scenarios.begin(), Scenarios.end(),
                           [&](const Scenario& scenario) { return scenario.name == name; });
    }

    void printUsage()
    {
        std::cout << "Usage: Lorenzo2DFuzzValidation [--seed N] [--cases N] "
                     "[--scenario NAME] [--help]\n";
        std::cout << "Scenarios:";
        for (const Scenario& scenario : Scenarios)
            std::cout << ' ' << scenario.name;
        std::cout << '\n';
    }
}

int main(int argc, char** argv)
{
    std::uint64_t seed = DefaultSeed;
    std::size_t cases = DefaultCases;
    std::optional<std::string> selectedScenario;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--help")
        {
            printUsage();
            return 0;
        }
        if (argument == "--seed" || argument == "--cases" || argument == "--scenario")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "Missing value for " << argument << ".\n";
                return 2;
            }

            const std::string_view value(argv[++index]);
            if (argument == "--scenario")
            {
                if (!knownScenario(value))
                {
                    std::cerr << "Unknown scenario: " << value << ".\n";
                    return 2;
                }
                selectedScenario = std::string(value);
                continue;
            }

            std::uint64_t parsed = 0u;
            if (!parseUnsigned(value, parsed))
            {
                std::cerr << "Invalid numeric value for " << argument << ": " << value << ".\n";
                return 2;
            }

            if (argument == "--seed")
                seed = parsed;
            else
            {
                if (parsed == 0u || parsed > MaximumCases)
                {
                    std::cerr << "--cases must be between 1 and " << MaximumCases << ".\n";
                    return 2;
                }
                cases = static_cast<std::size_t>(parsed);
            }
            continue;
        }

        std::cerr << "Unknown argument: " << argument << ".\n";
        return 2;
    }

    std::size_t executedScenarios = 0u;
    std::size_t failures = 0u;
    for (const Scenario& scenario : Scenarios)
    {
        if (selectedScenario && *selectedScenario != scenario.name) continue;

        ++executedScenarios;
        try
        {
            scenario.run(seed, cases);
            std::cout << "[PASS] " << scenario.name << " seed=" << seed << " cases=" << cases
                      << '\n';
        }
        catch (const std::exception& exception)
        {
            ++failures;
            std::cerr << "[FAIL] " << exception.what() << '\n';
        }
        catch (...)
        {
            ++failures;
            std::cerr << "[FAIL] " << scenario.name << " raised a non-standard exception with seed "
                      << seed << ".\n";
        }
    }

    if (failures != 0u)
    {
        std::cerr << failures << " fuzz/property scenario(s) failed. Reproduce with --seed "
                  << seed;
        if (selectedScenario) std::cerr << " --scenario " << *selectedScenario;
        std::cerr << " --cases " << cases << ".\n";
        return 1;
    }

    std::cout << "Phase 11 fuzz/property validation passed: scenarios=" << executedScenarios
              << " cases_per_scenario=" << cases << " seed=" << seed << ".\n";
    return 0;
}

#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <SFML/Graphics/View.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr std::uint32_t REPORT_SCHEMA_VERSION = 1u;

    struct BenchmarkResult
    {
        double milliseconds = 0.0;
        std::size_t checksum = 0;
    };

    struct BenchmarkEntry
    {
        std::string name;
        std::size_t iterations = 0;
        BenchmarkResult result;
    };

    struct OutputOptions
    {
        std::optional<std::string> jsonPath;
        std::optional<std::string> csvPath;
        bool helpRequested = false;
    };

    class CountingComponent final : public l2d::Component
    {
      public:
        void onUpdate(float deltaTime) override
        {
            if (deltaTime > 0.f) ++m_updateCount;
        }

        std::size_t updateCount() const
        {
            return m_updateCount;
        }

      private:
        std::size_t m_updateCount = 0;
    };

    template <typename Callable>
    BenchmarkResult measure(std::size_t iterations, Callable&& callable)
    {
        std::size_t checksum = 0;
        const Clock::time_point start = Clock::now();

        for (std::size_t iteration = 0; iteration < iterations; ++iteration)
            checksum += callable();

        const Clock::time_point finish = Clock::now();
        const std::chrono::duration<double, std::milli> elapsed = finish - start;
        return {elapsed.count(), checksum};
    }

    BenchmarkResult benchmarkSceneUpdate(std::size_t iterations)
    {
        constexpr std::size_t objectCount = 4096u;
        l2d::Scene scene;

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            l2d::GameObject& object = scene.createGameObject("UpdatedObject");
            object.addComponent<CountingComponent>();
        }

        return measure(iterations,
                       [&scene]()
                       {
                           scene.fixedUpdate(1.f / 60.f);
                           const l2d::GameObject* first = scene.gameObjects().front().get();
                           const l2d::GameObject* last = scene.gameObjects().back().get();
                           const CountingComponent* firstCounter =
                               first != nullptr ? first->getComponent<CountingComponent>()
                                                : nullptr;
                           const CountingComponent* lastCounter =
                               last != nullptr ? last->getComponent<CountingComponent>() : nullptr;

                           return (firstCounter != nullptr ? firstCounter->updateCount() : 0u) +
                                  (lastCounter != nullptr ? lastCounter->updateCount() : 0u);
                       });
    }

    BenchmarkResult benchmarkInputActionSampling(std::size_t iterations)
    {
        constexpr std::size_t evaluationsPerIteration = 4096u;
        l2d::InputSnapshot snapshot;
        l2d::InputMap inputMap(snapshot);
        const l2d::InputCode left = l2d::InputCode::keyboard(sf::Keyboard::Scancode::A);
        const l2d::InputCode right = l2d::InputCode::keyboard(sf::Keyboard::Scancode::D);
        const l2d::InputCode up = l2d::InputCode::keyboard(sf::Keyboard::Scancode::W);
        const l2d::InputCode down = l2d::InputCode::keyboard(sf::Keyboard::Scancode::S);

        inputMap.bindAxis2D("move", left, right, up, down);
        inputMap.bindButton("interact", l2d::InputCode::keyboard(sf::Keyboard::Scancode::Space));
        snapshot.beginFrame();
        snapshot.setButton(right, true);
        snapshot.setButton(up, true);

        return measure(iterations,
                       [&inputMap]()
                       {
                           std::size_t checksum = 0u;
                           for (std::size_t evaluation = 0u; evaluation < evaluationsPerIteration;
                                ++evaluation)
                           {
                               const sf::Vector2f movement = inputMap.axis2D("move");
                               checksum += movement.x > 0.f ? 1u : 0u;
                               checksum += movement.y < 0.f ? 1u : 0u;
                               checksum += inputMap.down("interact") ? 1u : 0u;
                           }
                           return checksum;
                       });
    }

    BenchmarkResult benchmarkRenderQueueBuild(std::size_t iterations)
    {
        constexpr std::size_t objectCount = 4096u;
        l2d::Scene scene;

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            l2d::GameObject& object = scene.createGameObject("RenderedObject");
            const std::size_t bucket = (index * 37u) % 127u;
            object.setZOrder(static_cast<std::int32_t>(bucket) - 63);
        }

        l2d::RenderQueue2D queue;

        return measure(iterations,
                       [&scene, &queue]()
                       {
                           queue.build(scene);

                           if (queue.empty()) return std::size_t{0};

                           return queue.size() + queue.entries().front().insertionOrder +
                                  queue.entries().back().insertionOrder;
                       });
    }

    void populatePhysicsScene(l2d::Scene& scene)
    {
        constexpr std::size_t width = 24u;
        constexpr std::size_t height = 24u;
        constexpr float spacing = 48.f;

        for (std::size_t y = 0; y < height; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                l2d::GameObject& object = scene.createGameObject("Body");
                object.transform.setPosition(
                    {static_cast<float>(x) * spacing, static_cast<float>(y) * spacing});
                object.addComponent<l2d::CircleCollider2D>(8.f);
                l2d::RigidBody2D& body = object.addComponent<l2d::RigidBody2D>();
                body.setBodyType(l2d::BodyType2D::Kinematic);
            }
        }
    }

    BenchmarkResult benchmarkPhysics(l2d::PhysicsBroadPhaseMode2D broadPhaseMode,
                                     std::size_t iterations)
    {
        l2d::Scene scene;
        populatePhysicsScene(scene);

        l2d::PhysicsWorld2DConfig config;
        config.gravity = {0.f, 0.f};
        config.broadPhaseMode = broadPhaseMode;
        config.broadPhaseCellSize = 32.f;
        l2d::PhysicsWorld2D world(config);

        return measure(iterations,
                       [&scene, &world]()
                       {
                           world.step(scene, 1.f / 60.f);
                           const l2d::PhysicsBroadPhaseStats2D& stats = world.broadPhaseStats();
                           return stats.proxyCount + stats.candidatePairCount +
                                  stats.narrowPhaseTestCount;
                       });
    }

    l2d::TileMap::Layout makeTileLayout()
    {
        constexpr std::size_t width = 256u;
        constexpr std::size_t height = 192u;
        l2d::TileMap::Layout layout(height, std::string(width, '.'));

        for (std::size_t y = 0; y < height; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                if (y + 1u == height || (x % 19u == 0u && y % 5u != 0u)) layout[y][x] = '#';
            }
        }

        return layout;
    }

    BenchmarkResult benchmarkTileMapBuild(std::size_t iterations)
    {
        const l2d::TileMap::Layout layout = makeTileLayout();

        return measure(iterations,
                       [&layout]()
                       {
                           l2d::Scene scene;
                           l2d::TileMap tileMap;
                           tileMap.loadFromLayout(scene, layout);
                           const l2d::TileMapBuildStats& stats = tileMap.buildStats();
                           return stats.solidTileCount + stats.renderChunkCount +
                                  stats.collisionRectangleCount;
                       });
    }

    BenchmarkResult benchmarkTileMapCulling(std::size_t iterations)
    {
        l2d::Scene scene;
        l2d::TileMap tileMap;
        tileMap.loadFromLayout(scene, makeTileLayout());
        const sf::View view({4096.f, 3072.f}, {1280.f, 720.f});

        return measure(iterations,
                       [&tileMap, &view]()
                       {
                           const l2d::TileMapRenderStats stats = tileMap.renderStatsForView(view);
                           return stats.visibleChunkCount + stats.culledChunkCount +
                                  stats.submittedVertexCount;
                       });
    }

    std::vector<BenchmarkEntry> runBenchmarks()
    {
        constexpr std::size_t sceneIterations = 120u;
        constexpr std::size_t inputIterations = 120u;
        constexpr std::size_t renderQueueIterations = 120u;
        constexpr std::size_t physicsIterations = 30u;
        constexpr std::size_t tileMapBuildIterations = 5u;
        constexpr std::size_t tileMapCullingIterations = 1000u;

        std::vector<BenchmarkEntry> entries;
        entries.push_back({"input action sampling", inputIterations,
                           benchmarkInputActionSampling(inputIterations)});
        entries.push_back(
            {"scene fixed update", sceneIterations, benchmarkSceneUpdate(sceneIterations)});
        entries.push_back({"render queue build", renderQueueIterations,
                           benchmarkRenderQueueBuild(renderQueueIterations)});
        entries.push_back(
            {"physics uniform grid", physicsIterations,
             benchmarkPhysics(l2d::PhysicsBroadPhaseMode2D::UniformGrid, physicsIterations)});
        entries.push_back(
            {"physics brute force", physicsIterations,
             benchmarkPhysics(l2d::PhysicsBroadPhaseMode2D::BruteForce, physicsIterations)});
        entries.push_back({"tile-map full build", tileMapBuildIterations,
                           benchmarkTileMapBuild(tileMapBuildIterations)});
        entries.push_back({"tile-map view culling", tileMapCullingIterations,
                           benchmarkTileMapCulling(tileMapCullingIterations)});
        return entries;
    }

    void printHumanReadable(const std::vector<BenchmarkEntry>& entries)
    {
        std::cout << "Lorenzo2D " << l2d::VersionString << " diagnostic benchmarks\n";

        for (const BenchmarkEntry& entry : entries)
        {
            std::cout << std::left << std::setw(30) << entry.name << std::right << std::setw(10)
                      << entry.iterations << " iterations  " << std::setw(10) << std::fixed
                      << std::setprecision(3) << entry.result.milliseconds
                      << " ms  checksum=" << entry.result.checksum << '\n';
        }
    }

    void writeJsonString(std::ostream& output, std::string_view value)
    {
        output << '"';

        for (const char character : value)
        {
            switch (character)
            {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << character;
                break;
            }
        }

        output << '"';
    }

    void writeJsonReport(const std::string& path, const std::vector<BenchmarkEntry>& entries)
    {
        std::ofstream output(path);
        if (!output) throw std::runtime_error("unable to open JSON report: " + path);

        output << "{\n  \"schema_version\": " << REPORT_SCHEMA_VERSION
               << ",\n  \"engine_version\": ";
        writeJsonString(output, l2d::VersionString);
        output << ",\n  \"benchmarks\": [\n";

        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const BenchmarkEntry& entry = entries[index];
            output << "    {\"name\": ";
            writeJsonString(output, entry.name);
            output << ", \"iterations\": " << entry.iterations
                   << ", \"milliseconds\": " << std::setprecision(15) << entry.result.milliseconds
                   << ", \"checksum\": " << entry.result.checksum << '}';
            output << (index + 1u == entries.size() ? "\n" : ",\n");
        }

        output << "  ]\n}\n";
        if (!output) throw std::runtime_error("failed to write JSON report: " + path);
    }

    void writeCsvField(std::ostream& output, std::string_view value)
    {
        output << '"';
        for (const char character : value)
        {
            if (character == '"') output << '"';
            output << character;
        }
        output << '"';
    }

    void writeCsvReport(const std::string& path, const std::vector<BenchmarkEntry>& entries)
    {
        std::ofstream output(path);
        if (!output) throw std::runtime_error("unable to open CSV report: " + path);

        output << "schema_version,engine_version,name,iterations,milliseconds,checksum\n";

        for (const BenchmarkEntry& entry : entries)
        {
            output << REPORT_SCHEMA_VERSION << ',';
            writeCsvField(output, l2d::VersionString);
            output << ',';
            writeCsvField(output, entry.name);
            output << ',' << entry.iterations << ',' << std::setprecision(15)
                   << entry.result.milliseconds << ',' << entry.result.checksum << '\n';
        }

        if (!output) throw std::runtime_error("failed to write CSV report: " + path);
    }

    OutputOptions parseOptions(int argumentCount, char** arguments)
    {
        OutputOptions options;

        for (int index = 1; index < argumentCount; ++index)
        {
            const std::string argument = arguments[index];

            if (argument == "--help" || argument == "-h")
            {
                options.helpRequested = true;
                continue;
            }

            if (argument != "--json" && argument != "--csv")
                throw std::invalid_argument("unknown option: " + argument);

            if (index + 1 >= argumentCount)
                throw std::invalid_argument("missing path after " + argument);

            const std::string path = arguments[++index];
            if (path.empty()) throw std::invalid_argument("empty path after " + argument);

            std::optional<std::string>& destination =
                argument == "--json" ? options.jsonPath : options.csvPath;
            if (destination) throw std::invalid_argument("duplicate option: " + argument);
            destination = path;
        }

        return options;
    }

    void printUsage(const char* executable)
    {
        std::cout << "Usage: " << executable << " [--json <path>] [--csv <path>]\n";
    }
}

int main(int argumentCount, char** arguments)
{
    try
    {
        const OutputOptions options = parseOptions(argumentCount, arguments);
        if (options.helpRequested)
        {
            printUsage(arguments[0]);
            return 0;
        }

        const std::vector<BenchmarkEntry> entries = runBenchmarks();
        printHumanReadable(entries);

        if (options.jsonPath) writeJsonReport(*options.jsonPath, entries);
        if (options.csvPath) writeCsvReport(*options.csvPath, entries);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Lorenzo2D benchmark error: " << exception.what() << '\n';
        printUsage(arguments[0]);
        return 2;
    }

    return 0;
}

#include <Lorenzo2D/Animation/AnimationClip.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Audio/AudioSystem.hpp>
#include <Lorenzo2D/Core/FixedStepScheduler.hpp>
#include <Lorenzo2D/Core/Version.hpp>
#include <Lorenzo2D/Diagnostics/DeterministicReplay.hpp>
#include <Lorenzo2D/Diagnostics/Diagnostics.hpp>
#include <Lorenzo2D/Diagnostics/SubsystemDiagnostics.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/Navigation/AStarPathfinder2D.hpp>
#include <Lorenzo2D/Navigation/NavigationGrid2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>
#include <Lorenzo2D/Save/SaveGame.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/SoundChannel.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct StressConfig
    {
        std::string name;
        std::size_t tileWidth = 0u;
        std::size_t tileHeight = 0u;
        std::size_t streamChanges = 0u;
        std::size_t renderables = 0u;
        std::size_t physicsColliders = 0u;
        std::size_t physicsTicks = 0u;
        std::size_t physicsQueriesPerTick = 0u;
        std::size_t navigationAgents = 0u;
        std::size_t navigationReplansPerAgent = 0u;
        std::size_t navigationGridExtent = 0u;
        std::size_t fixedTicks = 0u;
        std::size_t fixedObjects = 0u;
        std::size_t assetCycles = 0u;
        std::size_t uiButtons = 0u;
        std::size_t uiInteractions = 0u;
        std::size_t audioVoices = 0u;
        std::size_t audioBatchSize = 0u;
        std::size_t saveEntries = 0u;
        std::size_t saveStringBytes = 0u;
        std::size_t saveCycles = 0u;
        std::size_t saveFileCycles = 0u;
    };

    struct Options
    {
        std::string profile = "smoke";
        std::optional<std::string> scenario;
        std::optional<std::filesystem::path> jsonPath;
        bool help = false;
    };

    struct ScenarioResult
    {
        std::string name;
        bool passed = false;
        std::uint64_t operations = 0u;
        std::uint64_t checksum = 0u;
        double milliseconds = 0.0;
        std::string failure;
    };

    class StressComponent final : public l2d::Component
    {
      public:
        explicit StressComponent(std::uint64_t seed) : m_state(seed) {}

        void onUpdate(float deltaTime) override
        {
            (void)deltaTime;
            ++m_updates;
            m_state = m_state * 6364136223846793005ull + 1442695040888963407ull;
            m_state ^= m_updates + 0x9e3779b97f4a7c15ull;
        }

        std::uint64_t state() const noexcept
        {
            return m_state;
        }

        std::uint64_t updates() const noexcept
        {
            return m_updates;
        }

      private:
        std::uint64_t m_state = 0u;
        std::uint64_t m_updates = 0u;
    };

    class TemporarySaveFile final
    {
      public:
        explicit TemporarySaveFile(std::filesystem::path path) : m_path(std::move(path))
        {
            cleanup();
        }

        ~TemporarySaveFile()
        {
            cleanup();
        }

        const std::filesystem::path& path() const noexcept
        {
            return m_path;
        }

        void cleanup() const noexcept
        {
            std::error_code error;
            std::filesystem::remove(m_path, error);
            error.clear();
            std::filesystem::remove(m_path.string() + ".tmp", error);
            error.clear();
            std::filesystem::remove(m_path.string() + ".bak", error);
        }

      private:
        std::filesystem::path m_path;
    };

    std::uint64_t toUint64(std::size_t value) noexcept
    {
        constexpr std::uint64_t Maximum = std::numeric_limits<std::uint64_t>::max();

        if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
        {
            if (value > static_cast<std::size_t>(Maximum)) return Maximum;
        }

        return static_cast<std::uint64_t>(value);
    }

    std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right) noexcept
    {
        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        return left >= maximum - right ? maximum : left + right;
    }

    std::uint64_t saturatingMultiply(std::size_t left, std::size_t right) noexcept
    {
        if (left == 0u || right == 0u) return 0u;

        const std::uint64_t lhs = toUint64(left);
        const std::uint64_t rhs = toUint64(right);
        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();

        return lhs > maximum / rhs ? maximum : lhs * rhs;
    }

    void require(bool condition, std::string_view message)
    {
        if (!condition) throw std::runtime_error(std::string(message));
    }

    std::filesystem::path uniqueStressSavePath()
    {
        const auto timestamp =
            std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::random_device::result_type randomValue = std::random_device{}();

        return std::filesystem::temp_directory_path() /
               ("lorenzo2d_phase11_stress_save_" + std::to_string(timestamp) + "_" +
                std::to_string(randomValue) + ".json");
    }

    StressConfig stressConfig(std::string_view name)
    {
        if (name == "smoke")
        {
            return {"smoke", 64u, 64u, 8u,  256u, 96u, 8u, 4u,   32u, 2u, 48u,
                    600u,    8u,  64u, 64u, 256u, 64u, 8u, 128u, 64u, 4u, 2u};
        }

        if (name == "standard")
        {
            return {"standard", 128u, 96u,   64u,  1024u, 512u, 24u, 16u,   256u, 4u,  128u,
                    10000u,     16u,  1000u, 256u, 5000u, 512u, 32u, 4000u, 768u, 32u, 8u};
        }

        if (name == "soak")
        {
            return {"soak",  128u, 96u,    1000u, 2048u,  512u,  240u, 32u,   256u, 16u,  128u,
                    432000u, 32u,  10000u, 512u,  50000u, 4096u, 64u,  4000u, 768u, 128u, 32u};
        }

        throw std::invalid_argument("Unknown stress profile: " + std::string(name));
    }

    Options parseOptions(int argc, char** argv)
    {
        Options options;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);

            if (argument == "--help" || argument == "-h")
            {
                options.help = true;
            }
            else if (argument == "--profile")
            {
                require(index + 1 < argc, "--profile requires smoke, standard, or soak");
                options.profile = argv[++index];
            }
            else if (argument == "--scenario")
            {
                require(index + 1 < argc, "--scenario requires a scenario name");
                options.scenario = std::string(argv[++index]);
            }
            else if (argument == "--json")
            {
                require(index + 1 < argc, "--json requires an output path");
                options.jsonPath = std::filesystem::path(argv[++index]);
            }
            else
            {
                throw std::invalid_argument("Unknown argument: " + std::string(argument));
            }
        }

        return options;
    }

    l2d::SoundBufferHandle makeSoundBuffer()
    {
        std::vector<std::int16_t> samples(256u);
        for (std::size_t index = 0u; index < samples.size(); ++index)
            samples[index] = index % 2u == 0u ? std::int16_t{900} : std::int16_t{-900};

        auto buffer = std::make_shared<sf::SoundBuffer>();
        const std::vector<sf::SoundChannel> channels{sf::SoundChannel::Mono};
        require(buffer->loadFromSamples(samples.data(), samples.size(), 1u, 44100u, channels),
                "Failed to create in-memory stress sound buffer");
        return l2d::SoundBufferHandle(std::move(buffer));
    }

    ScenarioResult runScenario(std::string name,
                               const std::function<std::pair<std::uint64_t, std::uint64_t>()>& body)
    {
        ScenarioResult result;
        result.name = std::move(name);
        const Clock::time_point started = Clock::now();

        try
        {
            const auto [operations, checksum] = body();
            result.operations = operations;
            result.checksum = checksum;
            result.passed = true;
        }
        catch (const std::exception& exception)
        {
            result.failure = exception.what();
        }
        catch (...)
        {
            result.failure = "unknown exception";
        }

        result.milliseconds =
            std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        return result;
    }

    std::pair<std::uint64_t, std::uint64_t> stressTileMap(const StressConfig& config)
    {
        require(config.tileHeight == 0u ||
                    config.tileWidth <= std::numeric_limits<std::size_t>::max() / config.tileHeight,
                "Tile workload overflow");
        const std::size_t tileCount = config.tileWidth * config.tileHeight;

        l2d::Scene scene("stress-tilemap");
        l2d::TileMap tileMap;
        tileMap.setRenderChunkSize({16u, 16u});
        const l2d::TileMap::Layout layout(config.tileHeight, std::string(config.tileWidth, '#'));
        tileMap.loadFromLayout(scene, layout);

        const l2d::TileMapBuildStats& build = tileMap.buildStats();
        require(build.solidTileCount == tileCount, "Tilemap solid-tile counter drift");
        require(build.renderedTileCount == tileCount, "Tilemap rendered-tile counter drift");
        require(build.renderChunkCount > 0u, "Tilemap produced no render chunks");

        const float tileSizeX = tileMap.loadedTileSize().x;
        const float tileSizeY = tileMap.loadedTileSize().y;
        const sf::Vector2f worldSize = tileMap.worldSize();
        const sf::View fullView({worldSize.x * 0.5f, worldSize.y * 0.5f}, worldSize);
        const l2d::TileMapRenderStats fullStats = tileMap.renderStatsForView(fullView);
        require(fullStats.submittedTileCount == tileCount,
                "Full-map stress view did not submit every rendered tile");

        const std::size_t regionColumns = std::min<std::size_t>(32u, config.tileWidth);
        const std::size_t regionRows = std::min<std::size_t>(24u, config.tileHeight);
        require(regionColumns > 0u && regionRows > 0u, "Tilemap stream region is empty");

        sf::View view({0.f, 0.f}, {static_cast<float>(regionColumns) * tileSizeX,
                                   static_cast<float>(regionRows) * tileSizeY});

        std::uint64_t checksum = toUint64(fullStats.submittedTileCount);
        l2d::DiagnosticCounters counters;

        for (std::size_t change = 0u; change < config.streamChanges; ++change)
        {
            const std::size_t columnRange = config.tileWidth - regionColumns + 1u;
            const std::size_t rowRange = config.tileHeight - regionRows + 1u;
            const std::size_t firstColumn = (change * 7u) % columnRange;
            const std::size_t firstRow = (change * 11u) % rowRange;

            require(tileMap.setStreamRegion({firstColumn, firstRow, regionColumns, regionRows}),
                    "Tilemap rejected a bounded stream region");

            view.setCenter(
                {(static_cast<float>(firstColumn) + static_cast<float>(regionColumns) * 0.5f) *
                     tileSizeX,
                 (static_cast<float>(firstRow) + static_cast<float>(regionRows) * 0.5f) *
                     tileSizeY});

            const l2d::TileMapRenderStats stats = tileMap.renderStatsForView(view);
            require(stats.chunkCount == build.renderChunkCount,
                    "Tilemap chunk count changed while streaming");
            require(stats.residentChunkCount + stats.nonResidentChunkCount == stats.chunkCount,
                    "Tilemap resident chunk accounting drift");
            require(stats.visibleChunkCount + stats.culledChunkCount == stats.chunkCount,
                    "Tilemap culling accounting drift");
            require(stats.submittedTileCount <= tileCount,
                    "Tilemap submitted more tiles than exist");

            counters.reset();
            l2d::accumulateTileMapRenderDiagnostics(stats, counters);
            require(counters.value(l2d::DiagnosticCounter::DrawCalls) == stats.drawCallCount,
                    "Tilemap draw-call diagnostic drift");
            require(counters.value(l2d::DiagnosticCounter::RenderedItems) ==
                        stats.submittedTileCount,
                    "Tilemap rendered-item diagnostic drift");

            checksum = saturatingAdd(checksum, toUint64(stats.visibleChunkCount));
            checksum = saturatingAdd(checksum, toUint64(stats.submittedTileCount));
            checksum = saturatingAdd(checksum, toUint64(stats.drawCallCount));
        }

        tileMap.clearStreamRegion();
        const l2d::TileMapRenderStats unstreamed = tileMap.renderStatsForView(view);
        require(unstreamed.residentChunkCount == unstreamed.chunkCount,
                "Clearing the stream region did not restore all resident chunks");

        return {saturatingAdd(toUint64(tileCount), toUint64(config.streamChanges)), checksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressRenderables(const StressConfig& config)
    {
        l2d::Scene scene("stress-renderables");
        std::vector<l2d::GameObjectHandle> destroyedHandles;
        destroyedHandles.reserve(config.renderables / 17u + 1u);

        for (std::size_t index = 0u; index < config.renderables; ++index)
        {
            l2d::GameObject& object = scene.createGameObject("stress-renderable");
            object.setZOrder(static_cast<std::int32_t>(index % 257u) - 128);
            l2d::RenderOrder2D& order = object.addComponent<l2d::RenderOrder2D>();
            order.setLayer(static_cast<std::int32_t>(index % 9u));
            require(order.setExplicitDepth(static_cast<float>(index % 101u)),
                    "Render-order depth rejected a finite value");

            if (index % 17u == 0u) destroyedHandles.push_back(scene.createHandle(object));
        }

        l2d::RenderQueue2D queue;
        std::uint64_t checksum = 0u;
        const std::size_t rebuilds = std::max<std::size_t>(4u, config.streamChanges / 2u);

        for (std::size_t rebuild = 0u; rebuild < rebuilds; ++rebuild)
        {
            queue.build(scene);
            require(queue.size() == config.renderables, "Render queue lost live objects");
            require(!queue.empty(), "Render queue unexpectedly empty");
            checksum = saturatingAdd(checksum, toUint64(queue.entries().front().insertionOrder));
            checksum = saturatingAdd(checksum, toUint64(queue.entries().back().insertionOrder));
        }

        for (const l2d::GameObjectHandle& handle : destroyedHandles)
        {
            l2d::GameObject* object = handle.get();
            require(object != nullptr, "Fresh renderable handle unexpectedly invalid");
            scene.destroyGameObject(*object);
        }
        scene.destroyQueuedGameObjects();

        for (const l2d::GameObjectHandle& handle : destroyedHandles)
            require(handle.get() == nullptr, "Destroyed renderable handle remained valid");

        const std::size_t expectedActive = config.renderables - destroyedHandles.size();
        queue.build(scene);
        require(queue.size() == expectedActive, "Render queue retained destroyed objects");

        l2d::DiagnosticCounters counters;
        l2d::accumulateSceneDiagnostics(scene, counters);
        l2d::accumulateRenderQueueDiagnostics(queue, counters);
        require(counters.value(l2d::DiagnosticCounter::ActiveEntities) == toUint64(expectedActive),
                "Active-entity diagnostics drift under renderable churn");
        require(counters.value(l2d::DiagnosticCounter::ActiveComponents) ==
                    toUint64(expectedActive),
                "Active-component diagnostics drift under renderable churn");
        require(counters.value(l2d::DiagnosticCounter::RenderedItems) == toUint64(expectedActive),
                "Render-queue diagnostics drift under renderable churn");

        checksum = saturatingAdd(checksum, toUint64(expectedActive));
        const std::uint64_t operations = saturatingAdd(
            saturatingMultiply(config.renderables, rebuilds), toUint64(destroyedHandles.size()));
        return {operations, checksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressPhysics(const StressConfig& config)
    {
        l2d::Scene scene("stress-physics");
        std::vector<l2d::GameObject*> objects;
        std::vector<l2d::RigidBody2D*> bodies;
        std::vector<l2d::ColliderId> colliderIds;
        std::vector<sf::Vector2f> basePositions;
        objects.reserve(config.physicsColliders);
        bodies.reserve(config.physicsColliders);
        colliderIds.reserve(config.physicsColliders);
        basePositions.reserve(config.physicsColliders);

        std::size_t columns = 1u;
        while (columns < config.physicsColliders / columns)
            ++columns;
        const float spacing = 13.f;

        for (std::size_t index = 0u; index < config.physicsColliders; ++index)
        {
            const std::size_t column = index % columns;
            const std::size_t row = index / columns;
            const sf::Vector2f position{static_cast<float>(column) * spacing,
                                        static_cast<float>(row) * spacing};

            l2d::GameObject& object = scene.createGameObject("stress-collider");
            object.transform.setPosition(position);
            l2d::BoxCollider2D& collider =
                object.addComponent<l2d::BoxCollider2D>(sf::Vector2f{10.f, 10.f});
            l2d::RigidBody2D& body = object.addComponent<l2d::RigidBody2D>();
            body.setBodyType(l2d::BodyType2D::Dynamic);
            body.setUseGravity(false);
            body.setFixedRotation(true);
            objects.push_back(&object);
            bodies.push_back(&body);
            colliderIds.push_back(collider.id());
            basePositions.push_back(position);
        }

        l2d::PhysicsWorld2DConfig worldConfig;
        worldConfig.gravity = {0.f, 0.f};
        worldConfig.broadPhaseCellSize = 32.f;
        worldConfig.sleeping = false;
        l2d::PhysicsWorld2D world(worldConfig);

        l2d::DiagnosticCounters counters;
        std::uint64_t checksum = 0u;

        for (std::size_t tick = 0u; tick < config.physicsTicks; ++tick)
        {
            for (std::size_t index = 0u; index < objects.size(); ++index)
            {
                sf::Vector2f position = basePositions[index];
                if (tick % 2u == 1u && index % 8u == 1u && index % columns != 0u) position.x -= 4.f;

                objects[index]->transform.setPosition(position);
                bodies[index]->setVelocity({0.f, 0.f});
                bodies[index]->setAngularVelocity(0.f);
                bodies[index]->clearForces();
            }

            world.step(scene, 1.f / 60.f);
            require(world.broadPhaseStats().proxyCount == config.physicsColliders,
                    "Physics proxy count drift");

            if (tick % 2u == 0u)
                require(world.contacts().empty(),
                        "Physics contact churn expected a separated phase");
            else
                require(!world.contacts().empty(),
                        "Physics contact churn expected an overlapping phase");

            const l2d::PhysicsQueryContext2D queries = world.createQueryContext(scene);
            require(queries.proxyCount() == config.physicsColliders,
                    "Physics query snapshot proxy drift");

            for (std::size_t query = 0u; query < config.physicsQueriesPerTick; ++query)
            {
                const std::size_t row = query % std::max<std::size_t>(1u, objects.size() / columns);
                const float y = static_cast<float>(row) * spacing;
                const auto hit =
                    queries.raycast({-16.f, y}, {static_cast<float>(columns) * spacing + 16.f, y});
                require(hit.has_value(), "Physics stress raycast unexpectedly missed");
                require(hit->colliderId == colliderIds[row * columns],
                        "Physics stress raycast hit an unexpected collider");
                checksum = saturatingAdd(checksum, hit->colliderId);
                l2d::recordPhysicsQueries(counters);
            }

            checksum = saturatingAdd(checksum, toUint64(world.contacts().size()));
            checksum =
                saturatingAdd(checksum, toUint64(world.broadPhaseStats().candidatePairCount));
        }

        l2d::accumulatePhysicsDiagnostics(world, counters);
        require(counters.value(l2d::DiagnosticCounter::Colliders) ==
                    toUint64(config.physicsColliders),
                "Physics collider diagnostic drift");
        require(counters.value(l2d::DiagnosticCounter::PhysicsQueries) ==
                    saturatingMultiply(config.physicsTicks, config.physicsQueriesPerTick),
                "Physics query diagnostic drift");

        const std::uint64_t operations =
            saturatingAdd(saturatingMultiply(config.physicsColliders, config.physicsTicks),
                          saturatingMultiply(config.physicsTicks, config.physicsQueriesPerTick));
        return {operations, checksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressNavigation(const StressConfig& config)
    {
        l2d::NavigationGridConfig2D gridConfig;
        gridConfig.size = {static_cast<unsigned int>(config.navigationGridExtent),
                           static_cast<unsigned int>(config.navigationGridExtent)};
        gridConfig.cellSize = {16.f, 16.f};
        gridConfig.connectivity = l2d::NavigationConnectivity2D::FourWay;
        l2d::NavigationGrid2D grid(gridConfig);
        const l2d::AStarPathfinder2D pathfinder;

        l2d::DiagnosticCounters counters;
        std::uint64_t checksum = 0u;

        for (std::size_t replan = 0u; replan < config.navigationReplansPerAgent; ++replan)
        {
            const int center = static_cast<int>(config.navigationGridExtent / 2u);
            const float traversalCost = replan % 2u == 0u ? 1.0f : 1.25f;
            require(grid.setTraversalCost({center, center}, traversalCost),
                    "Navigation grid rejected bounded traversal cost");

            for (std::size_t agent = 0u; agent < config.navigationAgents; ++agent)
            {
                const int extent = static_cast<int>(config.navigationGridExtent);
                const int startRow = static_cast<int>(agent % config.navigationGridExtent);
                const int goalRow =
                    static_cast<int>((agent * 7u + replan * 3u) % config.navigationGridExtent);

                const l2d::NavigationPath2D path =
                    pathfinder.findPath(grid, {0, startRow}, {extent - 1, goalRow});
                require(path.succeeded(), "Navigation stress path failed");
                require(path.gridRevision == grid.revision(),
                        "Navigation path reported a stale grid revision");
                l2d::recordNavigationPathDiagnostics(path, counters, replan != 0u);

                checksum = saturatingAdd(checksum, toUint64(path.visitedNodes));
                checksum = saturatingAdd(checksum, toUint64(path.cells.size()));
            }
        }

        const std::uint64_t expectedReplans = saturatingMultiply(
            config.navigationAgents,
            config.navigationReplansPerAgent > 0u ? config.navigationReplansPerAgent - 1u : 0u);
        require(counters.value(l2d::DiagnosticCounter::NavigationExpansions) > 0u,
                "Navigation expansion diagnostics remained zero");
        require(counters.value(l2d::DiagnosticCounter::NavigationReplans) == expectedReplans,
                "Navigation replan diagnostic drift");

        return {saturatingMultiply(config.navigationAgents, config.navigationReplansPerAgent),
                checksum};
    }

    l2d::ReplayTrace runFixedStepReplay(const StressConfig& config, std::uint64_t& finalChecksum)
    {
        l2d::Scene scene("stress-fixed-step");
        std::vector<StressComponent*> components;
        components.reserve(config.fixedObjects);

        for (std::size_t index = 0u; index < config.fixedObjects; ++index)
        {
            l2d::GameObject& object = scene.createGameObject("stress-fixed-object");
            components.push_back(
                &object.addComponent<StressComponent>(0x123456789abcdef0ull + toUint64(index)));
        }

        l2d::ReplayTraceConfig replayConfig;
        replayConfig.maxTicks = config.fixedTicks;
        replayConfig.maxInputBytesPerTick = 0u;
        replayConfig.maxTotalInputBytes = 0u;
        l2d::ReplayTrace trace(replayConfig);

        l2d::FixedStepConfig schedulerConfig;
        schedulerConfig.maximumTicksPerFrame = 2u;
        l2d::FixedStepScheduler scheduler(schedulerConfig);

        finalChecksum = 0u;
        for (std::size_t tick = 0u; tick < config.fixedTicks; ++tick)
        {
            const l2d::FixedStepFrame frame = scheduler.advance(schedulerConfig.fixedDeltaTime);
            require(frame.ticksToRun == 1u, "Fixed-step soak scheduled an unexpected tick count");
            require(frame.droppedTicks == 0u, "Fixed-step soak dropped simulation ticks");

            scene.fixedUpdate(static_cast<float>(schedulerConfig.fixedDeltaTime));

            l2d::DeterministicHasher64 hasher;
            hasher.appendString("phase11.4-fixed-step");
            hasher.appendUInt64(toUint64(tick));
            for (const StressComponent* component : components)
            {
                require(component->updates() == toUint64(tick + 1u),
                        "Fixed-step component update count drift");
                hasher.appendUInt64(component->state());
                hasher.appendUInt64(component->updates());
            }

            finalChecksum = hasher.value();
            require(trace.record(toUint64(tick), {}, finalChecksum),
                    "Replay trace rejected a bounded fixed-step tick");
        }

        require(scheduler.tickCount() == toUint64(config.fixedTicks),
                "Fixed-step scheduler cumulative tick drift");
        require(scheduler.droppedTickCount() == 0u,
                "Fixed-step scheduler cumulative dropped-tick drift");

        l2d::DiagnosticCounters counters;
        l2d::accumulateSceneDiagnostics(scene, counters);
        require(counters.value(l2d::DiagnosticCounter::ActiveEntities) ==
                    toUint64(config.fixedObjects),
                "Fixed-step scene active-entity diagnostic drift");
        require(counters.value(l2d::DiagnosticCounter::ActiveComponents) ==
                    toUint64(config.fixedObjects),
                "Fixed-step scene active-component diagnostic drift");

        return trace;
    }

    std::pair<std::uint64_t, std::uint64_t> stressFixedStepReplay(const StressConfig& config)
    {
        std::uint64_t firstChecksum = 0u;
        std::uint64_t secondChecksum = 0u;
        const l2d::ReplayTrace first = runFixedStepReplay(config, firstChecksum);
        const l2d::ReplayTrace second = runFixedStepReplay(config, secondChecksum);

        const l2d::ReplayComparison comparison = l2d::compareReplayTraces(first, second);
        require(comparison.equivalent(), "Accelerated fixed-step replay diverged");
        require(comparison.matchedTicks == config.fixedTicks,
                "Replay comparison matched-tick count drift");
        require(firstChecksum == secondChecksum, "Final fixed-step state hashes diverged");

        return {saturatingMultiply(saturatingMultiply(config.fixedTicks, config.fixedObjects), 2u),
                firstChecksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressAssets(const StressConfig& config)
    {
        l2d::AssetManager assets;
        const l2d::SoundBufferHandle sound = makeSoundBuffer();
        const l2d::LiveSoundBufferHandle live = assets.liveSoundBuffer("stress.sound");
        require(static_cast<bool>(live), "Live sound binding was not created");

        std::uint64_t expectedGeneration = live.generation();
        std::uint64_t checksum = 0u;

        for (std::size_t cycle = 0u; cycle < config.assetCycles; ++cycle)
        {
            require(assets.storeSoundBuffer("stress.sound", sound),
                    "Sound-buffer replacement failed");
            ++expectedGeneration;
            require(live.generation() == expectedGeneration,
                    "Live sound generation drift after replacement");

            const l2d::SoundBufferHandle snapshot = live.snapshot();
            require(static_cast<bool>(snapshot), "Live sound snapshot was empty after replacement");

            auto clip = std::make_shared<l2d::AnimationClip>("stress.clip");
            require(clip->addFrame({{0, 0}, {16, 16}}, 0.1f),
                    "Stress animation frame creation failed");
            require(
                assets.storeAnimationClip("stress.clip", l2d::AnimationClipHandle(std::move(clip))),
                "Animation replacement failed");
            const l2d::AnimationClipHandle clipSnapshot = assets.getAnimationClip("stress.clip");
            require(static_cast<bool>(clipSnapshot), "Animation snapshot was empty");

            require(assets.loadedAssetCount() == 2u, "Loaded-asset count drift during replacement");
            require(assets.unloadSoundBuffer("stress.sound"), "Sound-buffer unload failed");
            ++expectedGeneration;
            require(live.generation() == expectedGeneration,
                    "Live sound generation drift after unload");
            require(!live.snapshot(), "Live sound binding retained an unloaded registry asset");
            require(static_cast<bool>(snapshot),
                    "Snapshot handle was invalidated by registry unload");

            require(assets.unloadAnimationClip("stress.clip"), "Animation unload failed");
            require(static_cast<bool>(clipSnapshot),
                    "Animation snapshot was invalidated by registry unload");
            require(assets.loadedAssetCount() == 0u, "Loaded assets leaked across cycles");

            checksum = saturatingAdd(checksum, expectedGeneration);
        }

        l2d::DiagnosticCounters counters;
        l2d::accumulateAssetDiagnostics(assets, counters);
        require(counters.value(l2d::DiagnosticCounter::LoadedAssets) == 0u,
                "Loaded-asset diagnostic drift after churn");
        require(counters.value(l2d::DiagnosticCounter::LiveAssets) == 1u,
                "Live-asset-slot diagnostic drift after churn");

        return {saturatingMultiply(config.assetCycles, 4u), checksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressUi(const StressConfig& config)
    {
        l2d::UiCanvas2D canvas;
        constexpr std::size_t Columns = 16u;
        constexpr float ButtonWidth = 32.f;
        constexpr float ButtonHeight = 24.f;

        for (std::size_t index = 0u; index < config.uiButtons; ++index)
        {
            l2d::UiButton2D button;
            button.id = "button-" + std::to_string(index);
            button.label = "Stress";
            button.position = {static_cast<float>(index % Columns) * ButtonWidth,
                               static_cast<float>(index / Columns) * ButtonHeight};
            button.size = {ButtonWidth, ButtonHeight};
            require(canvas.addButton(std::move(button)), "UI stress button insertion failed");
        }

        require(canvas.buttonCount() == config.uiButtons, "UI button count drift after setup");
        std::uint64_t checksum = 0u;

        for (std::size_t interaction = 0u; interaction < config.uiInteractions; ++interaction)
        {
            const std::size_t index = interaction % config.uiButtons;
            const std::string id = "button-" + std::to_string(index);
            const int x = static_cast<int>(index % Columns) * static_cast<int>(ButtonWidth) + 4;
            const int y = static_cast<int>(index / Columns) * static_cast<int>(ButtonHeight) + 4;

            l2d::PointerState press;
            press.screenPosition = {x, y};
            press.down = true;
            press.pressed = true;
            canvas.update(press);
            require(canvas.pressedButton() == std::optional<std::string>(id),
                    "UI press capture drift");

            l2d::PointerState release;
            release.screenPosition = {x, y};
            release.released = true;
            canvas.update(release);
            require(canvas.wasActivated(id), "UI activation drift under churn");
            require(!canvas.pressedButton().has_value(), "UI retained pressed state after release");

            if (interaction % 31u == 0u)
            {
                require(canvas.setButtonLabel(id, "Stress-" + std::to_string(interaction)),
                        "UI label mutation failed");
                require(canvas.setButtonEnabled(id, false), "UI disable mutation failed");
                require(canvas.setButtonEnabled(id, true), "UI enable mutation failed");
                require(canvas.setButtonVisible(id, false), "UI hide mutation failed");
                require(canvas.setButtonVisible(id, true), "UI show mutation failed");
            }

            if (interaction % 257u == 0u) canvas.cancelInteraction();
            checksum = saturatingAdd(checksum, toUint64(index + 1u));
        }

        require(canvas.buttonCount() == config.uiButtons, "UI button count drift after churn");
        return {saturatingMultiply(config.uiInteractions, 2u), checksum};
    }

    std::pair<std::uint64_t, std::uint64_t> stressAudio(const StressConfig& config)
    {
        const l2d::SoundBufferHandle buffer = makeSoundBuffer();
        l2d::AudioSystem audio;
        l2d::AudioPlayOptions2D options;
        options.looping = true;
        options.volume = 25.f;

        std::size_t emitted = 0u;
        std::uint64_t checksum = 0u;

        while (emitted < config.audioVoices)
        {
            const std::size_t batch = std::min(config.audioBatchSize, config.audioVoices - emitted);
            std::vector<l2d::AudioVoiceId> voices;
            voices.reserve(batch);

            for (std::size_t index = 0u; index < batch; ++index)
            {
                const l2d::AudioVoiceId voice = audio.playSound(buffer, options);
                require(voice != l2d::InvalidAudioVoiceId,
                        "Audio stress failed to allocate a voice");
                voices.push_back(voice);
                checksum = saturatingAdd(checksum, voice);
            }

            l2d::DiagnosticCounters counters;
            l2d::accumulateAudioDiagnostics(audio, counters);
            require(counters.value(l2d::DiagnosticCounter::ActiveAudioVoices) == toUint64(batch),
                    "Active-audio-voice diagnostic drift");

            for (const l2d::AudioVoiceId voice : voices)
                require(audio.stopVoice(voice), "Audio stress failed to stop a live voice");

            require(audio.activeVoiceCount() == 0u, "Audio voices leaked after batch stop");
            emitted += batch;
        }

        return {saturatingMultiply(config.audioVoices, 2u), checksum};
    }

    l2d::SaveDocument makeStressSave(const StressConfig& config)
    {
        l2d::SaveDocument document("phase11.stress", 1u);
        require(config.saveStringBytes > 0u, "Stress save string payload must be non-empty");
        std::string payload(config.saveStringBytes, 'x');

        for (std::size_t index = 0u; index < config.saveEntries; ++index)
        {
            payload[index % payload.size()] =
                static_cast<char>('a' + static_cast<int>(index % 26u));
            require(document.setString("entry." + std::to_string(index), payload),
                    "Failed to populate bounded stress save");
        }

        return document;
    }

    std::pair<std::uint64_t, std::uint64_t> stressSave(const StressConfig& config)
    {
        l2d::SaveDocument document = makeStressSave(config);
        l2d::SaveGameLimits limits;

        std::ostringstream baselineOutput;
        require(l2d::SaveGameSerializer::save(baselineOutput, document, limits),
                "Near-limit stress save could not be serialized");
        const std::string baseline = baselineOutput.str();
        require(baseline.size() <= limits.maxTotalBytes,
                "Stress save exceeded configured total-byte limit");

        if (config.name != "smoke")
        {
            require(baseline.size() >= limits.maxTotalBytes * 3u / 5u,
                    "Standard stress save is not representative of the configured size limit");
        }

        l2d::DiagnosticCounters counters;
        std::uint64_t checksum = 0u;

        for (std::size_t cycle = 0u; cycle < config.saveCycles; ++cycle)
        {
            std::ostringstream output;
            require(l2d::SaveGameSerializer::save(output, document, limits),
                    "Repeated save serialization failed");
            const std::string serialized = output.str();
            require(serialized == baseline, "Repeated save serialization became nondeterministic");
            l2d::recordSaveWriteDiagnostics(counters, serialized.size());

            l2d::SaveDocument loaded;
            std::istringstream input(serialized);
            require(l2d::SaveGameSerializer::load(input, loaded, limits),
                    "Repeated save load failed");
            require(loaded == document, "Repeated save round trip changed the document");
            l2d::recordSaveReadDiagnostics(counters, serialized.size());
            checksum = saturatingAdd(checksum, toUint64(serialized.size()));
        }

        const std::uint64_t expectedBytes = saturatingMultiply(baseline.size(), config.saveCycles);
        require(counters.value(l2d::DiagnosticCounter::SaveBytesWritten) == expectedBytes,
                "Save-byte-written diagnostic drift");
        require(counters.value(l2d::DiagnosticCounter::SaveBytesRead) == expectedBytes,
                "Save-byte-read diagnostic drift");

        TemporarySaveFile file(uniqueStressSavePath());
        for (std::size_t cycle = 0u; cycle < config.saveFileCycles; ++cycle)
        {
            require(document.setInteger("stress.cycle", static_cast<std::int64_t>(cycle)),
                    "Failed to mutate stress save generation");
            require(l2d::SaveGameSerializer::saveToFile(file.path(), document, limits),
                    "Transactional save-file replacement failed");

            l2d::SaveDocument loaded;
            require(l2d::SaveGameSerializer::loadFromFile(file.path(), loaded, limits),
                    "Transactional stress save-file load failed");
            require(loaded == document, "Transactional save-file replacement changed data");

            std::error_code error;
            const bool temporaryExists =
                std::filesystem::exists(file.path().string() + ".tmp", error);
            require(!error && !temporaryExists,
                    "Temporary save artifact leaked or could not be inspected");
            error.clear();
            const bool backupExists = std::filesystem::exists(file.path().string() + ".bak", error);
            require(!error && !backupExists,
                    "Backup save artifact leaked or could not be inspected");
        }

        checksum = saturatingAdd(checksum, toUint64(config.saveFileCycles));
        const std::uint64_t operations =
            saturatingAdd(saturatingMultiply(config.saveCycles, 2u),
                          saturatingMultiply(config.saveFileCycles, 2u));
        return {operations, checksum};
    }

    std::string jsonEscape(std::string_view value)
    {
        std::ostringstream output;
        output << '"';

        for (const char rawCharacter : value)
        {
            const unsigned char character = static_cast<unsigned char>(rawCharacter);
            switch (character)
            {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
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
                if (character < 0x20u)
                {
                    output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
                }
                else
                {
                    output << static_cast<char>(character);
                }
                break;
            }
        }

        output << '"';
        return output.str();
    }

    bool writeJsonReport(const std::filesystem::path& path, const StressConfig& config,
                         const std::vector<ScenarioResult>& results)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return false;

        const bool passed = std::all_of(results.begin(), results.end(),
                                        [](const ScenarioResult& result) { return result.passed; });

        output << "{\n"
               << "  \"format_version\": 1,\n"
               << "  \"engine_version\": " << jsonEscape(l2d::VersionString) << ",\n"
               << "  \"profile\": " << jsonEscape(config.name) << ",\n"
               << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
               << "  \"scenarios\": [\n";

        for (std::size_t index = 0u; index < results.size(); ++index)
        {
            const ScenarioResult& result = results[index];
            output << "    {\"name\":" << jsonEscape(result.name)
                   << ",\"passed\":" << (result.passed ? "true" : "false")
                   << ",\"operations\":" << result.operations << ",\"checksum\":" << result.checksum
                   << ",\"milliseconds\":" << std::fixed << std::setprecision(3)
                   << result.milliseconds << ",\"failure\":" << jsonEscape(result.failure) << "}";
            if (index + 1u != results.size()) output << ',';
            output << '\n';
        }

        output << "  ]\n}\n";
        return static_cast<bool>(output);
    }

    void printUsage()
    {
        std::cout
            << "Lorenzo2D stress validation\n"
            << "Usage: Lorenzo2DStressValidation [--profile smoke|standard|soak]"
               " [--scenario NAME] [--json PATH]\n\n"
            << "Profiles are fixed and bounded; arbitrary workload sizes are intentionally not"
               " accepted.\n"
            << "Scenarios: tilemap-streaming, renderables-handles, physics-queries-contacts,\n"
            << "           navigation-replans, fixed-step-replay, asset-lifecycle, ui-churn,\n"
            << "           audio-voices, save-persistence\n";
    }
}

int main(int argc, char** argv)
{
    Options options;

    try
    {
        options = parseOptions(argc, argv);
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        printUsage();
        return 2;
    }

    if (options.help)
    {
        printUsage();
        return 0;
    }

    StressConfig config;
    try
    {
        config = stressConfig(options.profile);
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        printUsage();
        return 2;
    }

    if (!l2d::AudioSystem::useNullPlaybackDevice())
    {
        std::cerr << "Failed to activate the SFML null playback device.\n";
        return 2;
    }

    using Scenario =
        std::pair<std::string, std::function<std::pair<std::uint64_t, std::uint64_t>()>>;
    const std::vector<Scenario> scenarios = {
        {"tilemap-streaming", [&config]() { return stressTileMap(config); }},
        {"renderables-handles", [&config]() { return stressRenderables(config); }},
        {"physics-queries-contacts", [&config]() { return stressPhysics(config); }},
        {"navigation-replans", [&config]() { return stressNavigation(config); }},
        {"fixed-step-replay", [&config]() { return stressFixedStepReplay(config); }},
        {"asset-lifecycle", [&config]() { return stressAssets(config); }},
        {"ui-churn", [&config]() { return stressUi(config); }},
        {"audio-voices", [&config]() { return stressAudio(config); }},
        {"save-persistence", [&config]() { return stressSave(config); }},
    };

    if (options.scenario)
    {
        const bool known =
            std::any_of(scenarios.begin(), scenarios.end(), [&options](const Scenario& scenario)
                        { return scenario.first == *options.scenario; });
        if (!known)
        {
            std::cerr << "Unknown scenario: " << *options.scenario << '\n';
            printUsage();
            return 2;
        }
    }

    std::vector<ScenarioResult> results;
    results.reserve(scenarios.size());

    for (const Scenario& scenario : scenarios)
    {
        if (options.scenario && scenario.first != *options.scenario) continue;

        ScenarioResult result = runScenario(scenario.first, scenario.second);
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ") << result.name << "  " << std::fixed
                  << std::setprecision(3) << result.milliseconds
                  << " ms  operations=" << result.operations << " checksum=" << result.checksum;
        if (!result.passed) std::cout << "  " << result.failure;
        std::cout << '\n';
        results.push_back(std::move(result));
    }

    if (options.jsonPath && !writeJsonReport(*options.jsonPath, config, results))
    {
        std::cerr << "Failed to write stress report: " << options.jsonPath->string() << '\n';
        return 2;
    }

    const bool passed = std::all_of(results.begin(), results.end(),
                                    [](const ScenarioResult& result) { return result.passed; });

    std::cout << "Lorenzo2D " << l2d::VersionString << " stress profile " << config.name << ": "
              << (passed ? "PASS" : "FAIL") << " (" << results.size() << " scenario(s))\n";
    return passed ? 0 : 1;
}

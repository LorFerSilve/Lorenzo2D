#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct BenchmarkResult
    {
        double milliseconds = 0.0;
        std::size_t checksum = 0;
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

    void populatePhysicsScene(l2d::Scene& scene)
    {
        constexpr std::size_t width = 24;
        constexpr std::size_t height = 24;
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
        constexpr std::size_t width = 256;
        constexpr std::size_t height = 192;
        l2d::TileMap::Layout layout(height, std::string(width, '.'));

        for (std::size_t y = 0; y < height; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                if (y + 1 == height || (x % 19 == 0 && y % 5 != 0)) layout[y][x] = '#';
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

    void printResult(const std::string& name, std::size_t iterations, const BenchmarkResult& result)
    {
        std::cout << std::left << std::setw(30) << name << std::right << std::setw(10) << iterations
                  << " iterations  " << std::setw(10) << std::fixed << std::setprecision(3)
                  << result.milliseconds << " ms  checksum=" << result.checksum << '\n';
    }
}

int main()
{
    constexpr std::size_t physicsIterations = 30;
    constexpr std::size_t tileMapIterations = 5;

    printResult("physics uniform grid", physicsIterations,
                benchmarkPhysics(l2d::PhysicsBroadPhaseMode2D::UniformGrid, physicsIterations));
    printResult("physics brute force", physicsIterations,
                benchmarkPhysics(l2d::PhysicsBroadPhaseMode2D::BruteForce, physicsIterations));
    printResult("tile-map full build", tileMapIterations, benchmarkTileMapBuild(tileMapIterations));

    return 0;
}
